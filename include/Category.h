#ifndef FINTRACK_CATEGORY_H
#define FINTRACK_CATEGORY_H

#include "Transaction.h"
#include <string>
#include <vector>

namespace fintrack {

class Category {
public:
    Category() = default;
    Category(const std::string& name, TransactionType type, bool isBuiltIn);

    const std::string& getName() const;
    TransactionType getType() const;
    bool isBuiltIn() const;

    static std::vector<Category> getDefaultCategories();
    static std::vector<std::string> getDefaultPaymentMethods();
    static bool isValidPaymentMethod(const std::string& method);

private:
    std::string name_;
    TransactionType type_ = TransactionType::Expense;
    bool isBuiltIn_ = false;
};

} // namespace fintrack

#endif // FINTRACK_CATEGORY_H