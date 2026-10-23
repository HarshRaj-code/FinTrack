#include "Budget.h"
#include "Category.h"
#include "CsvStorage.h"
#include "DateUtils.h"
#include "FinanceManager.h"
#include "InputUtils.h"
#include "Transaction.h"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace fintrack;

namespace {

void printHeader(const std::string& title) {
    std::cout << "\n================================================================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "================================================================================\n";
}

void printDivider() {
    std::cout << "--------------------------------------------------------------------------------\n";
}

std::string truncateText(const std::string& text, std::size_t maxLen) {
    if (text.size() <= maxLen) {
        return text;
    }
    return text.substr(0, maxLen - 3) + "...";
}

void printTransactionRowHeader() {
    std::cout << std::left
              << std::setw(5)  << "ID"
              << std::setw(12) << "Date"
              << std::setw(9)  << "Type"
              << std::setw(18) << "Category"
              << std::setw(15) << "Method"
              << std::setw(14) << "Amount"
              << "Description\n";
    printDivider();
}

void printTransactionRow(const Transaction& tx) {
    std::cout << std::left
              << std::setw(5)  << tx.getId()
              << std::setw(12) << tx.getDate()
              << std::setw(9)  << Transaction::typeToString(tx.getType())
              << std::setw(18) << truncateText(tx.getCategory(), 16)
              << std::setw(15) << truncateText(tx.getPaymentMethod(), 13)
              << std::setw(14) << InputUtils::formatPaise(tx.getAmountPaise())
              << truncateText(tx.getDescription(), 22) << "\n";
}

void displayPaginatedTransactions(const std::vector<Transaction>& list, const std::string& title) {
    printHeader(title + " (" + std::to_string(list.size()) + " record(s))");
    if (list.empty()) {
        std::cout << "  No transactions match the current view.\n";
        return;
    }

    constexpr std::size_t PAGE_SIZE = 10;
    std::size_t totalPages = (list.size() + PAGE_SIZE - 1) / PAGE_SIZE;
    std::size_t currentPage = 0;

    while (true) {
        std::size_t startIdx = currentPage * PAGE_SIZE;
        std::size_t endIdx = std::min(startIdx + PAGE_SIZE, list.size());

        std::cout << "\nPage " << (currentPage + 1) << " of " << totalPages
                  << " (Showing records " << (startIdx + 1) << "-" << endIdx << ")\n";
        printTransactionRowHeader();
        for (std::size_t i = startIdx; i < endIdx; ++i) {
            printTransactionRow(list[i]);
        }
        printDivider();

        if (totalPages == 1) {
            break;
        }

        std::cout << "Navigation: [N]ext Page | [P]revious Page | [R]eturn to Menu\n";
        std::string nav = InputUtils::toLower(InputUtils::readLine("Select action (N/P/R): "));
        if (nav == "n" || nav == "next") {
            if (currentPage + 1 < totalPages) {
                ++currentPage;
            } else {
                std::cout << "  [i] Already on the last page.\n";
            }
        } else if (nav == "p" || nav == "prev" || nav == "previous") {
            if (currentPage > 0) {
                --currentPage;
            } else {
                std::cout << "  [i] Already on the first page.\n";
            }
        } else if (nav == "r" || nav == "return" || nav.empty()) {
            break;
        } else {
            std::cout << "  [!] Please enter N, P, or R.\n";
        }
    }
}

void displayDashboard(const FinanceManager& manager) {
    DashboardData dash = manager.getDashboardData();
    printHeader("FINTRACK DASHBOARD | Date: " + dash.currentDate + " | Month: " + dash.currentMonth);

    std::cout << std::left << std::setw(34) << "  Monthly Income:"
              << InputUtils::formatPaise(dash.monthlyIncomePaise) << "\n";
    std::cout << std::left << std::setw(34) << "  Monthly Expenses:"
              << InputUtils::formatPaise(dash.monthlyExpensePaise) << "\n";
    std::cout << std::left << std::setw(34) << "  Net Cash Flow (Income - Exp):"
              << InputUtils::formatPaise(dash.netCashFlowPaise) << "\n";
    std::cout << std::left << std::setw(34) << "  Today's Expenses (" + dash.currentDate + "):"
              << InputUtils::formatPaise(dash.todayExpensePaise) << "\n";

    if (dash.hasMonthlyBudgets) {
        std::cout << std::left << std::setw(34) << "  Remaining Monthly Budget:"
                  << InputUtils::formatPaise(dash.remainingTotalBudgetPaise)
                  << " (of " << InputUtils::formatPaise(dash.totalMonthlyBudgetLimitPaise) << " budgeted)\n";
    } else {
        std::cout << std::left << std::setw(34) << "  Remaining Monthly Budget:"
                  << "No budgets configured for " << dash.currentMonth << "\n";
    }

    std::cout << std::left << std::setw(34) << "  Transactions This Month:"
              << dash.monthlyTransactionCount << "\n";

    printDivider();
    std::cout << "  RECENT 5 TRANSACTIONS:\n";
    if (dash.recentTransactions.empty()) {
        std::cout << "  No transactions recorded yet.\n";
    } else {
        printTransactionRowHeader();
        for (const auto& tx : dash.recentTransactions) {
            printTransactionRow(tx);
        }
    }

    printDivider();
    std::cout << "  BUDGET ALERTS (" << dash.currentMonth << "):\n";
    if (dash.budgetAlerts.empty()) {
        std::cout << "  All budgeted categories are on track.\n";
    } else {
        for (const auto& alert : dash.budgetAlerts) {
            std::cout << "  [ALERT] " << alert.category << " -> " << alert.statusLabel
                      << " | Spent: " << InputUtils::formatPaise(alert.spentPaise)
                      << " / Limit: " << InputUtils::formatPaise(alert.limitPaise)
                      << " (" << std::fixed << std::setprecision(1) << alert.percentageUsed << "%)\n";
        }
    }
}

std::optional<std::string> selectCategoryInteractive(const FinanceManager& manager, TransactionType type) {
    auto cats = manager.getCategoriesByType(type);
    std::cout << "\nAvailable " << Transaction::typeToString(type) << " Categories:\n";
    for (std::size_t i = 0; i < cats.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << cats[i].getName()
                  << (cats[i].isBuiltIn() ? "" : " [Custom]") << "\n";
    }
    std::cout << "  0. Cancel\n";
    int choice = InputUtils::readMenuChoice("Select category number: ", 0, static_cast<int>(cats.size()));
    if (choice == 0) {
        return std::nullopt;
    }
    return cats[static_cast<std::size_t>(choice - 1)].getName();
}

std::optional<std::string> selectPaymentMethodInteractive() {
    auto methods = Category::getDefaultPaymentMethods();
    std::cout << "\nAvailable Payment Methods:\n";
    for (std::size_t i = 0; i < methods.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << methods[i] << "\n";
    }
    std::cout << "  0. Cancel\n";
    int choice = InputUtils::readMenuChoice("Select payment method number: ", 0, static_cast<int>(methods.size()));
    if (choice == 0) {
        return std::nullopt;
    }
    return methods[static_cast<std::size_t>(choice - 1)];
}

void showBudgetAlertForExpense(const FinanceManager& manager, const std::string& date, const std::string& category) {
    std::string month = DateUtils::extractMonth(date);
    auto evalOpt = manager.evaluateCategoryBudget(month, category);
    if (!evalOpt.has_value()) {
        return;
    }
    const auto& ev = *evalOpt;
    std::cout << "\n  [Budget Status: " << ev.category << " (" << ev.month << ")]\n"
              << "  Limit: " << InputUtils::formatPaise(ev.limitPaise)
              << " | Spent: " << InputUtils::formatPaise(ev.spentPaise)
              << " | Remaining: " << InputUtils::formatPaise(ev.remainingPaise)
              << " (" << std::fixed << std::setprecision(1) << ev.percentageUsed << "% - "
              << ev.statusLabel << ")\n";
    if (ev.state != BudgetState::OnTrack) {
        std::cout << "  [!] WARNING: Category '" << ev.category << "' is at status: " << ev.statusLabel << "!\n";
    }
}

void handleAddTransaction(FinanceManager& manager) {
    printHeader("ADD NEW TRANSACTION");
    std::cout << "  1. Expense\n  2. Income\n  0. Cancel\n";
    int typeChoice = InputUtils::readMenuChoice("Select transaction type (0-2): ", 0, 2);
    if (typeChoice == 0) {
        std::cout << "  [i] Add transaction cancelled.\n";
        return;
    }
    TransactionType type = (typeChoice == 2) ? TransactionType::Income : TransactionType::Expense;

    std::string date;
    while (true) {
        std::string input = InputUtils::readLine("Enter date YYYY-MM-DD (leave blank for today " +
                                                 DateUtils::getCurrentDate() + ", or 'c' to cancel): ");
        if (InputUtils::equalsIgnoreCase(input, "c")) {
            std::cout << "  [i] Cancelled.\n";
            return;
        }
        if (input.empty()) {
            date = DateUtils::getCurrentDate();
            break;
        }
        if (DateUtils::isValidDate(input)) {
            date = input;
            break;
        }
        std::cout << "  [!] Invalid calendar date. Please use YYYY-MM-DD (e.g., 2026-10-23).\n";
    }

    std::int64_t amountPaise = 0;
    while (true) {
        std::string input = InputUtils::readLine("Enter amount in INR (e.g., 250 or 499.50, 'c' to cancel): ");
        if (InputUtils::equalsIgnoreCase(input, "c")) {
            std::cout << "  [i] Cancelled.\n";
            return;
        }
        if (InputUtils::parseAmountToPaise(input, amountPaise)) {
            break;
        }
        std::cout << "  [!] Invalid amount. Enter a positive number with up to 2 decimal places.\n";
    }

    auto catOpt = selectCategoryInteractive(manager, type);
    if (!catOpt.has_value()) {
        std::cout << "  [i] Cancelled.\n";
        return;
    }

    auto pmOpt = selectPaymentMethodInteractive();
    if (!pmOpt.has_value()) {
        std::cout << "  [i] Cancelled.\n";
        return;
    }

    std::string description = InputUtils::readLine("Enter optional description/note (or press Enter to skip): ");

    printDivider();
    std::cout << "  Confirm New Transaction:\n"
              << "    Type:           " << Transaction::typeToString(type) << "\n"
              << "    Date:           " << date << "\n"
              << "    Amount:         " << InputUtils::formatPaise(amountPaise) << "\n"
              << "    Category:       " << *catOpt << "\n"
              << "    Payment Method: " << *pmOpt << "\n"
              << "    Description:    " << (description.empty() ? "(none)" : description) << "\n";

    if (!InputUtils::readConfirmation("Save this transaction?")) {
        std::cout << "  [i] Transaction discarded.\n";
        return;
    }

    Transaction created;
    std::string error;
    if (manager.addTransaction(date, type, amountPaise, *catOpt, *pmOpt, description, created, error)) {
        std::cout << "  [OK] Transaction saved! ID: " << created.getId()
                  << " | " << Transaction::typeToString(created.getType())
                  << " | " << InputUtils::formatPaise(created.getAmountPaise())
                  << " | " << created.getCategory()
                  << " | " << created.getDate() << "\n";
        if (created.getType() == TransactionType::Expense) {
            showBudgetAlertForExpense(manager, created.getDate(), created.getCategory());
        }
    } else {
        std::cout << "  [ERROR] Could not save transaction: " << error << "\n";
    }
}

void handleEditTransaction(FinanceManager& manager) {
    printHeader("EDIT TRANSACTION");
    std::string idStr = InputUtils::readLine("Enter Transaction ID to edit (or 'c' to cancel): ");
    if (InputUtils::equalsIgnoreCase(idStr, "c") || idStr.empty()) {
        return;
    }
    int id = 0;
    if (!InputUtils::parsePositiveInt(idStr, id)) {
        std::cout << "  [!] Invalid Transaction ID.\n";
        return;
    }

    const Transaction* existing = manager.findTransactionById(id);
    if (!existing) {
        std::cout << "  [!] Transaction ID " << id << " does not exist.\n";
        return;
    }

    std::cout << "\nCurrent Transaction Record:\n";
    printTransactionRowHeader();
    printTransactionRow(*existing);
    printDivider();
    std::cout << "Leave any prompt blank to keep the existing value, or type 'c' to cancel.\n";

    TransactionType newType = existing->getType();
    while (true) {
        std::string in = InputUtils::readLine("Type [Current: " + Transaction::typeToString(newType) +
                                              "] (1=Expense, 2=Income, Enter=keep, c=cancel): ");
        if (InputUtils::equalsIgnoreCase(in, "c")) return;
        if (in.empty()) break;
        if (in == "1") { newType = TransactionType::Expense; break; }
        if (in == "2") { newType = TransactionType::Income; break; }
        std::cout << "  [!] Enter 1, 2, or press Enter.\n";
    }

    std::string newDate = existing->getDate();
    while (true) {
        std::string in = InputUtils::readLine("Date [Current: " + newDate + "] (YYYY-MM-DD, Enter=keep, c=cancel): ");
        if (InputUtils::equalsIgnoreCase(in, "c")) return;
        if (in.empty()) break;
        if (DateUtils::isValidDate(in)) { newDate = in; break; }
        std::cout << "  [!] Invalid date format or non-existent calendar date.\n";
    }

    std::int64_t newAmount = existing->getAmountPaise();
    while (true) {
        std::string in = InputUtils::readLine("Amount [Current: " + InputUtils::formatPaise(newAmount) +
                                              "] (INR, Enter=keep, c=cancel): ");
        if (InputUtils::equalsIgnoreCase(in, "c")) return;
        if (in.empty()) break;
        if (InputUtils::parseAmountToPaise(in, newAmount)) break;
        std::cout << "  [!] Invalid positive monetary amount.\n";
    }

    std::string newCategory = existing->getCategory();
    std::string canonicalCheck;
    bool typeChanged = (newType != existing->getType());
    if (typeChanged || !manager.isValidCategoryForType(newCategory, newType, canonicalCheck)) {
        std::cout << "  [i] Transaction type changed; please select a valid "
                  << Transaction::typeToString(newType) << " category:\n";
        auto catOpt = selectCategoryInteractive(manager, newType);
        if (!catOpt.has_value()) return;
        newCategory = *catOpt;
    } else {
        std::string changeCat = InputUtils::readLine("Category [Current: " + newCategory +
                                                     "] (Press Enter to keep, or 'y' to change): ");
        if (InputUtils::equalsIgnoreCase(changeCat, "y")) {
            auto catOpt = selectCategoryInteractive(manager, newType);
            if (!catOpt.has_value()) return;
            newCategory = *catOpt;
        }
    }

    std::string newPaymentMethod = existing->getPaymentMethod();
    std::string changePm = InputUtils::readLine("Payment Method [Current: " + newPaymentMethod +
                                                "] (Press Enter to keep, or 'y' to change): ");
    if (InputUtils::equalsIgnoreCase(changePm, "y")) {
        auto pmOpt = selectPaymentMethodInteractive();
        if (!pmOpt.has_value()) return;
        newPaymentMethod = *pmOpt;
    }

    std::string newDescription = existing->getDescription();
    std::string descIn = InputUtils::readLine("Description [Current: " +
                                              (newDescription.empty() ? "(none)" : newDescription) +
                                              "] (Enter=keep, '-'=clear, or type new): ");
    if (descIn == "-") {
        newDescription.clear();
    } else if (!descIn.empty()) {
        newDescription = descIn;
    }

    if (!InputUtils::readConfirmation("Apply changes to Transaction #" + std::to_string(id) + "?")) {
        std::cout << "  [i] Edit cancelled. Original transaction preserved.\n";
        return;
    }

    Transaction updated;
    std::string error;
    if (manager.editTransaction(id, newDate, newType, newAmount, newCategory, newPaymentMethod, newDescription, updated, error)) {
        std::cout << "  [OK] Transaction #" << updated.getId() << " updated and persisted.\n";
        if (updated.getType() == TransactionType::Expense) {
            showBudgetAlertForExpense(manager, updated.getDate(), updated.getCategory());
        }
    } else {
        std::cout << "  [ERROR] Failed to update transaction: " << error << "\n";
    }
}

void handleDeleteTransaction(FinanceManager& manager) {
    printHeader("DELETE TRANSACTION");
    std::string idStr = InputUtils::readLine("Enter Transaction ID to delete (or 'c' to cancel): ");
    if (InputUtils::equalsIgnoreCase(idStr, "c") || idStr.empty()) {
        return;
    }
    int id = 0;
    if (!InputUtils::parsePositiveInt(idStr, id)) {
        std::cout << "  [!] Invalid Transaction ID.\n";
        return;
    }

    const Transaction* existing = manager.findTransactionById(id);
    if (!existing) {
        std::cout << "  [!] Transaction ID " << id << " not found.\n";
        return;
    }

    std::cout << "\nSelected Transaction for Deletion:\n";
    printTransactionRowHeader();
    printTransactionRow(*existing);
    printDivider();

    if (!InputUtils::readConfirmation("Are you sure you want to permanently delete Transaction #" + std::to_string(id) + "?")) {
        std::cout << "  [i] Deletion cancelled.\n";
        return;
    }

    std::string error;
    if (manager.deleteTransaction(id, error)) {
        std::cout << "  [OK] Transaction #" << id << " deleted and storage updated.\n";
    } else {
        std::cout << "  [ERROR] Failed to delete transaction: " << error << "\n";
    }
}

void handleExportFlow(const FinanceManager& manager, const std::vector<Transaction>& records, const std::string& label) {
    printHeader("EXPORT TRANSACTIONS TO CSV (" + label + ")");
    if (records.empty()) {
        std::cout << "  [!] There are no transactions in the current selection to export.\n";
        return;
    }
    std::string defaultFile = "export_" + DateUtils::getCurrentDate() + ".csv";
    std::string path = InputUtils::readLine("Enter output CSV filename [Default: " + defaultFile + ", 'c' to cancel]: ");
    if (InputUtils::equalsIgnoreCase(path, "c")) {
        return;
    }
    if (path.empty()) {
        path = defaultFile;
    }

    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        if (!InputUtils::readConfirmation("File '" + path + "' already exists. Overwrite?")) {
            std::cout << "  [i] Export cancelled.\n";
            return;
        }
    }

