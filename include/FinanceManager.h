#ifndef FINTRACK_FINANCE_MANAGER_H
#define FINTRACK_FINANCE_MANAGER_H

#include "Budget.h"
#include "Category.h"
#include "CsvStorage.h"
#include "DateUtils.h"
#include "Transaction.h"
#include <optional>
#include <string>
#include <vector>

namespace fintrack {

struct TransactionFilter {
    std::optional<int> id;
    std::optional<std::string> exactDate;
    std::optional<std::string> startDate;
    std::optional<std::string> endDate;
    std::optional<TransactionType> type;
    std::optional<std::string> category;
    std::optional<std::int64_t> minAmountPaise;
    std::optional<std::int64_t> maxAmountPaise;
    std::optional<std::string> paymentMethod;
    std::optional<std::string> keyword;

    bool isEmpty() const;
};

enum class SortCriteria {
    DateOldestFirst,
    DateNewestFirst,
    AmountSmallestFirst,
    AmountLargestFirst,
    CategoryAlphabetical,
    IdAscending,
    IdDescending
};

struct CategoryBreakdown {
    std::string category;
    TransactionType type = TransactionType::Expense;
    std::int64_t totalPaise = 0;
    int transactionCount = 0;
    double percentageOfTotal = 0.0;
};

struct PaymentMethodBreakdown {
    std::string paymentMethod;
    std::int64_t totalExpensePaise = 0;
    int expenseCount = 0;
    std::int64_t totalIncomePaise = 0;
    int incomeCount = 0;
};

struct PeriodSummary {
    std::string startDate;
    std::string endDate;
    std::int64_t totalIncomePaise = 0;
    std::int64_t totalExpensePaise = 0;
    std::int64_t netCashFlowPaise = 0;
    int incomeCount = 0;
    int expenseCount = 0;
    std::int64_t averageExpensePaise = 0;
    std::optional<Transaction> largestExpense;
    std::string highestSpendingCategory;
    std::int64_t highestCategoryExpensePaise = 0;
    std::vector<CategoryBreakdown> expenseBreakdown;
    std::vector<CategoryBreakdown> incomeBreakdown;
};

struct DashboardData {
    std::string currentMonth;
    std::string currentDate;
    std::int64_t monthlyIncomePaise = 0;
    std::int64_t monthlyExpensePaise = 0;
    std::int64_t netCashFlowPaise = 0;
    std::int64_t todayExpensePaise = 0;
    bool hasMonthlyBudgets = false;
    std::int64_t totalMonthlyBudgetLimitPaise = 0;
    std::int64_t totalBudgetedCategorySpentPaise = 0;
    std::int64_t remainingTotalBudgetPaise = 0;
    int monthlyTransactionCount = 0;
    std::vector<Transaction> recentTransactions;
    std::vector<BudgetEvaluation> budgetAlerts;
};

class FinanceManager {
public:
    explicit FinanceManager(const CsvStorage& storage);

    // Startup initialization
    bool initialize(std::vector<std::string>& outWarnings, std::string& outError);

    // Transaction CRUD
    bool addTransaction(const std::string& date,
                        TransactionType type,
                        std::int64_t amountPaise,
                        const std::string& category,
                        const std::string& paymentMethod,
                        const std::string& description,
                        Transaction& outCreated,
                        std::string& outError);

    bool editTransaction(int id,
                         const std::string& newDate,
                         TransactionType newType,
                         std::int64_t newAmountPaise,
                         const std::string& newCategory,
                         const std::string& newPaymentMethod,
                         const std::string& newDescription,
                         Transaction& outUpdated,
                         std::string& outError);

    bool deleteTransaction(int id, std::string& outError);
    const Transaction* findTransactionById(int id) const;
    const std::vector<Transaction>& getAllTransactions() const;

    // Search, Filter, and Sort
    std::vector<Transaction> filterTransactions(const TransactionFilter& filter) const;
    static std::vector<Transaction> sortTransactions(const std::vector<Transaction>& list,
                                                     SortCriteria criteria);

    // Categories
    const std::vector<Category>& getAllCategories() const;
    std::vector<Category> getCategoriesByType(TransactionType type) const;
    bool isValidCategoryForType(const std::string& name, TransactionType type, std::string& outCanonicalName) const;
    bool addCustomCategory(const std::string& name, TransactionType type, std::string& outError);
    bool deleteCustomCategory(const std::string& name, std::string& outError);

    // Budgets
    bool setMonthlyBudget(const std::string& month,
                          const std::string& category,
                          std::int64_t limitPaise,
                          std::string& outError);
    bool deleteMonthlyBudget(const std::string& month,
                             const std::string& category,
                             std::string& outError);
    std::optional<BudgetEvaluation> evaluateCategoryBudget(const std::string& month,
                                                           const std::string& category) const;
    std::vector<BudgetEvaluation> evaluateMonthBudgets(const std::string& month) const;
    const std::vector<Budget>& getAllBudgets() const;

    // Analytics, Dashboard, and Insights
    DashboardData getDashboardData(const std::string& currentDate = DateUtils::getCurrentDate()) const;
    PeriodSummary generatePeriodSummary(const std::string& startDate, const std::string& endDate) const;
    PeriodSummary generateMonthlySummary(const std::string& month) const;
    std::vector<PaymentMethodBreakdown> generatePaymentMethodReport(const std::string& startDate,
                                                                    const std::string& endDate) const;
    std::vector<std::string> generateFinancialInsights(const std::string& referenceDate = DateUtils::getCurrentDate()) const;

    // Export, Backup, and Restore
    bool exportToCsv(const std::vector<Transaction>& list,
                     const std::string& filePath,
                     std::string& outError) const;
    bool createBackup(std::string& outBackupId, std::string& outError) const;
    std::vector<BackupInfo> listBackups() const;
    bool restoreFromBackup(const std::string& backupId,
                           std::string& outPreRestoreBackupId,
                           std::string& outError);

    bool hasUnsavedChanges() const;

private:
    CsvStorage storage_;
    std::vector<Category> categories_;
    std::vector<Transaction> transactions_;
    std::vector<Budget> budgets_;
    int nextTransactionId_ = 1;
    bool hasUnsavedChanges_ = false;

    std::int64_t calculateCategorySpentInMonth(const std::string& month,
                                               const std::string& category) const;
};

} // namespace fintrack

#endif // FINTRACK_FINANCE_MANAGER_H