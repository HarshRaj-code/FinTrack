#include "CsvStorage.h"
#include "DateUtils.h"
#include "InputUtils.h"
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace fintrack {

CsvStorage::CsvStorage(const std::string& dataDir, const std::string& backupDir)
    : dataDir_(dataDir), backupDir_(backupDir) {}

std::string CsvStorage::getTransactionsPath() const {
    return (fs::path(dataDir_) / "transactions.csv").string();
}

std::string CsvStorage::getBudgetsPath() const {
    return (fs::path(dataDir_) / "budgets.csv").string();
}

std::string CsvStorage::getCategoriesPath() const {
    return (fs::path(dataDir_) / "categories.csv").string();
}

const std::string& CsvStorage::getDataDir() const { return dataDir_; }
const std::string& CsvStorage::getBackupDir() const { return backupDir_; }

bool CsvStorage::ensureDirectoriesExist(std::string& outError) const {
    std::error_code ec;
    fs::create_directories(dataDir_, ec);
    if (ec) {
        outError = "Failed to create data directory '" + dataDir_ + "': " + ec.message();
        return false;
    }
    fs::create_directories(backupDir_, ec);
    if (ec) {
        outError = "Failed to create backup directory '" + backupDir_ + "': " + ec.message();
        return false;
    }
    return true;
}

std::string CsvStorage::escapeCsvField(const std::string& field) {
    bool needsQuotes = false;
    for (char c : field) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) {
        return field;
    }
    std::string escaped = "\"";
    for (char c : field) {
        if (c == '"') {
            escaped += "\"\"";
        } else {
            escaped.push_back(c);
        }
    }
    escaped += "\"";
    return escaped;
}

bool CsvStorage::readCsvRecord(std::istream& in, std::vector<std::string>& outFields) {
    outFields.clear();
    if (in.eof()) {
        return false;
    }

    // Skip completely blank lines between records
    while (in.peek() == '\r' || in.peek() == '\n') {
        in.get();
    }
    if (in.eof() || !in.good()) {
        return false;
    }

    std::string currentField;
    bool inQuotes = false;

    while (true) {
        int ch = in.get();
        if (ch == std::char_traits<char>::eof()) {
            outFields.push_back(currentField);
            break;
        }
        char c = static_cast<char>(ch);
        if (inQuotes) {
            if (c == '"') {
                if (in.peek() == '"') {
                    in.get();
                    currentField.push_back('"');
                } else {
                    inQuotes = false;
                }
            } else {
                if (c != '\r') {
                    currentField.push_back(c);
                }
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == ',') {
                outFields.push_back(currentField);
                currentField.clear();
            } else if (c == '\r') {
                if (in.peek() == '\n') {
                    in.get();
                }
                outFields.push_back(currentField);
                break;
            } else if (c == '\n') {
                outFields.push_back(currentField);
                break;
            } else {
                currentField.push_back(c);
            }
        }
    }
    return !outFields.empty();
}

std::string CsvStorage::generateTimestampId() {
    std::time_t now = std::time(nullptr);
    std::tm localTm{};
#if defined(_WIN32)
    localtime_s(&localTm, &now);
#else
    localtime_r(&now, &localTm);
#endif
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (localTm.tm_year + 1900)
        << std::setw(2) << (localTm.tm_mon + 1)
        << std::setw(2) << localTm.tm_mday << "_"
        << std::setw(2) << localTm.tm_hour
        << std::setw(2) << localTm.tm_min
        << std::setw(2) << localTm.tm_sec;
    return oss.str();
}

void CsvStorage::preserveCorruptFileBackup(const std::string& filePath) const {
    std::error_code ec;
    if (!fs::exists(filePath, ec)) {
        return;
    }
    fs::create_directories(backupDir_, ec);
    std::string filename = fs::path(filePath).filename().string();
    fs::path backupPath = fs::path(backupDir_) / ("corrupt_" + generateTimestampId() + "_" + filename);
    fs::copy_file(filePath, backupPath, fs::copy_options::overwrite_existing, ec);
}

