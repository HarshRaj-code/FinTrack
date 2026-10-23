#include "Budget.h"
#include "DateUtils.h"
#include "InputUtils.h"
#include <stdexcept>

namespace fintrack {

Budget::Budget(const std::string& month, const std::string& category, std::int64_t limitPaise)
    : month_(InputUtils::trim(month)),
      category_(InputUtils::trim(category)),
      limitPaise_(limitPaise) {
    if (!DateUtils::isValidMonth(month_)) {
        throw std::invalid_argument("Invalid budget month format (expected YYYY-MM): " + month_);
    }
    if (category_.empty()) {
        throw std::invalid_argument("Budget category cannot be empty.");
    }
    if (limitPaise_ <= 0) {
        throw std::invalid_argument("Budget limit must be positive.");
    }
}

const std::string& Budget::getMonth() const { return month_; }
const std::string& Budget::getCategory() const { return category_; }
std::int64_t Budget::getLimitPaise() const { return limitPaise_; }

void Budget::setLimitPaise(std::int64_t limitPaise) {
    if (limitPaise <= 0) {
        throw std::invalid_argument("Budget limit must be positive.");
    }
    limitPaise_ = limitPaise;
}

BudgetEvaluation Budget::evaluate(std::int64_t actualSpentPaise) const {
    BudgetEvaluation eval;
    eval.month = month_;
    eval.category = category_;
    eval.limitPaise = limitPaise_;
    eval.spentPaise = (actualSpentPaise < 0) ? 0 : actualSpentPaise;
    eval.remainingPaise = limitPaise_ - eval.spentPaise;
    eval.percentageUsed = (static_cast<double>(eval.spentPaise) * 100.0) /
                          static_cast<double>(limitPaise_);

    // Exact integer cross-multiplication avoids floating-point boundary ambiguity at 80% and 100%
    if (eval.spentPaise > limitPaise_) {
        eval.state = BudgetState::Exceeded;
        eval.statusLabel = "Budget Exceeded";
    } else if (eval.spentPaise == limitPaise_) {
        eval.state = BudgetState::LimitReached;
        eval.statusLabel = "Limit Reached (100%)";
    } else if (eval.spentPaise * 100LL >= limitPaise_ * 80LL) {
        eval.state = BudgetState::ApproachingLimit;
        eval.statusLabel = "Approaching Limit";
    } else {
        eval.state = BudgetState::OnTrack;
        eval.statusLabel = "On Track";
    }

    return eval;
}

} // namespace fintrack