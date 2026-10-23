#include "Category.h"
#include "InputUtils.h"
#include <stdexcept>

namespace fintrack {

Category::Category(const std::string& name, TransactionType type, bool isBuiltIn)
    : name_(InputUtils::trim(name)), type_(type), isBuiltIn_(isBuiltIn) {
    if (name_.empty()) {
        throw std::invalid_argument("Category name cannot be empty.");
    }
}

const std::string& Category::getName() const { return name_; }
TransactionType Category::getType() const { return type_; }
bool Category::isBuiltIn() const { return isBuiltIn_; }

std::vector<Category> Category::getDefaultCategories() {
    return {
        // Default Expense Categories (13)
        Category("Food & Dining", TransactionType::Expense, true),
        Category("Groceries", TransactionType::Expense, true),
        Category("Transportation", TransactionType::Expense, true),
        Category("Rent", TransactionType::Expense, true),
        Category("Bills & Utilities", TransactionType::Expense, true),
        Category("Education", TransactionType::Expense, true),
        Category("Health & Medical", TransactionType::Expense, true),
        Category("Shopping", TransactionType::Expense, true),
        Category("Entertainment", TransactionType::Expense, true),
        Category("Personal Care", TransactionType::Expense, true),
        Category("Travel", TransactionType::Expense, true),
        Category("Subscriptions", TransactionType::Expense, true),
        Category("Other Expenses", TransactionType::Expense, true),

        // Default Income Categories (8)
        Category("Salary", TransactionType::Income, true),
        Category("Internship Stipend", TransactionType::Income, true),
        Category("Freelancing", TransactionType::Income, true),
        Category("Scholarship", TransactionType::Income, true),
        Category("Gift", TransactionType::Income, true),
        Category("Interest", TransactionType::Income, true),
        Category("Refund", TransactionType::Income, true),
        Category("Other Income", TransactionType::Income, true)
    };
}

std::vector<std::string> Category::getDefaultPaymentMethods() {
    return {
        "Cash",
        "UPI",
        "Debit Card",
        "Credit Card",
        "Bank Transfer",
        "Other"
    };
}

bool Category::isValidPaymentMethod(const std::string& method) {
    for (const auto& pm : getDefaultPaymentMethods()) {
        if (InputUtils::equalsIgnoreCase(pm, method)) {
            return true;
        }
    }
    return false;
}

} // namespace fintrack