bool CsvStorage::writeContentSafely(const std::string& targetPath,
                                    const std::string& content,
                                    std::string& outError) const {
    if (!ensureDirectoriesExist(outError)) {
        return false;
    }
    std::string tempPath = targetPath + ".tmp";
    {
        std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            outError = "Cannot open temporary file for writing: " + tempPath;
            return false;
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.flush();
        if (!out.good()) {
            out.close();
            std::error_code ec;
            fs::remove(tempPath, ec);
            outError = "Failed while writing data to temporary file: " + tempPath;
            return false;
        }
    }

    std::error_code ec;
    fs::rename(tempPath, targetPath, ec);
    if (ec) {
        // On Windows, rename can fail if targetPath already exists; copy and remove safely
        ec.clear();
        fs::copy_file(tempPath, targetPath, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            outError = "Failed to replace target file '" + targetPath + "': " + ec.message();
            fs::remove(tempPath, ec);
            return false;
        }
        fs::remove(tempPath, ec);
    }
    return true;
}

LoadReport CsvStorage::loadCategories(std::vector<Category>& ioCategories) const {
    LoadReport report;
    ioCategories = Category::getDefaultCategories();

    std::string path = getCategoriesPath();
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        return report;
    }
    report.fileExisted = true;

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        report.warnings.push_back("Could not open categories.csv for reading.");
        return report;
    }

    std::vector<std::string> fields;
    int rowNum = 0;
    while (readCsvRecord(in, fields)) {
        ++rowNum;
        if (rowNum == 1 && fields.size() >= 2 &&
            InputUtils::equalsIgnoreCase(fields[0], "Name")) {
            continue; // Header row
        }
        if (fields.size() != 2) {
            report.skippedRecords++;
            report.hadCorruptRows = true;
            report.warnings.push_back("categories.csv row " + std::to_string(rowNum) + ": expected 2 fields.");
            continue;
        }
        std::string name = InputUtils::trim(fields[0]);
        TransactionType type;
        if (name.empty() || !Transaction::stringToType(fields[1], type)) {
            report.skippedRecords++;
            report.hadCorruptRows = true;
            report.warnings.push_back("categories.csv row " + std::to_string(rowNum) + ": invalid name or type.");
            continue;
        }

        bool duplicate = false;
        for (const auto& existing : ioCategories) {
            if (InputUtils::equalsIgnoreCase(existing.getName(), name)) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            ioCategories.emplace_back(name, type, false);
            report.validRecordsLoaded++;
        }
    }

    if (report.hadCorruptRows) {
        preserveCorruptFileBackup(path);
    }
    return report;
}