    std::string error;
    if (manager.exportToCsv(records, path, error)) {
        std::cout << "  [OK] Exported " << records.size() << " transaction(s) to '" << path << "'.\n";
    } else {
        std::cout << "  [ERROR] Export failed: " << error << "\n";
    }
}

void handleSearchAndFilter(const FinanceManager& manager) {
    TransactionFilter filter;
    while (true) {
        printHeader("SEARCH AND FILTER TRANSACTIONS");
        std::cout << "Active Filter Criteria (leave any filter unset to keep it unrestricted):\n";
        std::cout << "  1. Transaction ID:      " << (filter.id ? std::to_string(*filter.id) : "Any") << "\n";
        std::cout << "  2. Exact Date:          " << (filter.exactDate ? *filter.exactDate : "Any") << "\n";
        std::cout << "  3. Date Range:          " << (filter.startDate ? *filter.startDate : "Min")
                  << " to " << (filter.endDate ? *filter.endDate : "Max") << "\n";
        std::cout << "  4. Transaction Type:    " << (filter.type ? Transaction::typeToString(*filter.type) : "Any") << "\n";
        std::cout << "  5. Category:            " << (filter.category ? *filter.category : "Any") << "\n";
        std::cout << "  6. Amount Range (INR):  "
                  << (filter.minAmountPaise ? InputUtils::formatPaise(*filter.minAmountPaise) : "Min")
                  << " to "
                  << (filter.maxAmountPaise ? InputUtils::formatPaise(*filter.maxAmountPaise) : "Max") << "\n";
        std::cout << "  7. Payment Method:      " << (filter.paymentMethod ? *filter.paymentMethod : "Any") << "\n";
        std::cout << "  8. Description Keyword: " << (filter.keyword ? *filter.keyword : "Any") << "\n";
        printDivider();
        std::cout << "  9. Run Search & View Matching Transactions\n";
        std::cout << " 10. Export Matching Results to CSV\n";
        std::cout << " 11. Clear All Filters & View Full List\n";
        std::cout << "  0. Return to Main Menu\n";

        int choice = InputUtils::readMenuChoice("Select option (0-11): ", 0, 11);
        if (choice == 0) break;

        switch (choice) {
            case 1: {
                int idVal = 0;
                std::string in = InputUtils::readLine("Enter ID (or blank to clear): ");
                if (in.empty()) filter.id.reset();
                else if (InputUtils::parsePositiveInt(in, idVal)) filter.id = idVal;
                else std::cout << "  [!] Invalid ID.\n";
                break;
            }
            case 2: {
                std::string in = InputUtils::readLine("Enter exact date YYYY-MM-DD (or blank to clear): ");
                if (in.empty()) filter.exactDate.reset();
                else if (DateUtils::isValidDate(in)) filter.exactDate = in;
                else std::cout << "  [!] Invalid date.\n";
                break;
            }
            case 3: {
                std::string s = InputUtils::readLine("Start date YYYY-MM-DD (or blank for unrestricted): ");
                std::string e = InputUtils::readLine("End date YYYY-MM-DD (or blank for unrestricted): ");
                if (!s.empty() && !DateUtils::isValidDate(s)) {
                    std::cout << "  [!] Invalid start date.\n";
                    break;
                }
                if (!e.empty() && !DateUtils::isValidDate(e)) {
                    std::cout << "  [!] Invalid end date.\n";
                    break;
                }
                if (!s.empty() && !e.empty() && s > e) {
                    std::cout << "  [!] Start date cannot be later than end date.\n";
                    break;
                }
                filter.startDate = s.empty() ? std::nullopt : std::make_optional(s);
                filter.endDate = e.empty() ? std::nullopt : std::make_optional(e);
                break;
            }
            case 4: {
                std::string in = InputUtils::readLine("Type (1=Expense, 2=Income, blank=Any): ");
                if (in.empty()) filter.type.reset();
                else if (in == "1") filter.type = TransactionType::Expense;
                else if (in == "2") filter.type = TransactionType::Income;
                else std::cout << "  [!] Invalid type choice.\n";
                break;
            }
            case 5: {
                std::string in = InputUtils::readLine("Enter category name (or blank to clear): ");
                if (in.empty()) filter.category.reset();
                else filter.category = in;
                break;
            }
            case 6: {
                std::string minStr = InputUtils::readLine("Minimum amount INR (or blank): ");
                std::string maxStr = InputUtils::readLine("Maximum amount INR (or blank): ");
                std::optional<std::int64_t> minP, maxP;
                std::int64_t parsed = 0;
                if (!minStr.empty()) {
                    if (!InputUtils::parseAmountToPaise(minStr, parsed)) {
                        std::cout << "  [!] Invalid minimum amount.\n";
                        break;
                    }
                    minP = parsed;
                }
                if (!maxStr.empty()) {
                    if (!InputUtils::parseAmountToPaise(maxStr, parsed)) {
                        std::cout << "  [!] Invalid maximum amount.\n";
                        break;
                    }
                    maxP = parsed;
                }
                if (minP && maxP && *minP > *maxP) {
                    std::cout << "  [!] Minimum amount cannot exceed maximum amount.\n";
                    break;
                }
                filter.minAmountPaise = minP;
                filter.maxAmountPaise = maxP;
                break;
            }
            case 7: {
                std::string in = InputUtils::readLine("Enter payment method (or blank to clear): ");
                if (in.empty()) filter.paymentMethod.reset();
                else filter.paymentMethod = in;
                break;
            }
            case 8: {
                std::string in = InputUtils::readLine("Enter description keyword (case-insensitive, blank to clear): ");
                if (in.empty()) filter.keyword.reset();
                else filter.keyword = in;
                break;
            }
            case 9: {
                auto matches = manager.filterTransactions(filter);
                displayPaginatedTransactions(matches, "FILTERED TRANSACTIONS");
                break;
            }
            case 10: {
                auto matches = manager.filterTransactions(filter);
                handleExportFlow(manager, matches, "Filtered Results");
                break;
            }
            case 11: {
                filter = TransactionFilter{};
                std::cout << "  [OK] All filters cleared.\n";
                displayPaginatedTransactions(manager.getAllTransactions(), "ALL TRANSACTIONS");
                break;
            }
        }
    }
}

