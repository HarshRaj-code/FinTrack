#ifndef FINTRACK_TRANSACTION_H
#define FINTRACK_TRANSACTION_H

#include <cstdint>
#include <string>

namespace fintrack {

enum class TransactionType {
    Income,
    Expense
};

class Transaction {
public:
    Transaction() = default;
    Transaction(int id,
                const std::string& date,
                TransactionType type,
                std::int64_t amountPaise,
                const std::string& category,
                const std::string& paymentMethod,
                const std::string& description);

    int getId() const;
    const std::string& getDate() const;
    TransactionType getType() const;
    std::int64_t getAmountPaise() const;
    const std::string& getCategory() const;
    const std::string& getPaymentMethod() const;
    const std::string& getDescription() const;

    void setDate(const std::string& date);
    void setType(TransactionType type);
    void setAmountPaise(std::int64_t amountPaise);
    void setCategory(const std::string& category);
    void setPaymentMethod(const std::string& paymentMethod);
    void setDescription(const std::string& description);

    static std::string typeToString(TransactionType type);
    static bool stringToType(const std::string& str, TransactionType& outType);

private:
    int id_ = 0;
    std::string date_;
    TransactionType type_ = TransactionType::Expense;
    std::int64_t amountPaise_ = 0;
    std::string category_;
    std::string paymentMethod_;
    std::string description_;
};

} // namespace fintrack

#endif // FINTRACK_TRANSACTION_H