LoadReport CsvStorage::loadTransactions(const std::vector<Category>& categories,
                                        std::vector<Transaction>& outTransactions,
                                        int& outNextId) const {
    LoadReport report;
    outTransactions.clear();
    int maxId = 0;

    std::string path = getTransactionsPath();
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        outNextId = std::max(outNextId, 1);
        return report;
    }
    report.fileExisted = true;

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        report.warnings.push_back("Could not open transactions.csv for reading.");
        outNextId = std::max(outNextId, 1);
        return report;
    }

    std::set<int> seenIds;
    std::vector<std::string> fields;
    int rowNum = 0;

    while (readCsvRecord(in, fields)) {
        ++rowNum;
        if (rowNum == 1 && !fields.empty() &&
            InputUtils::equalsIgnoreCase(fields[0], "ID")) {
            continue; // Header row
        }
        if (fields.size() != 7) {
            report.skippedRecords++;
            report.hadCorruptRows = true;
            report.warnings.push_back("transactions.csv row " + std::to_string(rowNum) + ": expected 7 columns.");
            continue;
        }

        int id = 0;
        if (!InputUtils::parsePositiveInt(fields[0], id)) {
            report.skippedRecords++;
            report.hadCorruptRows = true;
            report.warnings.push_back("transactions.csv row " + std::to_string(rowNum) + ": invalid ID.");
            continue;
        }
        if (seenIds.count(id) > 0) {
            report.skippedRecords++;
            report.hadCorruptRows = true;
            report.warnings.push_back("transactions.csv row " + std::to_string(rowNum) + ": duplicate ID " + std::to_string(id) + ".");
            continue;
        }

        std::string date = InputUtils::trim(fields[1]);
        TransactionType type;
        if (!DateUtils::isValidDate(date) || !Transaction::stringToType(fields[2], type)) {
            report.skippedRecords++;
            report.hadCorruptRows = true;
            report.warnings.push_back("transactions.csv row " + std::to_string(rowNum) + ": invalid date or type.");
            continue;
        }

        std::string paiseStr = InputUtils::trim(fields[3]);
        bool validPaiseDigits = !paiseStr.empty() && paiseStr.size() <= 14;
        for (char c : paiseStr) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                validPaiseDigits = false;
                break;
            }
        }
        if (!validPaiseDigits) {
            report.skippedRecords++;
            report.hadCorruptRows = true;
            report.warnings.push_back("transactions.csv row " + std::to_string(rowNum) + ": invalid integer paise.");
            continue;
        }
        std::int64_t amountPaise = std::stoll(paiseStr);
        if (amountPaise <= 0) {
            report.skippedRecords++;
            report.hadCorruptRows = true;
            report.warnings.push_back("transactions.csv row " + std::to_string(rowNum) + ": non-positive amount.");
            continue;
        }

        std::string categoryName = InputUtils::trim(fields[4]);
        std::string paymentMethod = InputUtils::trim(fields[5]);
        std::string description = InputUtils::trim(fields[6]);

        // Match canonical category name and verify transaction type compatibility
        std::string matchedCategory;
        for (const auto& cat : categories) {
            if (cat.getType() == type && InputUtils::equalsIgnoreCase(cat.getName(), categoryName)) {
                matchedCategory = cat.getName();
                break;
            }
        }
        if (matchedCategory.empty() || !Category::isValidPaymentMethod(paymentMethod)) {
            report.skippedRecords++;
            report.hadCorruptRows = true;
            report.warnings.push_back("transactions.csv row " + std::to_string(rowNum) +
                                      ": unknown/mismatched category or payment method.");
            continue;
        }

        try {
            outTransactions.emplace_back(id, date, type, amountPaise, matchedCategory, paymentMethod, description);
            seenIds.insert(id);
            maxId = std::max(maxId, id);
            report.validRecordsLoaded++;
        } catch (const std::exception& ex) {
            report.skippedRecords++;
            report.hadCorruptRows = true;
            report.warnings.push_back("transactions.csv row " + std::to_string(rowNum) + ": " + ex.what());
        }
    }

    outNextId = std::max(outNextId, maxId + 1);
    if (report.hadCorruptRows) {
        preserveCorruptFileBackup(path);
    }
    return report;
}

LoadReport CsvStorage::loadBudgets(const std::vector<Category>& categories,
                                   std::vector<Budget>& outBudgets) const {
    LoadReport report;
    outBudgets.clear();

    std::string path = getBudgetsPath();
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        return report;
    }
    report.fileExisted = true;

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        report.warnings.push_back("Could not open budgets.csv for reading.");
        return report;
    }

    std::vector<std::string> fields;
    int rowNum = 0;
    while (readCsvRecord(in, fields)) {
        ++rowNum;
        if (rowNum == 1 && !fields.empty() &&
            InputUtils::equalsIgnoreCase(fields[0], "Month")) {
            continue; // Header row
        }
        if (fields.size() != 3) {
            report.skippedRecords++;
            report.hadCorruptRows = true;
            report.warnings.push_back("budgets.csv row " + std::to_string(rowNum) + ": expected 3 columns.");
            continue;
        }

        std::string month = InputUtils::trim(fields[0]);
        std::string categoryName = InputUtils::trim(fields[1]);
        std::string paiseStr = InputUtils::trim(fields[2]);

        bool validDigits = !paiseStr.empty() && paiseStr.size() <= 14;
        for (char c : paiseStr) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                validDigits = false;
                break;
            }
        }
        if (!DateUtils::isValidMonth(month) || !validDigits) {
            report.skippedRecords++;
            report.hadCorruptRows = true;
            report.warnings.push_back("budgets.csv row " + std::to_string(rowNum) + ": invalid month or limit.");
            continue;
        }

        std::int64_t limitPaise = std::stoll(paiseStr);
        std::string matchedExpenseCat;
        for (const auto& cat : categories) {
            if (cat.getType() == TransactionType::Expense &&
                InputUtils::equalsIgnoreCase(cat.getName(), categoryName)) {
                matchedExpenseCat = cat.getName();
                break;
            }
        }
        if (matchedExpenseCat.empty() || limitPaise <= 0) {
            report.skippedRecords++;
            report.hadCorruptRows = true;
            report.warnings.push_back("budgets.csv row " + std::to_string(rowNum) +
                                      ": category is not a valid Expense category or limit <= 0.");
            continue;
        }

        bool duplicate = false;
        for (const auto& b : outBudgets) {
            if (b.getMonth() == month && InputUtils::equalsIgnoreCase(b.getCategory(), matchedExpenseCat)) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            report.skippedRecords++;
            report.hadCorruptRows = true;
            report.warnings.push_back("budgets.csv row " + std::to_string(rowNum) + ": duplicate budget entry.");
            continue;
        }

        outBudgets.emplace_back(month, matchedExpenseCat, limitPaise);
        report.validRecordsLoaded++;
    }

    if (report.hadCorruptRows) {
        preserveCorruptFileBackup(path);
    }
    return report;
}