void handleSortTransactions(const FinanceManager& manager) {
    printHeader("SORT TRANSACTIONS");
    std::cout << "  1. Date (Oldest First)\n"
              << "  2. Date (Newest First)\n"
              << "  3. Amount (Smallest First)\n"
              << "  4. Amount (Largest First)\n"
              << "  5. Category (Alphabetical A-Z)\n"
              << "  6. Transaction ID (Ascending)\n"
              << "  7. Transaction ID (Descending)\n"
              << "  0. Return to Main Menu\n";

    int choice = InputUtils::readMenuChoice("Select sort order (0-7): ", 0, 7);
    if (choice == 0) return;

    SortCriteria criteria = SortCriteria::IdAscending;
    std::string label;
    switch (choice) {
        case 1: criteria = SortCriteria::DateOldestFirst; label = "Sorted by Date (Oldest First)"; break;
        case 2: criteria = SortCriteria::DateNewestFirst; label = "Sorted by Date (Newest First)"; break;
        case 3: criteria = SortCriteria::AmountSmallestFirst; label = "Sorted by Amount (Smallest First)"; break;
        case 4: criteria = SortCriteria::AmountLargestFirst; label = "Sorted by Amount (Largest First)"; break;
        case 5: criteria = SortCriteria::CategoryAlphabetical; label = "Sorted by Category (A-Z)"; break;
        case 6: criteria = SortCriteria::IdAscending; label = "Sorted by ID (Ascending)"; break;
        case 7: criteria = SortCriteria::IdDescending; label = "Sorted by ID (Descending)"; break;
    }

    auto sortedView = FinanceManager::sortTransactions(manager.getAllTransactions(), criteria);
    displayPaginatedTransactions(sortedView, label);
}

