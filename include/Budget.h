#ifndef FINTRACK_BUDGET_H
#define FINTRACK_BUDGET_H

#include <cstdint>
#include <string>

namespace fintrack {

enum class BudgetState {
    OnTrack,          // Below 80%
    ApproachingLimit, // 80% to < 100%
    LimitReached,     // Exactly 100%
    Exceeded          // Above 100%
};

struct BudgetEvaluation {
    std::string month;
    std::string category;
    std::int64_t limitPaise = 0;
    std::int64_t spentPaise = 0;
    std::int64_t remainingPaise = 0; // Negative when over budget
    double percentageUsed = 0.0;
    BudgetState state = BudgetState::OnTrack;
    std::string statusLabel;
};

class Budget {
public:
    Budget() = default;
    Budget(const std::string& month, const std::string& category, std::int64_t limitPaise);

    const std::string& getMonth() const;
    const std::string& getCategory() const;
    std::int64_t getLimitPaise() const;

    void setLimitPaise(std::int64_t limitPaise);

    BudgetEvaluation evaluate(std::int64_t actualSpentPaise) const;

private:
    std::string month_;
    std::string category_;
    std::int64_t limitPaise_ = 0;
};

} // namespace fintrack

#endif // FINTRACK_BUDGET_H