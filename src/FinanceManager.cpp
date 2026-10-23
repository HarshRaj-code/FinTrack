#include "FinanceManager.h"
#include "DateUtils.h"
#include "InputUtils.h"
#include <algorithm>
#include <iomanip>
#include <map>
#include <sstream>

namespace fintrack {

bool TransactionFilter::isEmpty() const {
    return !id.has_value() && !exactDate.has_value() && !startDate.has_value() &&
           !endDate.has_value() && !type.has_value() && !category.has_value() &&
           !minAmountPaise.has_value() && !maxAmountPaise.has_value() &&
           !paymentMethod.has_value() && !keyword.has_value();
}

FinanceManager::FinanceManager(const CsvStorage& storage)
    : storage_(storage),
      categories_(Category::getDefaultCategories()),
      nextTransactionId_(1),
      hasUnsavedChanges_(false) {}

bool FinanceManager::initialize(std::vector<std::string>& outWarnings, std::string& outError) {
    outWarnings.clear();
    if (!storage_.ensureDirectoriesExist(outError)) {
        return false;
    }

    LoadReport catRep = storage_.loadCategories(categories_);
    LoadReport txRep = storage_.loadTransactions(categories_, transactions_, nextTransactionId_);
    LoadReport bgRep = storage_.loadBudgets(categories_, budgets_);

    for (const auto& w : catRep.warnings) outWarnings.push_back(w);
    for (const auto& w : txRep.warnings) outWarnings.push_back(w);
    for (const auto& w : bgRep.warnings) outWarnings.push_back(w);

    hasUnsavedChanges_ = false;
    return true;
}

const Transaction* FinanceManager::findTransactionById(int id) const {
    auto it = std::find_if(transactions_.begin(), transactions_.end(),
                           [id](const Transaction& tx) { return tx.getId() == id; });
    if (it == transactions_.end()) {
        return nullptr;
    }
    return &(*it);
}

const std::vector<Transaction>& FinanceManager::getAllTransactions() const {
    return transactions_;
}

bool FinanceManager::isValidCategoryForType(const std::string& name,
                                            TransactionType type,
                                            std::string& outCanonicalName) const {
    for (const auto& cat : categories_) {
        if (cat.getType() == type && InputUtils::equalsIgnoreCase(cat.getName(), name)) {
            outCanonicalName = cat.getName();
            return true;
        }
    }
    return false;
}

bool FinanceManager::addTransaction(const std::string& date,
                                    TransactionType type,
                                    std::int64_t amountPaise,
                                    const std::string& category,
                                    const std::string& paymentMethod,
                                    const std::string& description,
                                    Transaction& outCreated,
                                    std::string& outError) {
    if (!DateUtils::isValidDate(date)) {
        outError = "Invalid date (expected valid YYYY-MM-DD).";
        return false;
    }
    if (amountPaise <= 0) {
        outError = "Transaction amount must be greater than zero.";
        return false;
    }
    std::string canonicalCat;
    if (!isValidCategoryForType(category, type, canonicalCat)) {
        outError = "Category '" + category + "' is not valid for " + Transaction::typeToString(type) + ".";
        return false;
    }
    if (!Category::isValidPaymentMethod(paymentMethod)) {
        outError = "Invalid payment method: " + paymentMethod;
        return false;
    }

    int assignedId = nextTransactionId_;
    Transaction candidate(assignedId, date, type, amountPaise, canonicalCat, paymentMethod, description);

    std::vector<Transaction> updatedList = transactions_;
    updatedList.push_back(candidate);

    if (!storage_.saveTransactions(updatedList, outError)) {
        hasUnsavedChanges_ = true;
        return false;
    }

    transactions_ = std::move(updatedList);
    nextTransactionId_ = assignedId + 1;
    hasUnsavedChanges_ = false;
    outCreated = candidate;
    return true;
}

bool FinanceManager::editTransaction(int id,
                                     const std::string& newDate,
                                     TransactionType newType,
                                     std::int64_t newAmountPaise,
                                     const std::string& newCategory,
                                     const std::string& newPaymentMethod,
                                     const std::string& newDescription,
                                     Transaction& outUpdated,
                                     std::string& outError) {
    auto it = std::find_if(transactions_.begin(), transactions_.end(),
                           [id](const Transaction& tx) { return tx.getId() == id; });
    if (it == transactions_.end()) {
        outError = "Transaction ID " + std::to_string(id) + " not found.";
        return false;
    }

    if (!DateUtils::isValidDate(newDate)) {
        outError = "Invalid date (expected valid YYYY-MM-DD).";
        return false;
    }
    if (newAmountPaise <= 0) {
        outError = "Amount must be greater than zero.";
        return false;
    }
    std::string canonicalCat;
    if (!isValidCategoryForType(newCategory, newType, canonicalCat)) {
        outError = "Category '" + newCategory + "' is not valid for " + Transaction::typeToString(newType) + ".";
        return false;
    }
    if (!Category::isValidPaymentMethod(newPaymentMethod)) {
        outError = "Invalid payment method: " + newPaymentMethod;
        return false;
    }

    // Build validated replacement without mutating current record until persistence succeeds
    Transaction replacement(id, newDate, newType, newAmountPaise, canonicalCat, newPaymentMethod, newDescription);
    std::size_t index = static_cast<std::size_t>(std::distance(transactions_.begin(), it));
    std::vector<Transaction> updatedList = transactions_;
    updatedList[index] = replacement;

    if (!storage_.saveTransactions(updatedList, outError)) {
        hasUnsavedChanges_ = true;
        return false;
    }

    transactions_ = std::move(updatedList);
    hasUnsavedChanges_ = false;
    outUpdated = replacement;
    return true;
}

bool FinanceManager::deleteTransaction(int id, std::string& outError) {
    auto it = std::find_if(transactions_.begin(), transactions_.end(),
                           [id](const Transaction& tx) { return tx.getId() == id; });
    if (it == transactions_.end()) {
        outError = "Transaction ID " + std::to_string(id) + " does not exist.";
        return false;
    }

    std::vector<Transaction> updatedList = transactions_;
    updatedList.erase(updatedList.begin() + std::distance(transactions_.begin(), it));

    if (!storage_.saveTransactions(updatedList, outError)) {
        hasUnsavedChanges_ = true;
        return false;
    }

    transactions_ = std::move(updatedList);
    // Note: nextTransactionId_ is never decremented so deleted IDs are never reused
    hasUnsavedChanges_ = false;
    return true;
}

std::vector<Transaction> FinanceManager::filterTransactions(const TransactionFilter& filter) const {
    std::vector<Transaction> results;
    for (const auto& tx : transactions_) {
        if (filter.id.has_value() && tx.getId() != *filter.id) continue;
        if (filter.exactDate.has_value() && tx.getDate() != *filter.exactDate) continue;
        if (filter.startDate.has_value() && tx.getDate() < *filter.startDate) continue;
        if (filter.endDate.has_value() && tx.getDate() > *filter.endDate) continue;
        if (filter.type.has_value() && tx.getType() != *filter.type) continue;
        if (filter.category.has_value() &&
            !InputUtils::equalsIgnoreCase(tx.getCategory(), *filter.category)) continue;
        if (filter.minAmountPaise.has_value() && tx.getAmountPaise() < *filter.minAmountPaise) continue;
        if (filter.maxAmountPaise.has_value() && tx.getAmountPaise() > *filter.maxAmountPaise) continue;
        if (filter.paymentMethod.has_value() &&
            !InputUtils::equalsIgnoreCase(tx.getPaymentMethod(), *filter.paymentMethod)) continue;
        if (filter.keyword.has_value() &&
            !InputUtils::containsIgnoreCase(tx.getDescription(), *filter.keyword)) continue;

        results.push_back(tx);
    }
    return results;
}

std::vector<Transaction> FinanceManager::sortTransactions(const std::vector<Transaction>& list,
                                                          SortCriteria criteria) {
    std::vector<Transaction> sorted = list;
    std::sort(sorted.begin(), sorted.end(), [criteria](const Transaction& a, const Transaction& b) {
        switch (criteria) {
            case SortCriteria::DateOldestFirst:
                if (a.getDate() != b.getDate()) return a.getDate() < b.getDate();
                return a.getId() < b.getId();
            case SortCriteria::DateNewestFirst:
                if (a.getDate() != b.getDate()) return a.getDate() > b.getDate();
                return a.getId() > b.getId();
            case SortCriteria::AmountSmallestFirst:
                if (a.getAmountPaise() != b.getAmountPaise()) return a.getAmountPaise() < b.getAmountPaise();
                return a.getId() < b.getId();
            case SortCriteria::AmountLargestFirst:
                if (a.getAmountPaise() != b.getAmountPaise()) return a.getAmountPaise() > b.getAmountPaise();
                return a.getId() < b.getId();
            case SortCriteria::CategoryAlphabetical: {
                std::string catA = InputUtils::toLower(a.getCategory());
                std::string catB = InputUtils::toLower(b.getCategory());
                if (catA != catB) return catA < catB;
                return a.getId() < b.getId();
            }
            case SortCriteria::IdAscending:
                return a.getId() < b.getId();
            case SortCriteria::IdDescending:
                return a.getId() > b.getId();
        }
        return a.getId() < b.getId();
    });
    return sorted;
}

const std::vector<Category>& FinanceManager::getAllCategories() const {
    return categories_;
}

std::vector<Category> FinanceManager::getCategoriesByType(TransactionType type) const {
    std::vector<Category> filtered;
    for (const auto& cat : categories_) {
        if (cat.getType() == type) {
            filtered.push_back(cat);
        }
    }
    return filtered;
}

bool FinanceManager::addCustomCategory(const std::string& name,
                                       TransactionType type,
                                       std::string& outError) {
    std::string cleaned = InputUtils::trim(name);
    if (cleaned.empty()) {
        outError = "Category name cannot be empty or whitespace only.";
        return false;
    }
    for (const auto& existing : categories_) {
        if (InputUtils::equalsIgnoreCase(existing.getName(), cleaned)) {
            outError = "Category '" + existing.getName() + "' already exists.";
            return false;
        }
    }

    std::vector<Category> updated = categories_;
    updated.emplace_back(cleaned, type, false);
    if (!storage_.saveCategories(updated, outError)) {
        hasUnsavedChanges_ = true;
        return false;
    }

    categories_ = std::move(updated);
    hasUnsavedChanges_ = false;
    return true;
}

bool FinanceManager::deleteCustomCategory(const std::string& name, std::string& outError) {
    auto it = std::find_if(categories_.begin(), categories_.end(),
                           [&name](const Category& c) {
                               return InputUtils::equalsIgnoreCase(c.getName(), name);
                           });
    if (it == categories_.end()) {
        outError = "Category '" + name + "' not found.";
        return false;
    }
    if (it->isBuiltIn()) {
        outError = "Built-in category '" + it->getName() + "' cannot be deleted.";
        return false;
    }

    // Prevent deletion if referenced in transactions or budgets
    for (const auto& tx : transactions_) {
        if (InputUtils::equalsIgnoreCase(tx.getCategory(), it->getName())) {
            outError = "Cannot delete category '" + it->getName() +
                       "' because it is used by existing transactions.";
            return false;
        }
    }
    for (const auto& b : budgets_) {
        if (InputUtils::equalsIgnoreCase(b.getCategory(), it->getName())) {
            outError = "Cannot delete category '" + it->getName() +
                       "' because it has a configured monthly budget.";
            return false;
        }
    }

    std::vector<Category> updated = categories_;
    updated.erase(updated.begin() + std::distance(categories_.begin(), it));
    if (!storage_.saveCategories(updated, outError)) {
        hasUnsavedChanges_ = true;
        return false;
    }

    categories_ = std::move(updated);
    hasUnsavedChanges_ = false;
    return true;
}

std::int64_t FinanceManager::calculateCategorySpentInMonth(const std::string& month,
                                                           const std::string& category) const {
    std::int64_t spent = 0;
    for (const auto& tx : transactions_) {
        if (tx.getType() == TransactionType::Expense &&
            DateUtils::extractMonth(tx.getDate()) == month &&
            InputUtils::equalsIgnoreCase(tx.getCategory(), category)) {
            spent += tx.getAmountPaise();
        }
    }
    return spent;
}

bool FinanceManager::setMonthlyBudget(const std::string& month,
                                      const std::string& category,
                                      std::int64_t limitPaise,
                                      std::string& outError) {
    if (!DateUtils::isValidMonth(month)) {
        outError = "Invalid month format (expected YYYY-MM).";
        return false;
    }
    if (limitPaise <= 0) {
        outError = "Budget amount must be positive.";
        return false;
    }
    std::string canonicalCat;
    if (!isValidCategoryForType(category, TransactionType::Expense, canonicalCat)) {
        outError = "Budgets can only be assigned to valid Expense categories.";
        return false;
    }

    std::vector<Budget> updated = budgets_;
    auto it = std::find_if(updated.begin(), updated.end(),
                           [&month, &canonicalCat](const Budget& b) {
                               return b.getMonth() == month &&
                                      InputUtils::equalsIgnoreCase(b.getCategory(), canonicalCat);
                           });
    if (it != updated.end()) {
        it->setLimitPaise(limitPaise);
    } else {
        updated.emplace_back(month, canonicalCat, limitPaise);
    }

    if (!storage_.saveBudgets(updated, outError)) {
        hasUnsavedChanges_ = true;
        return false;
    }

    budgets_ = std::move(updated);
    hasUnsavedChanges_ = false;
    return true;
}

bool FinanceManager::deleteMonthlyBudget(const std::string& month,
                                         const std::string& category,
                                         std::string& outError) {
    auto it = std::find_if(budgets_.begin(), budgets_.end(),
                           [&month, &category](const Budget& b) {
                               return b.getMonth() == month &&
                                      InputUtils::equalsIgnoreCase(b.getCategory(), category);
                           });
    if (it == budgets_.end()) {
        outError = "No budget found for '" + category + "' in " + month + ".";
        return false;
    }

    std::vector<Budget> updated = budgets_;
    updated.erase(updated.begin() + std::distance(budgets_.begin(), it));
    if (!storage_.saveBudgets(updated, outError)) {
        hasUnsavedChanges_ = true;
        return false;
    }

    budgets_ = std::move(updated);
    hasUnsavedChanges_ = false;
    return true;
}

std::optional<BudgetEvaluation> FinanceManager::evaluateCategoryBudget(const std::string& month,
                                                                       const std::string& category) const {
    auto it = std::find_if(budgets_.begin(), budgets_.end(),
                           [&month, &category](const Budget& b) {
                               return b.getMonth() == month &&
                                      InputUtils::equalsIgnoreCase(b.getCategory(), category);
                           });
    if (it == budgets_.end()) {
        return std::nullopt;
    }
    std::int64_t spent = calculateCategorySpentInMonth(month, it->getCategory());
    return it->evaluate(spent);
}

std::vector<BudgetEvaluation> FinanceManager::evaluateMonthBudgets(const std::string& month) const {
    std::vector<BudgetEvaluation> evaluations;
    for (const auto& b : budgets_) {
        if (b.getMonth() == month) {
            std::int64_t spent = calculateCategorySpentInMonth(month, b.getCategory());
            evaluations.push_back(b.evaluate(spent));
        }
    }
    std::sort(evaluations.begin(), evaluations.end(),
              [](const BudgetEvaluation& a, const BudgetEvaluation& b) {
                  return InputUtils::toLower(a.category) < InputUtils::toLower(b.category);
              });
    return evaluations;
}

const std::vector<Budget>& FinanceManager::getAllBudgets() const {
    return budgets_;
}

DashboardData FinanceManager::getDashboardData(const std::string& currentDate) const {
    DashboardData data;
    data.currentDate = DateUtils::isValidDate(currentDate) ? currentDate : DateUtils::getCurrentDate();
    data.currentMonth = DateUtils::extractMonth(data.currentDate);

    for (const auto& tx : transactions_) {
        if (tx.getDate() == data.currentDate && tx.getType() == TransactionType::Expense) {
            data.todayExpensePaise += tx.getAmountPaise();
        }
        if (DateUtils::extractMonth(tx.getDate()) == data.currentMonth) {
            data.monthlyTransactionCount++;
            if (tx.getType() == TransactionType::Income) {
                data.monthlyIncomePaise += tx.getAmountPaise();
            } else {
                data.monthlyExpensePaise += tx.getAmountPaise();
            }
        }
    }
    data.netCashFlowPaise = data.monthlyIncomePaise - data.monthlyExpensePaise;

    std::vector<BudgetEvaluation> monthEvals = evaluateMonthBudgets(data.currentMonth);
    if (!monthEvals.empty()) {
        data.hasMonthlyBudgets = true;
        for (const auto& ev : monthEvals) {
            data.totalMonthlyBudgetLimitPaise += ev.limitPaise;
            data.totalBudgetedCategorySpentPaise += ev.spentPaise;
            if (ev.state != BudgetState::OnTrack) {
                data.budgetAlerts.push_back(ev);
            }
        }
        data.remainingTotalBudgetPaise =
            data.totalMonthlyBudgetLimitPaise - data.totalBudgetedCategorySpentPaise;
    }

    std::vector<Transaction> sortedRecent = sortTransactions(transactions_, SortCriteria::DateNewestFirst);
    if (sortedRecent.size() > 5) {
        sortedRecent.resize(5);
    }
    data.recentTransactions = std::move(sortedRecent);
    return data;
}

PeriodSummary FinanceManager::generatePeriodSummary(const std::string& startDate,
                                                    const std::string& endDate) const {
    PeriodSummary summary;
    summary.startDate = startDate;
    summary.endDate = endDate;

    std::map<std::string, std::pair<std::int64_t, int>> expenseByCat;
    std::map<std::string, std::pair<std::int64_t, int>> incomeByCat;

    for (const auto& tx : transactions_) {
        if (tx.getDate() < startDate || tx.getDate() > endDate) {
            continue;
        }
        if (tx.getType() == TransactionType::Income) {
            summary.totalIncomePaise += tx.getAmountPaise();
            summary.incomeCount++;
            incomeByCat[tx.getCategory()].first += tx.getAmountPaise();
            incomeByCat[tx.getCategory()].second += 1;
        } else {
            summary.totalExpensePaise += tx.getAmountPaise();
            summary.expenseCount++;
            expenseByCat[tx.getCategory()].first += tx.getAmountPaise();
            expenseByCat[tx.getCategory()].second += 1;

            if (!summary.largestExpense.has_value() ||
                tx.getAmountPaise() > summary.largestExpense->getAmountPaise() ||
                (tx.getAmountPaise() == summary.largestExpense->getAmountPaise() &&
                 tx.getId() < summary.largestExpense->getId())) {
                summary.largestExpense = tx;
            }
        }
    }

    summary.netCashFlowPaise = summary.totalIncomePaise - summary.totalExpensePaise;
    if (summary.expenseCount > 0) {
        summary.averageExpensePaise = summary.totalExpensePaise / summary.expenseCount;
    }

    for (const auto& kv : expenseByCat) {
        CategoryBreakdown cb;
        cb.category = kv.first;
        cb.type = TransactionType::Expense;
        cb.totalPaise = kv.second.first;
        cb.transactionCount = kv.second.second;
        cb.percentageOfTotal = (summary.totalExpensePaise > 0)
            ? (static_cast<double>(cb.totalPaise) * 100.0) / static_cast<double>(summary.totalExpensePaise)
            : 0.0;
        summary.expenseBreakdown.push_back(cb);

        if (cb.totalPaise > summary.highestCategoryExpensePaise) {
            summary.highestCategoryExpensePaise = cb.totalPaise;
            summary.highestSpendingCategory = cb.category;
        }
    }

    std::sort(summary.expenseBreakdown.begin(), summary.expenseBreakdown.end(),
              [](const CategoryBreakdown& a, const CategoryBreakdown& b) {
                  if (a.totalPaise != b.totalPaise) return a.totalPaise > b.totalPaise;
                  return a.category < b.category;
              });

    for (const auto& kv : incomeByCat) {
        CategoryBreakdown cb;
        cb.category = kv.first;
        cb.type = TransactionType::Income;
        cb.totalPaise = kv.second.first;
        cb.transactionCount = kv.second.second;
        cb.percentageOfTotal = (summary.totalIncomePaise > 0)
            ? (static_cast<double>(cb.totalPaise) * 100.0) / static_cast<double>(summary.totalIncomePaise)
            : 0.0;
        summary.incomeBreakdown.push_back(cb);
    }

    std::sort(summary.incomeBreakdown.begin(), summary.incomeBreakdown.end(),
              [](const CategoryBreakdown& a, const CategoryBreakdown& b) {
                  if (a.totalPaise != b.totalPaise) return a.totalPaise > b.totalPaise;
                  return a.category < b.category;
              });

    return summary;
}

PeriodSummary FinanceManager::generateMonthlySummary(const std::string& month) const {
    return generatePeriodSummary(DateUtils::getMonthStartDate(month),
                                 DateUtils::getMonthEndDate(month));
}

std::vector<PaymentMethodBreakdown> FinanceManager::generatePaymentMethodReport(
    const std::string& startDate,
    const std::string& endDate) const {
    std::map<std::string, PaymentMethodBreakdown> grouped;
    for (const auto& method : Category::getDefaultPaymentMethods()) {
        PaymentMethodBreakdown pmb;
        pmb.paymentMethod = method;
        grouped[method] = pmb;
    }

    for (const auto& tx : transactions_) {
        if (tx.getDate() < startDate || tx.getDate() > endDate) {
            continue;
        }
        auto& entry = grouped[tx.getPaymentMethod()];
        if (entry.paymentMethod.empty()) {
            entry.paymentMethod = tx.getPaymentMethod();
        }
        if (tx.getType() == TransactionType::Expense) {
            entry.totalExpensePaise += tx.getAmountPaise();
            entry.expenseCount++;
        } else {
            entry.totalIncomePaise += tx.getAmountPaise();
            entry.incomeCount++;
        }
    }

    std::vector<PaymentMethodBreakdown> report;
    for (const auto& kv : grouped) {
        report.push_back(kv.second);
    }
    std::sort(report.begin(), report.end(),
              [](const PaymentMethodBreakdown& a, const PaymentMethodBreakdown& b) {
                  if (a.totalExpensePaise != b.totalExpensePaise) {
                      return a.totalExpensePaise > b.totalExpensePaise;
                  }
                  return a.paymentMethod < b.paymentMethod;
              });
    return report;
}

std::vector<std::string> FinanceManager::generateFinancialInsights(const std::string& referenceDate) const {
    std::vector<std::string> insights;
    if (transactions_.empty()) {
        insights.push_back("Insufficient transaction history: add income and expense records to generate analytical insights.");
        return insights;
    }

    std::string validRefDate = DateUtils::isValidDate(referenceDate)
        ? referenceDate
        : DateUtils::getCurrentDate();
    std::string curMonth = DateUtils::extractMonth(validRefDate);
    int curDay = DateUtils::extractDay(validRefDate);

    // 1. Current month highest spending category
    PeriodSummary curMonthSummary = generateMonthlySummary(curMonth);
    if (curMonthSummary.expenseCount > 0) {
        insights.push_back("Highest monthly expense category in " + curMonth + ": " +
                           curMonthSummary.highestSpendingCategory + " (" +
                           InputUtils::formatPaise(curMonthSummary.highestCategoryExpensePaise) + ").");
    } else {
        insights.push_back("No expenses recorded yet for " + curMonth + ".");
    }

    // 2. Budget alerts for current month
    auto evals = evaluateMonthBudgets(curMonth);
    for (const auto& ev : evals) {
        if (ev.state == BudgetState::Exceeded) {
            insights.push_back("Budget exceeded in " + ev.category + " for " + curMonth +
                               ": spent " + InputUtils::formatPaise(ev.spentPaise) +
                               " against limit of " + InputUtils::formatPaise(ev.limitPaise) +
                               " (over by " + InputUtils::formatPaise(-ev.remainingPaise) + ").");
        } else if (ev.state == BudgetState::ApproachingLimit || ev.state == BudgetState::LimitReached) {
            std::ostringstream pct;
            pct << std::fixed << std::setprecision(1) << ev.percentageUsed;
            insights.push_back("Budget alert for " + ev.category + " in " + curMonth + ": " +
                               pct.str() + "% used (" + ev.statusLabel + ").");
        }
    }

    // 3. Month-to-Date (MTD) comparison against equivalent dates in previous month
    std::string prevMonth = DateUtils::getPreviousMonth(curMonth);
    std::string curMtdStart = DateUtils::getMonthStartDate(curMonth);
    std::string curMtdEnd = validRefDate;
    std::string prevMtdStart = DateUtils::getMonthStartDate(prevMonth);
    std::string prevMtdEnd = DateUtils::makeClampedDate(prevMonth, curDay);

    PeriodSummary curMtd = generatePeriodSummary(curMtdStart, curMtdEnd);
    PeriodSummary prevMtd = generatePeriodSummary(prevMtdStart, prevMtdEnd);

    if (curMtd.expenseCount == 0 && prevMtd.expenseCount == 0) {
        insights.push_back("Month-to-date comparison (" + curMtdStart + " to " + curMtdEnd +
                           " vs. " + prevMtdStart + " to " + prevMtdEnd +
                           "): insufficient data in both periods.");
    } else if (prevMtd.expenseCount == 0) {
        insights.push_back("Month-to-date (" + curMtdStart + " to " + curMtdEnd +
                           ") spending is " + InputUtils::formatPaise(curMtd.totalExpensePaise) +
                           ". No prior-month transactions exist in the equivalent period (" +
                           prevMtdStart + " to " + prevMtdEnd + ") for comparison.");
    } else {
        std::int64_t diff = curMtd.totalExpensePaise - prevMtd.totalExpensePaise;
        if (diff > 0) {
            insights.push_back("Month-to-date spending (" + curMtdStart + " to " + curMtdEnd + ": " +
                               InputUtils::formatPaise(curMtd.totalExpensePaise) + ") is higher by " +
                               InputUtils::formatPaise(diff) + " compared with the equivalent period last month (" +
                               prevMtdStart + " to " + prevMtdEnd + ": " +
                               InputUtils::formatPaise(prevMtd.totalExpensePaise) + ").");
        } else if (diff < 0) {
            insights.push_back("Month-to-date spending (" + curMtdStart + " to " + curMtdEnd + ": " +
                               InputUtils::formatPaise(curMtd.totalExpensePaise) + ") is lower by " +
                               InputUtils::formatPaise(-diff) + " compared with the equivalent period last month (" +
                               prevMtdStart + " to " + prevMtdEnd + ": " +
                               InputUtils::formatPaise(prevMtd.totalExpensePaise) + ").");
        } else {
            insights.push_back("Month-to-date spending (" + curMtdStart + " to " + curMtdEnd +
                               ") matches the equivalent period last month (" +
                               InputUtils::formatPaise(curMtd.totalExpensePaise) + ").");
        }

        // 4. Category-level rises in Month-to-Date window
        std::map<std::string, std::int64_t> prevCatMap;
        for (const auto& cb : prevMtd.expenseBreakdown) {
            prevCatMap[cb.category] = cb.totalPaise;
        }
        for (const auto& curCb : curMtd.expenseBreakdown) {
            auto it = prevCatMap.find(curCb.category);
            if (it != prevCatMap.end() && curCb.totalPaise > it->second) {
                insights.push_back("Category spending rise (month-to-date): " + curCb.category +
                                   " increased from " + InputUtils::formatPaise(it->second) +
                                   " to " + InputUtils::formatPaise(curCb.totalPaise) + ".");
            }
        }
    }

    return insights;
}

bool FinanceManager::exportToCsv(const std::vector<Transaction>& list,
                                 const std::string& filePath,
                                 std::string& outError) const {
    return storage_.exportTransactionsToCsv(list, filePath, outError);
}

bool FinanceManager::createBackup(std::string& outBackupId, std::string& outError) const {
    return storage_.createBackup(categories_, transactions_, budgets_, outBackupId, outError);
}

std::vector<BackupInfo> FinanceManager::listBackups() const {
    return storage_.listBackups();
}

bool FinanceManager::restoreFromBackup(const std::string& backupId,
                                       std::string& outPreRestoreBackupId,
                                       std::string& outError) {
    // Automatically back up current active data before restoring
    if (!createBackup(outPreRestoreBackupId, outError)) {
        outError = "Aborted restore because pre-restore safety backup failed: " + outError;
        return false;
    }

    if (!storage_.restoreBackup(backupId, categories_, transactions_, budgets_, nextTransactionId_, outError)) {
        return false;
    }

    hasUnsavedChanges_ = false;
    return true;
}

bool FinanceManager::hasUnsavedChanges() const {
    return hasUnsavedChanges_;
}

} // namespace fintrack