void handleManageCategories(FinanceManager& manager) {
    while (true) {
        printHeader("MANAGE CATEGORIES");
        std::cout << "  1. View All Categories\n"
                  << "  2. Add Custom Expense Category\n"
                  << "  3. Add Custom Income Category\n"
                  << "  4. Delete Custom Category\n"
                  << "  0. Return to Main Menu\n";

        int choice = InputUtils::readMenuChoice("Select option (0-4): ", 0, 4);
        if (choice == 0) break;

        if (choice == 1) {
            printDivider();
            std::cout << "EXPENSE CATEGORIES:\n";
            for (const auto& c : manager.getCategoriesByType(TransactionType::Expense)) {
                std::cout << "  - " << c.getName() << (c.isBuiltIn() ? " (Built-in)" : " (Custom)") << "\n";
            }
            std::cout << "\nINCOME CATEGORIES:\n";
            for (const auto& c : manager.getCategoriesByType(TransactionType::Income)) {
                std::cout << "  - " << c.getName() << (c.isBuiltIn() ? " (Built-in)" : " (Custom)") << "\n";
            }
        } else if (choice == 2 || choice == 3) {
            TransactionType type = (choice == 2) ? TransactionType::Expense : TransactionType::Income;
            std::string name = InputUtils::readLine("Enter new " + Transaction::typeToString(type) +
                                                    " category name (or 'c' to cancel): ");
            if (InputUtils::equalsIgnoreCase(name, "c")) continue;
            std::string error;
            if (manager.addCustomCategory(name, type, error)) {
                std::cout << "  [OK] Custom category '" << InputUtils::trim(name) << "' added and saved.\n";
            } else {
                std::cout << "  [ERROR] " << error << "\n";
            }
        } else if (choice == 4) {
            std::string name = InputUtils::readLine("Enter custom category name to delete (or 'c' to cancel): ");
            if (InputUtils::equalsIgnoreCase(name, "c") || name.empty()) continue;
            if (!InputUtils::readConfirmation("Delete custom category '" + name + "'?")) continue;
            std::string error;
            if (manager.deleteCustomCategory(name, error)) {
                std::cout << "  [OK] Custom category deleted.\n";
            } else {
                std::cout << "  [ERROR] " << error << "\n";
            }
        }
    }
}

