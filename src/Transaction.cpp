#include "Transaction.h"
#include "DateUtils.h"
#include "InputUtils.h"
#include <stdexcept>

namespace fintrack {

Transaction::Transaction(int id,
                         const std::string& date,
                         TransactionType type,
                         std::int64_t amountPaise,
                         const std::string& category,
                         const std::string& paymentMethod,
                         const std::string& description)
    : id_(id),
      date_(date),
      type_(type),
      amountPaise_(amountPaise),
      category_(InputUtils::trim(category)),
      paymentMethod_(InputUtils::trim(paymentMethod)),
      description_(InputUtils::trim(description)) {
    if (id_ <= 0) {
        throw std::invalid_argument("Transaction ID must be positive.");
    }
    if (!DateUtils::isValidDate(date_)) {
        throw std::invalid_argument("Invalid transaction date: " + date_);
    }
    if (amountPaise_ <= 0) {
        throw std::invalid_argument("Transaction amount must be positive.");
    }
    if (category_.empty()) {
        throw std::invalid_argument("Transaction category cannot be empty.");
    }
    if (paymentMethod_.empty()) {
        throw std::invalid_argument("Payment method cannot be empty.");
    }
}

int Transaction::getId() const { return id_; }
const std::string& Transaction::getDate() const { return date_; }
TransactionType Transaction::getType() const { return type_; }
std::int64_t Transaction::getAmountPaise() const { return amountPaise_; }
const std::string& Transaction::getCategory() const { return category_; }
const std::string& Transaction::getPaymentMethod() const { return paymentMethod_; }
const std::string& Transaction::getDescription() const { return description_; }

void Transaction::setDate(const std::string& date) {
    if (!DateUtils::isValidDate(date)) {
        throw std::invalid_argument("Invalid transaction date: " + date);
    }
    date_ = date;
}

void Transaction::setType(TransactionType type) {
    type_ = type;
}

void Transaction::setAmountPaise(std::int64_t amountPaise) {
    if (amountPaise <= 0) {
        throw std::invalid_argument("Transaction amount must be greater than zero.");
    }
    amountPaise_ = amountPaise;
}

void Transaction::setCategory(const std::string& category) {
    std::string cleaned = InputUtils::trim(category);
    if (cleaned.empty()) {
        throw std::invalid_argument("Category cannot be empty.");
    }
    category_ = cleaned;
}

void Transaction::setPaymentMethod(const std::string& paymentMethod) {
    std::string cleaned = InputUtils::trim(paymentMethod);
    if (cleaned.empty()) {
        throw std::invalid_argument("Payment method cannot be empty.");
    }
    paymentMethod_ = cleaned;
}

void Transaction::setDescription(const std::string& description) {
    description_ = InputUtils::trim(description);
}

std::string Transaction::typeToString(TransactionType type) {
    return (type == TransactionType::Income) ? "Income" : "Expense";
}

bool Transaction::stringToType(const std::string& str, TransactionType& outType) {
    if (InputUtils::equalsIgnoreCase(str, "Income")) {
        outType = TransactionType::Income;
        return true;
    }
    if (InputUtils::equalsIgnoreCase(str, "Expense")) {
        outType = TransactionType::Expense;
        return true;
    }
    return false;
}

} // namespace fintrack