bool CsvStorage::saveCategories(const std::vector<Category>& categories, std::string& outError) const {
    std::ostringstream oss;
    oss << "Name,Type\n";
    for (const auto& cat : categories) {
        if (!cat.isBuiltIn()) {
            oss << escapeCsvField(cat.getName()) << ","
                << Transaction::typeToString(cat.getType()) << "\n";
        }
    }
    return writeContentSafely(getCategoriesPath(), oss.str(), outError);
}

bool CsvStorage::saveTransactions(const std::vector<Transaction>& transactions, std::string& outError) const {
    std::ostringstream oss;
    // Amounts are stored in integer paise (1 INR = 100 paise)
    oss << "ID,Date,Type,AmountPaise,Category,PaymentMethod,Description\n";
    for (const auto& tx : transactions) {
        oss << tx.getId() << ","
            << escapeCsvField(tx.getDate()) << ","
            << Transaction::typeToString(tx.getType()) << ","
            << tx.getAmountPaise() << ","
            << escapeCsvField(tx.getCategory()) << ","
            << escapeCsvField(tx.getPaymentMethod()) << ","
            << escapeCsvField(tx.getDescription()) << "\n";
    }
    return writeContentSafely(getTransactionsPath(), oss.str(), outError);
}

bool CsvStorage::saveBudgets(const std::vector<Budget>& budgets, std::string& outError) const {
    std::ostringstream oss;
    oss << "Month,Category,LimitPaise\n";
    for (const auto& b : budgets) {
        oss << escapeCsvField(b.getMonth()) << ","
            << escapeCsvField(b.getCategory()) << ","
            << b.getLimitPaise() << "\n";
    }
    return writeContentSafely(getBudgetsPath(), oss.str(), outError);
}

bool CsvStorage::isInternalDatabaseFile(const std::string& filePath) const {
    std::error_code ec;
    fs::path candidate = fs::weakly_canonical(fs::path(filePath), ec);
    fs::path txPath = fs::weakly_canonical(fs::path(getTransactionsPath()), ec);
    fs::path bgPath = fs::weakly_canonical(fs::path(getBudgetsPath()), ec);
    fs::path catPath = fs::weakly_canonical(fs::path(getCategoriesPath()), ec);
    return (candidate == txPath || candidate == bgPath || candidate == catPath);
}

bool CsvStorage::exportTransactionsToCsv(const std::vector<Transaction>& transactions,
                                         const std::string& filePath,
                                         std::string& outError) const {
    if (isInternalDatabaseFile(filePath)) {
        outError = "Export path cannot overwrite internal application database files.";
        return false;
    }

    std::ostringstream oss;
    oss << "ID,Date,Type,Category,PaymentMethod,AmountINR,Description\n";
    for (const auto& tx : transactions) {
        oss << tx.getId() << ","
            << escapeCsvField(tx.getDate()) << ","
            << Transaction::typeToString(tx.getType()) << ","
            << escapeCsvField(tx.getCategory()) << ","
            << escapeCsvField(tx.getPaymentMethod()) << ","
            << InputUtils::formatPaisePlain(tx.getAmountPaise()) << ","
            << escapeCsvField(tx.getDescription()) << "\n";
    }

    fs::path target(filePath);
    if (target.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(target.parent_path(), ec);
    }
    return writeContentSafely(filePath, oss.str(), outError);
}