void handleManageBudgets(FinanceManager& manager) {
    while (true) {
        printHeader("MANAGE MONTHLY BUDGETS");
        std::cout << "  1. View Monthly Budget Status\n"
                  << "  2. Set or Update a Category Budget\n"
                  << "  3. Remove a Category Budget\n"
                  << "  0. Return to Main Menu\n";

        int choice = InputUtils::readMenuChoice("Select option (0-3): ", 0, 3);
        if (choice == 0) break;

        if (choice == 1) {
            std::string month = InputUtils::readLine("Enter month YYYY-MM [Default: " +
                                                     DateUtils::getCurrentMonth() + "]: ");
            if (month.empty()) month = DateUtils::getCurrentMonth();
            if (!DateUtils::isValidMonth(month)) {
                std::cout << "  [!] Invalid month format. Expected YYYY-MM.\n";
                continue;
            }
            auto evals = manager.evaluateMonthBudgets(month);
            printDivider();
            std::cout << "BUDGET STATUS FOR " << month << ":\n";
            if (evals.empty()) {
                std::cout << "  No budgets configured for " << month << ".\n";
            } else {
                std::cout << std::left
                          << std::setw(20) << "Category"
                          << std::setw(15) << "Limit"
                          << std::setw(15) << "Spent"
                          << std::setw(15) << "Remaining"
                          << std::setw(10) << "Used %"
                          << "Status\n";
                printDivider();
                for (const auto& ev : evals) {
                    std::ostringstream pct;
                    pct << std::fixed << std::setprecision(1) << ev.percentageUsed << "%";
                    std::cout << std::left
                              << std::setw(20) << truncateText(ev.category, 18)
                              << std::setw(15) << InputUtils::formatPaise(ev.limitPaise)
                              << std::setw(15) << InputUtils::formatPaise(ev.spentPaise)
                              << std::setw(15) << InputUtils::formatPaise(ev.remainingPaise)
                              << std::setw(10) << pct.str()
                              << ev.statusLabel << "\n";
                }
            }
        } else if (choice == 2) {
            std::string month = InputUtils::readLine("Enter month YYYY-MM [Default: " +
                                                     DateUtils::getCurrentMonth() + ", 'c' to cancel]: ");
            if (InputUtils::equalsIgnoreCase(month, "c")) continue;
            if (month.empty()) month = DateUtils::getCurrentMonth();
            if (!DateUtils::isValidMonth(month)) {
                std::cout << "  [!] Invalid month format.\n";
                continue;
            }
            auto catOpt = selectCategoryInteractive(manager, TransactionType::Expense);
            if (!catOpt.has_value()) continue;

            std::string amtStr = InputUtils::readLine("Enter monthly budget limit in INR for '" +
                                                      *catOpt + "': ");
            std::int64_t limitPaise = 0;
            if (!InputUtils::parseAmountToPaise(amtStr, limitPaise)) {
                std::cout << "  [!] Invalid positive monetary limit.\n";
                continue;
            }

            std::string error;
            if (manager.setMonthlyBudget(month, *catOpt, limitPaise, error)) {
                std::cout << "  [OK] Budget saved for " << *catOpt << " in " << month << ".\n";
                showBudgetAlertForExpense(manager, DateUtils::getMonthStartDate(month), *catOpt);
            } else {
                std::cout << "  [ERROR] " << error << "\n";
            }
        } else if (choice == 3) {
            std::string month = InputUtils::readLine("Enter month YYYY-MM [Default: " +
                                                     DateUtils::getCurrentMonth() + "]: ");
            if (month.empty()) month = DateUtils::getCurrentMonth();
            auto catOpt = selectCategoryInteractive(manager, TransactionType::Expense);
            if (!catOpt.has_value()) continue;
            if (!InputUtils::readConfirmation("Remove budget for '" + *catOpt + "' in " + month + "?")) {
                continue;
            }
            std::string error;
            if (manager.deleteMonthlyBudget(month, *catOpt, error)) {
                std::cout << "  [OK] Budget removed.\n";
            } else {
                std::cout << "  [ERROR] " << error << "\n";
            }
        }
    }
}

