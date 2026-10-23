#include "Budget.h"
#include "Category.h"
#include "CsvStorage.h"
#include "DateUtils.h"
#include "FinanceManager.h"
#include "InputUtils.h"
#include "Transaction.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace fintrack;

namespace {

int testsPassed = 0;

void check(bool condition, const std::string& testName) {
    if (!condition) {
        std::cerr << "[FAIL] " << testName << "\n";
        std::exit(1);
    }
    ++testsPassed;
    std::cout << "[PASS] " << testName << "\n";
}

void testMoneyParsingAndPrecision() {
    std::int64_t paise = 0;
    check(InputUtils::parseAmountToPaise("100.00", paise) && paise == 10000LL, "Accept ₹100.00");
    check(InputUtils::parseAmountToPaise("0.01", paise) && paise == 1LL, "Accept ₹0.01");
    check(InputUtils::parseAmountToPaise("999.99", paise) && paise == 99999LL, "Accept ₹999.99");
    check(InputUtils::parseAmountToPaise("250", paise) && paise == 25000LL, "Accept whole rupee 250");
    check(InputUtils::parseAmountToPaise("499.5", paise) && paise == 49950LL, "Accept single decimal 499.5");

    check(!InputUtils::parseAmountToPaise("0", paise), "Reject zero amount");
    check(!InputUtils::parseAmountToPaise("0.00", paise), "Reject 0.00 amount");
    check(!InputUtils::parseAmountToPaise("-50.00", paise), "Reject negative amount");
    check(!InputUtils::parseAmountToPaise("12.345", paise), "Reject 3 decimal places without silent rounding");
    check(!InputUtils::parseAmountToPaise("abc", paise), "Reject alphabetic input");
    check(!InputUtils::parseAmountToPaise("999999999999999.99", paise), "Reject overflow amount");

    // Verify exact arithmetic (0.10 + 0.20 == 0.30 exactly in paise)
    std::int64_t a = 0, b = 0;
    InputUtils::parseAmountToPaise("0.10", a);
    InputUtils::parseAmountToPaise("0.20", b);
    check(a + b == 30LL && InputUtils::formatPaisePlain(a + b) == "0.30",
          "Integer paise avoids floating-point precision drift");
}

void testDateValidation() {
    check(DateUtils::isValidDate("2026-10-23"), "Accept ordinary valid date");
    check(DateUtils::isValidDate("2024-02-29"), "Accept valid leap day 2024-02-29");
    check(!DateUtils::isValidDate("2026-02-29"), "Reject invalid leap day 2026-02-29");
    check(!DateUtils::isValidDate("2026-02-30"), "Reject impossible date 2026-02-30");
    check(!DateUtils::isValidDate("2026-04-31"), "Reject April 31");
    check(DateUtils::isValidMonth("2026-10"), "Accept valid month YYYY-MM");
    check(!DateUtils::isValidMonth("2026-13"), "Reject month 13");
    check(DateUtils::getMonthStartDate("2024-02") == "2024-02-01" &&
          DateUtils::getMonthEndDate("2024-02") == "2024-02-29",
          "Validate leap month start/end boundaries");
}

void testCrudAndBudgets() {
    std::string testDataDir = "test_tmp_data";
    std::string testBackupDir = "test_tmp_backups";
    std::error_code ec;
    fs::remove_all(testDataDir, ec);
    fs::remove_all(testBackupDir, ec);

    CsvStorage storage(testDataDir, testBackupDir);
    FinanceManager mgr(storage);
    std::vector<std::string> warnings;
    std::string err;
    check(mgr.initialize(warnings, err), "Initialize clean FinanceManager");

    // Add 1 Income (₹25,000.00) and 1 Expense (₹7,500.00)
    Transaction incTx, expTx;
    check(mgr.addTransaction("2026-10-05", TransactionType::Income, 2500000LL,
                             "Salary", "Bank Transfer", "October stipend", incTx, err),
          "Add income transaction");
    check(mgr.addTransaction("2026-10-10", TransactionType::Expense, 750000LL,
                             "Food & Dining", "UPI", "Hostel mess & dining", expTx, err),
          "Add expense transaction");
    check(incTx.getId() == 1 && expTx.getId() == 2, "Sequential unique IDs assigned");

    // Set budget of ₹10,000.00 for Food & Dining in 2026-10
    check(mgr.setMonthlyBudget("2026-10", "Food & Dining", 1000000LL, err), "Create monthly budget");

    // 7,500 / 10,000 = 75% (< 80% -> On Track)
    auto ev75 = mgr.evaluateCategoryBudget("2026-10", "Food & Dining");
    check(ev75.has_value() && ev75->state == BudgetState::OnTrack && ev75->remainingPaise == 250000LL,
          "Budget below 80% is On Track and income is ignored");

    // Edit expense to ₹8,000.00 (80% -> Approaching Limit) while preserving ID
    Transaction updatedTx;
    check(mgr.editTransaction(expTx.getId(), "2026-10-10", TransactionType::Expense, 800000LL,
                              "Food & Dining", "UPI", "Updated dining", updatedTx, err) &&
          updatedTx.getId() == expTx.getId(),
          "Edit expense preserves original ID");
    auto ev80 = mgr.evaluateCategoryBudget("2026-10", "Food & Dining");
    check(ev80.has_value() && ev80->state == BudgetState::ApproachingLimit, "Budget at 80% is Approaching Limit");

    // Edit expense to ₹9,999.00 (99.99% -> Approaching Limit)
    mgr.editTransaction(expTx.getId(), "2026-10-10", TransactionType::Expense, 999900LL,
                        "Food & Dining", "UPI", "Near limit", updatedTx, err);
    auto ev99 = mgr.evaluateCategoryBudget("2026-10", "Food & Dining");
    check(ev99.has_value() && ev99->state == BudgetState::ApproachingLimit, "Budget just below 100% is Approaching Limit");

    // Edit expense to ₹10,000.00 (100% -> Limit Reached)
    mgr.editTransaction(expTx.getId(), "2026-10-10", TransactionType::Expense, 1000000LL,
                        "Food & Dining", "UPI", "Exact limit", updatedTx, err);
    auto ev100 = mgr.evaluateCategoryBudget("2026-10", "Food & Dining");
    check(ev100.has_value() && ev100->state == BudgetState::LimitReached && ev100->remainingPaise == 0LL,
          "Budget at exactly 100% is Limit Reached");

    // Edit expense to ₹12,000.00 (120% -> Exceeded, remaining = -₹2,000.00)
    mgr.editTransaction(expTx.getId(), "2026-10-10", TransactionType::Expense, 1200000LL,
                        "Food & Dining", "UPI", "Over limit", updatedTx, err);
    auto ev120 = mgr.evaluateCategoryBudget("2026-10", "Food & Dining");
    check(ev120.has_value() && ev120->state == BudgetState::Exceeded && ev120->remainingPaise == -200000LL,
          "Budget above 100% is Exceeded with negative remaining balance");

    // Delete expense and verify recalculation + non-reuse of deleted ID
    check(mgr.deleteTransaction(expTx.getId(), err), "Delete expense transaction");
    auto evAfterDel = mgr.evaluateCategoryBudget("2026-10", "Food & Dining");
    check(evAfterDel.has_value() && evAfterDel->spentPaise == 0LL && evAfterDel->state == BudgetState::OnTrack,
          "Budget recalculates to 0 spent after expense deletion");

    Transaction thirdTx;
    check(mgr.addTransaction("2026-10-12", TransactionType::Expense, 50000LL,
                             "Groceries", "Cash", "Vegetables, \"fresh\", and\nmultiline note", thirdTx, err) &&
          thirdTx.getId() == 3,
          "Deleted ID 2 is never reused; new transaction gets ID 3");

    // Persistence & Restart Verification (including commas, quotes, and newlines)
    FinanceManager restartedMgr(storage);
    check(restartedMgr.initialize(warnings, err) && warnings.empty(), "Restart and reload from CSV cleanly");
    const Transaction* reloaded = restartedMgr.findTransactionById(3);
    check(reloaded != nullptr &&
          reloaded->getDescription() == "Vegetables, \"fresh\", and\nmultiline note",
          "CSV persistence preserves commas, escaped quotes, and line breaks");

    // Inject a duplicate ID and malformed row into transactions.csv and verify recovery
    {
        std::ofstream appendOut(storage.getTransactionsPath(), std::ios::app);
        appendOut << "3,2026-10-15,Expense,10000,Groceries,Cash,Duplicate ID row\n";
        appendOut << "corrupt_id,2026-99-99,InvalidType,-50,UnknownCat,BadMethod\n";
    }
    FinanceManager recoveredMgr(storage);
    warnings.clear();
    check(recoveredMgr.initialize(warnings, err) && warnings.size() == 2 &&
          recoveredMgr.getAllTransactions().size() == 2,
          "Malformed and duplicate ID rows are rejected while valid records survive");

    // Search, Filter, and Sort Verification
    Transaction tx4;
    recoveredMgr.addTransaction("2026-10-08", TransactionType::Expense, 150000LL,
                                "Education", "Debit Card", "C++ Reference Book", tx4, err);
    TransactionFilter filter;
    filter.type = TransactionType::Expense;
    filter.startDate = "2026-10-01";
    filter.endDate = "2026-10-31";
    filter.keyword = "book";
    auto filtered = recoveredMgr.filterTransactions(filter);
    check(filtered.size() == 1 && filtered[0].getId() == tx4.getId(),
          "Combined filter (type + date range + case-insensitive keyword)");

    auto sortedByAmt = FinanceManager::sortTransactions(recoveredMgr.getAllTransactions(),
                                                        SortCriteria::AmountSmallestFirst);
    check(sortedByAmt.front().getAmountPaise() == 50000LL &&
          recoveredMgr.getAllTransactions().front().getId() == 1,
          "Sorting orders view correctly without altering underlying saved vector");

    // Test failed save reporting (attempting to export over internal database file)
    check(!recoveredMgr.exportToCsv(recoveredMgr.getAllTransactions(), storage.getTransactionsPath(), err),
          "Prevent export from overwriting internal transactions.csv");

    fs::remove_all(testDataDir, ec);
    fs::remove_all(testBackupDir, ec);
}

} // namespace

int main() {
    std::cout << "=== Running FinTrack Automated Verification Suite ===\n";
    testMoneyParsingAndPrecision();
    testDateValidation();
    testCrudAndBudgets();
    std::cout << "=== All " << testsPassed << " automated test assertions PASSED! ===\n";
    return 0;
}