bool CsvStorage::createBackup(const std::vector<Category>& categories,
                              const std::vector<Transaction>& transactions,
                              const std::vector<Budget>& budgets,
                              std::string& outBackupId,
                              std::string& outError) const {
    if (!ensureDirectoriesExist(outError)) {
        return false;
    }
    std::string baseId = generateTimestampId();
    std::string finalId = baseId;
    int suffix = 1;
    std::error_code ec;
    while (fs::exists(fs::path(backupDir_) / ("backup_" + finalId), ec)) {
        finalId = baseId + "_" + std::to_string(suffix++);
    }

    fs::path snapshotDir = fs::path(backupDir_) / ("backup_" + finalId);
    fs::create_directories(snapshotDir, ec);
    if (ec) {
        outError = "Could not create backup folder: " + ec.message();
        return false;
    }

    CsvStorage snapshotStorage(snapshotDir.string(), backupDir_);
    if (!snapshotStorage.saveCategories(categories, outError) ||
        !snapshotStorage.saveTransactions(transactions, outError) ||
        !snapshotStorage.saveBudgets(budgets, outError)) {
        return false;
    }

    outBackupId = finalId;
    return true;
}

std::vector<BackupInfo> CsvStorage::listBackups() const {
    std::vector<BackupInfo> list;
    std::error_code ec;
    if (!fs::exists(backupDir_, ec)) {
        return list;
    }
    for (const auto& entry : fs::directory_iterator(backupDir_, ec)) {
        if (entry.is_directory()) {
            std::string folderName = entry.path().filename().string();
            if (folderName.rfind("backup_", 0) == 0 && folderName.size() > 7) {
                BackupInfo info;
                info.id = folderName.substr(7);
                info.directory = entry.path().string();
                list.push_back(info);
            }
        }
    }
    std::sort(list.begin(), list.end(), [](const BackupInfo& a, const BackupInfo& b) {
        return a.id > b.id; // Newest first
    });
    return list;
}

bool CsvStorage::restoreBackup(const std::string& backupId,
                               std::vector<Category>& outCategories,
                               std::vector<Transaction>& outTransactions,
                               std::vector<Budget>& outBudgets,
                               int& outNextId,
                               std::string& outError) const {
    fs::path snapshotDir = fs::path(backupDir_) / ("backup_" + backupId);
    std::error_code ec;
    if (!fs::exists(snapshotDir, ec) || !fs::is_directory(snapshotDir, ec)) {
        outError = "Backup '" + backupId + "' does not exist.";
        return false;
    }

    CsvStorage snapshotStorage(snapshotDir.string(), backupDir_);
    if (!fs::exists(snapshotStorage.getTransactionsPath(), ec) ||
        !fs::exists(snapshotStorage.getBudgetsPath(), ec) ||
        !fs::exists(snapshotStorage.getCategoriesPath(), ec)) {
        outError = "Backup '" + backupId + "' is incomplete (missing required CSV files).";
        return false;
    }

    std::vector<Category> tempCategories;
    std::vector<Transaction> tempTransactions;
    std::vector<Budget> tempBudgets;
    int tempNextId = outNextId;

    LoadReport catRep = snapshotStorage.loadCategories(tempCategories);
    LoadReport txRep = snapshotStorage.loadTransactions(tempCategories, tempTransactions, tempNextId);
    LoadReport bgRep = snapshotStorage.loadBudgets(tempCategories, tempBudgets);

    if (catRep.hadCorruptRows || txRep.hadCorruptRows || bgRep.hadCorruptRows) {
        outError = "Backup '" + backupId + "' contains malformed or corrupted records and was rejected.";
        return false;
    }

    // Persist validated backup data to active data directory
    if (!saveCategories(tempCategories, outError) ||
        !saveTransactions(tempTransactions, outError) ||
        !saveBudgets(tempBudgets, outError)) {
        return false;
    }

    outCategories = std::move(tempCategories);
    outTransactions = std::move(tempTransactions);
    outBudgets = std::move(tempBudgets);
    outNextId = std::max(outNextId, tempNextId);
    return true;
}

} // namespace fintrack