void printPeriodSummaryDetails(const PeriodSummary& summary, const std::string& heading) {
    printHeader(heading + " (" + summary.startDate + " to " + summary.endDate + ")");
    std::cout << "  Note: Net cash flow = Total Income - Total Expenses for the period (not a bank balance).\n\n";
    std::cout << std::left << std::setw(34) << "  Total Income:"
              << InputUtils::formatPaise(summary.totalIncomePaise)
              << " (" << summary.incomeCount << " transaction(s))\n";
    std::cout << std::left << std::setw(34) << "  Total Expenses:"
              << InputUtils::formatPaise(summary.totalExpensePaise)
              << " (" << summary.expenseCount << " transaction(s))\n";
    std::cout << std::left << std::setw(34) << "  Net Cash Flow:"
              << InputUtils::formatPaise(summary.netCashFlowPaise) << "\n";
    std::cout << std::left << std::setw(34) << "  Average Expense Transaction:"
              << (summary.expenseCount > 0 ? InputUtils::formatPaise(summary.averageExpensePaise) : "N/A (no expenses)") << "\n";

    if (summary.largestExpense.has_value()) {
        std::cout << std::left << std::setw(34) << "  Largest Expense:"
                  << InputUtils::formatPaise(summary.largestExpense->getAmountPaise())
                  << " [" << summary.largestExpense->getCategory() << " on "
                  << summary.largestExpense->getDate() << ", ID #"
                  << summary.largestExpense->getId() << "]\n";
        std::cout << std::left << std::setw(34) << "  Highest-Spending Category:"
                  << summary.highestSpendingCategory << " ("
                  << InputUtils::formatPaise(summary.highestCategoryExpensePaise) << ")\n";
    } else {
        std::cout << std::left << std::setw(34) << "  Largest Expense:" << "N/A\n";
        std::cout << std::left << std::setw(34) << "  Highest-Spending Category:" << "N/A\n";
    }

    printDivider();
    std::cout << "  CATEGORY-WISE EXPENSE BREAKDOWN:\n";
    if (summary.expenseBreakdown.empty()) {
        std::cout << "  No expenses recorded in this period.\n";
    } else {
        std::cout << std::left
                  << std::setw(24) << "Category"
                  << std::setw(12) << "Tx Count"
                  << std::setw(18) << "Total Spent"
                  << "Share of Expenses\n";
        printDivider();
        for (const auto& cb : summary.expenseBreakdown) {
            std::cout << std::left
                      << std::setw(24) << cb.category
                      << std::setw(12) << cb.transactionCount
                      << std::setw(18) << InputUtils::formatPaise(cb.totalPaise)
                      << std::fixed << std::setprecision(1) << cb.percentageOfTotal << "%\n";
        }
    }
}

