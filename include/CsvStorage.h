#ifndef FINTRACK_CSV_STORAGE_H
#define FINTRACK_CSV_STORAGE_H

#include "Budget.h"
#include "Category.h"
#include "Transaction.h"
#include <istream>
#include <string>
#include <vector>

namespace fintrack {

struct LoadReport {
    bool fileExisted = false;
    bool hadCorruptRows = false;
    int validRecordsLoaded = 0;
    int skippedRecords = 0;
    std::vector<std::string> warnings;
};

struct BackupInfo {
    std::string id;        // Timestamp string YYYYMMDD_HHMMSS
    std::string directory; // Backup directory path
};

class CsvStorage {
public:
    explicit CsvStorage(const std::string& dataDir = "data",
                        const std::string& backupDir = "backups");

    bool ensureDirectoriesExist(std::string& outError) const;

    // RFC-4180 CSV helpers (exposed for unit tests)
    static std::string escapeCsvField(const std::string& field);
    static bool readCsvRecord(std::istream& in, std::vector<std::string>& outFields);

    // Load methods
    LoadReport loadCategories(std::vector<Category>& ioCategories) const;
    LoadReport loadTransactions(const std::vector<Category>& categories,
                                std::vector<Transaction>& outTransactions,
                                int& outNextId) const;
    LoadReport loadBudgets(const std::vector<Category>& categories,
                           std::vector<Budget>& outBudgets) const;

    // Save methods (use temporary file + safe replacement)
    bool saveCategories(const std::vector<Category>& categories, std::string& outError) const;
    bool saveTransactions(const std::vector<Transaction>& transactions, std::string& outError) const;
    bool saveBudgets(const std::vector<Budget>& budgets, std::string& outError) const;

    // Export human-readable CSV
    bool exportTransactionsToCsv(const std::vector<Transaction>& transactions,
                                 const std::string& filePath,
                                 std::string& outError) const;

    // Checks if a path points to one of the active internal storage files
    bool isInternalDatabaseFile(const std::string& filePath) const;

    // Backup and Restore
    bool createBackup(const std::vector<Category>& categories,
                      const std::vector<Transaction>& transactions,
                      const std::vector<Budget>& budgets,
                      std::string& outBackupId,
                      std::string& outError) const;

    std::vector<BackupInfo> listBackups() const;

    bool restoreBackup(const std::string& backupId,
                       std::vector<Category>& outCategories,
                       std::vector<Transaction>& outTransactions,
                       std::vector<Budget>& outBudgets,
                       int& outNextId,
                       std::string& outError) const;

    std::string getTransactionsPath() const;
    std::string getBudgetsPath() const;
    std::string getCategoriesPath() const;
    const std::string& getDataDir() const;
    const std::string& getBackupDir() const;

private:
    std::string dataDir_;
    std::string backupDir_;

    bool writeContentSafely(const std::string& targetPath,
                            const std::string& content,
                            std::string& outError) const;
    void preserveCorruptFileBackup(const std::string& filePath) const;
    static std::string generateTimestampId();
};

} // namespace fintrack

#endif // FINTRACK_CSV_STORAGE_H