void handleReportsAndAnalytics(const FinanceManager& manager) {
    while (true) {
        printHeader("FINANCIAL REPORTS AND ANALYTICS");
        std::cout << "  1. Monthly Financial Report (with Budget vs. Actual)\n"
                  << "  2. Custom Date-Range Report\n"
                  << "  3. Category Breakdown Report (Income & Expense)\n"
                  << "  4. Payment-Method Report\n"
                  << "  5. Rule-Based Financial Insights\n"
                  << "  0. Return to Main Menu\n";

        int choice = InputUtils::readMenuChoice("Select option (0-5): ", 0, 5);
        if (choice == 0) break;

        if (choice == 1) {
            std::string month = InputUtils::readLine("Enter month YYYY-MM [Default: " +
                                                     DateUtils::getCurrentMonth() + "]: ");
            if (month.empty()) month = DateUtils::getCurrentMonth();
            if (!DateUtils::isValidMonth(month)) {
                std::cout << "  [!] Invalid month format (expected YYYY-MM).\n";
                continue;
            }
            PeriodSummary summary = manager.generateMonthlySummary(month);
            printPeriodSummaryDetails(summary, "MONTHLY FINANCIAL REPORT: " + month);

            printDivider();
            std::cout << "  BUDGET VS. ACTUAL SPENDING (" << month << "):\n";
            auto evals = manager.evaluateMonthBudgets(month);
            if (evals.empty()) {
                std::cout << "  No category budgets defined for " << month << ".\n";
            } else {
                for (const auto& ev : evals) {
                    std::cout << "  - " << std::left << std::setw(18) << ev.category
                              << " | Budget: " << std::setw(13) << InputUtils::formatPaise(ev.limitPaise)
                              << " | Actual: " << std::setw(13) << InputUtils::formatPaise(ev.spentPaise)
                              << " | Remaining: " << std::setw(13) << InputUtils::formatPaise(ev.remainingPaise)
                              << " | " << ev.statusLabel << "\n";
                }
            }
        } else if (choice == 2 || choice == 3 || choice == 4) {
            std::string s = InputUtils::readLine("Enter start date YYYY-MM-DD [Default: " +
                                                 DateUtils::getMonthStartDate(DateUtils::getCurrentMonth()) + "]: ");
            if (s.empty()) s = DateUtils::getMonthStartDate(DateUtils::getCurrentMonth());
            std::string e = InputUtils::readLine("Enter end date YYYY-MM-DD [Default: " +
                                                 DateUtils::getCurrentDate() + "]: ");
            if (e.empty()) e = DateUtils::getCurrentDate();

            if (!DateUtils::isValidDate(s) || !DateUtils::isValidDate(e) || s > e) {
                std::cout << "  [!] Invalid date range. Ensure valid YYYY-MM-DD dates and Start <= End.\n";
                continue;
            }

            if (choice == 2) {
                PeriodSummary summary = manager.generatePeriodSummary(s, e);
                printPeriodSummaryDetails(summary, "CUSTOM DATE-RANGE REPORT");
            } else if (choice == 3) {
                PeriodSummary summary = manager.generatePeriodSummary(s, e);
                printHeader("CATEGORY REPORT (" + s + " to " + e + ")");
                std::cout << "EXPENSE CATEGORIES (Highest Expenditure: "
                          << (summary.highestSpendingCategory.empty() ? "N/A" : summary.highestSpendingCategory)
                          << "):\n";
                if (summary.expenseBreakdown.empty()) {
                    std::cout << "  No expense transactions in this period.\n";
                } else {
                    for (const auto& cb : summary.expenseBreakdown) {
                        std::cout << "  - " << std::left << std::setw(20) << cb.category
                                  << " | Count: " << std::setw(5) << cb.transactionCount
                                  << " | Total: " << std::setw(14) << InputUtils::formatPaise(cb.totalPaise)
                                  << " | Share: " << std::fixed << std::setprecision(1)
                                  << cb.percentageOfTotal << "%\n";
                    }
                }
                printDivider();
                std::cout << "INCOME CATEGORIES:\n";
                if (summary.incomeBreakdown.empty()) {
                    std::cout << "  No income transactions in this period.\n";
                } else {
                    for (const auto& cb : summary.incomeBreakdown) {
                        std::cout << "  - " << std::left << std::setw(20) << cb.category
                                  << " | Count: " << std::setw(5) << cb.transactionCount
                                  << " | Total: " << std::setw(14) << InputUtils::formatPaise(cb.totalPaise)
                                  << " | Share: " << std::fixed << std::setprecision(1)
                                  << cb.percentageOfTotal << "%\n";
                    }
                }
            } else if (choice == 4) {
                auto pmReport = manager.generatePaymentMethodReport(s, e);
                printHeader("PAYMENT-METHOD REPORT (" + s + " to " + e + ")");
                std::cout << std::left
                          << std::setw(18) << "Payment Method"
                          << std::setw(18) << "Total Expenses"
                          << std::setw(12) << "Exp Count"
                          << std::setw(18) << "Total Income"
                          << "Inc Count\n";
                printDivider();
                for (const auto& row : pmReport) {
                    std::cout << std::left
                              << std::setw(18) << row.paymentMethod
                              << std::setw(18) << InputUtils::formatPaise(row.totalExpensePaise)
                              << std::setw(12) << row.expenseCount
                              << std::setw(18) << InputUtils::formatPaise(row.totalIncomePaise)
                              << row.incomeCount << "\n";
                }
            }
        } else if (choice == 5) {
            printHeader("RULE-BASED FINANCIAL INSIGHTS");
            auto insights = manager.generateFinancialInsights();
            for (const auto& item : insights) {
                std::cout << "  * " << item << "\n";
            }
        }
    }
}

void handleBackupAndRestore(FinanceManager& manager) {
    while (true) {
        printHeader("BACKUP AND RESTORE DATA");
        std::cout << "  1. Create a Backup of Current Data\n"
                  << "  2. List Available Backups\n"
                  << "  3. Restore from a Backup\n"
                  << "  0. Return to Main Menu\n";

        int choice = InputUtils::readMenuChoice("Select option (0-3): ", 0, 3);
        if (choice == 0) break;

        if (choice == 1) {
            std::string backupId, error;
            if (manager.createBackup(backupId, error)) {
                std::cout << "  [OK] Backup created successfully! Backup ID: " << backupId << "\n";
            } else {
                std::cout << "  [ERROR] Failed to create backup: " << error << "\n";
            }
        } else if (choice == 2) {
            auto backups = manager.listBackups();
            if (backups.empty()) {
                std::cout << "  No backups found in 'backups/' directory.\n";
            } else {
                std::cout << "\nAvailable Backups (newest first):\n";
                for (std::size_t i = 0; i < backups.size(); ++i) {
                    std::cout << "  " << (i + 1) << ". ID: " << backups[i].id
                              << " (" << backups[i].directory << ")\n";
                }
            }
        } else if (choice == 3) {
            auto backups = manager.listBackups();
            if (backups.empty()) {
                std::cout << "  [!] No backups available to restore.\n";
                continue;
            }
            std::cout << "\nSelect a Backup to Restore:\n";
            for (std::size_t i = 0; i < backups.size(); ++i) {
                std::cout << "  " << (i + 1) << ". " << backups[i].id << "\n";
            }
            std::cout << "  0. Cancel\n";
            int sel = InputUtils::readMenuChoice("Select backup number: ", 0, static_cast<int>(backups.size()));
            if (sel == 0) {
                std::cout << "  [i] Restore cancelled.\n";
                continue;
            }
            const std::string& chosenId = backups[static_cast<std::size_t>(sel - 1)].id;
            if (!InputUtils::readConfirmation("Restore backup '" + chosenId +
                                              "'? Current active data will be backed up first.")) {
                std::cout << "  [i] Restore cancelled.\n";
                continue;
            }
            std::string preBackupId, error;
            if (manager.restoreFromBackup(chosenId, preBackupId, error)) {
                std::cout << "  [OK] Restored backup '" << chosenId
                          << "'. Previous active state saved under backup ID '" << preBackupId << "'.\n";
            } else {
                std::cout << "  [ERROR] Restore failed (active data unchanged): " << error << "\n";
            }
        }
    }
}

void handleHelpAndAbout() {
    printHeader("HELP AND ABOUT FINTRACK v1.0");
    std::cout << "  Application: FinTrack — Personal Finance & Expense Tracker (C++17)\n"
              << "  Currency:    Indian Rupees (INR, stored internally as integer paise)\n"
              << "  Storage:     Local offline CSV files in 'data/' and snapshots in 'backups/'\n\n"
              << "  KEY GUIDELINES:\n"
              << "  - Dates must use YYYY-MM-DD (e.g., 2026-10-23) and months use YYYY-MM.\n"
              << "  - Amounts are stored in exact integer paise (std::int64_t) to prevent\n"
              << "    floating-point rounding errors.\n"
              << "  - Every change is saved immediately using a temporary file replacement\n"
              << "    strategy so unexpected power loss never corrupts existing files.\n"
              << "  - FinTrack is an offline personal bookkeeping tool and never connects\n"
              << "    to real bank accounts or executes financial transfers.\n";
}

} // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    printHeader("WELCOME TO FINTRACK v1.0 — PERSONAL FINANCE & EXPENSE TRACKER");

    CsvStorage storage("data", "backups");
    FinanceManager manager(storage);

    std::vector<std::string> startupWarnings;
    std::string startupError;
    if (!manager.initialize(startupWarnings, startupError)) {
        std::cout << "  [FATAL] Storage initialization error: " << startupError << "\n";
        return 1;
    }

    if (!startupWarnings.empty()) {
        std::cout << "  [!] Data Recovery Warnings during Startup:\n";
        for (const auto& w : startupWarnings) {
            std::cout << "      - " << w << "\n";
        }
        std::cout << "      (Suspicious files were backed up automatically in 'backups/'.)\n";
    }

    displayDashboard(manager);

    while (true) {
        std::cout << "\n============================== FINTRACK MAIN MENU ==============================\n"
                  << "   1. Dashboard                        8. Manage Categories\n"
                  << "   2. Add Transaction                  9. Manage Monthly Budgets\n"
                  << "   3. View All Transactions           10. Financial Reports and Analytics\n"
                  << "   4. Edit Transaction                11. Export Transactions to CSV\n"
                  << "   5. Delete Transaction              12. Backup and Restore Data\n"
                  << "   6. Search and Filter Transactions  13. Help and About\n"
                  << "   7. Sort Transactions               14. Exit\n"
                  << "================================================================================\n";

        int choice = InputUtils::readMenuChoice("Enter your choice (1-14): ", 1, 14);

        switch (choice) {
            case 1:
                displayDashboard(manager);
                break;
            case 2:
                handleAddTransaction(manager);
                displayDashboard(manager);
                break;
            case 3:
                displayPaginatedTransactions(
                    FinanceManager::sortTransactions(manager.getAllTransactions(), SortCriteria::DateNewestFirst),
                    "ALL TRANSACTIONS");
                break;
            case 4:
                handleEditTransaction(manager);
                displayDashboard(manager);
                break;
            case 5:
                handleDeleteTransaction(manager);
                displayDashboard(manager);
                break;
            case 6:
                handleSearchAndFilter(manager);
                break;
            case 7:
                handleSortTransactions(manager);
                break;
            case 8:
                handleManageCategories(manager);
                break;
            case 9:
                handleManageBudgets(manager);
                displayDashboard(manager);
                break;
            case 10:
                handleReportsAndAnalytics(manager);
                break;
            case 11:
                handleExportFlow(manager, manager.getAllTransactions(), "All Transactions");
                break;
            case 12:
                handleBackupAndRestore(manager);
                displayDashboard(manager);
                break;
            case 13:
                handleHelpAndAbout();
                break;
            case 14:
                if (manager.hasUnsavedChanges()) {
                    if (!InputUtils::readConfirmation(
                            "Warning: A recent file write failed. Exit anyway?")) {
                        break;
                    }
                }
                std::cout << "\nThank you for using FinTrack! All saved records are persisted in 'data/'. Goodbye!\n";
                return 0;
        }
    }
}