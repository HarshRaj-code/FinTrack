#include "DateUtils.h"
#include <ctime>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace fintrack {

bool DateUtils::isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int DateUtils::getDaysInMonth(int year, int month) {
    static const int daysPerMonth[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    if (month < 1 || month > 12) {
        return 0;
    }
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return daysPerMonth[month - 1];
}

bool DateUtils::parseYearMonthDay(const std::string& dateStr, int& year, int& month, int& day) {
    if (dateStr.size() != 10 || dateStr[4] != '-' || dateStr[7] != '-') {
        return false;
    }
    for (std::size_t i = 0; i < dateStr.size(); ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(dateStr[i]))) {
            return false;
        }
    }
    year = std::stoi(dateStr.substr(0, 4));
    month = std::stoi(dateStr.substr(5, 2));
    day = std::stoi(dateStr.substr(8, 2));
    return true;
}

bool DateUtils::parseYearMonth(const std::string& monthStr, int& year, int& month) {
    if (monthStr.size() != 7 || monthStr[4] != '-') {
        return false;
    }
    for (std::size_t i = 0; i < monthStr.size(); ++i) {
        if (i == 4) continue;
        if (!std::isdigit(static_cast<unsigned char>(monthStr[i]))) {
            return false;
        }
    }
    year = std::stoi(monthStr.substr(0, 4));
    month = std::stoi(monthStr.substr(5, 2));
    return true;
}

bool DateUtils::isValidDate(const std::string& dateStr) {
    int year = 0, month = 0, day = 0;
    if (!parseYearMonthDay(dateStr, year, month, day)) {
        return false;
    }
    if (year < 1900 || year > 2100) {
        return false;
    }
    if (month < 1 || month > 12) {
        return false;
    }
    int maxDays = getDaysInMonth(year, month);
    return day >= 1 && day <= maxDays;
}

bool DateUtils::isValidMonth(const std::string& monthStr) {
    int year = 0, month = 0;
    if (!parseYearMonth(monthStr, year, month)) {
        return false;
    }
    return (year >= 1900 && year <= 2100) && (month >= 1 && month <= 12);
}

std::string DateUtils::extractMonth(const std::string& dateStr) {
    if (dateStr.size() >= 7) {
        return dateStr.substr(0, 7);
    }
    return "";
}

int DateUtils::extractDay(const std::string& dateStr) {
    int year = 0, month = 0, day = 0;
    if (parseYearMonthDay(dateStr, year, month, day)) {
        return day;
    }
    return 1;
}

std::string DateUtils::getCurrentDate() {
    std::time_t now = std::time(nullptr);
    std::tm localTm{};
#if defined(_WIN32)
    localtime_s(&localTm, &now);
#else
    localtime_r(&now, &localTm);
#endif
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (localTm.tm_year + 1900) << '-'
        << std::setw(2) << (localTm.tm_mon + 1) << '-'
        << std::setw(2) << localTm.tm_mday;
    return oss.str();
}

std::string DateUtils::getCurrentMonth() {
    return extractMonth(getCurrentDate());
}

std::string DateUtils::getMonthStartDate(const std::string& monthStr) {
    if (!isValidMonth(monthStr)) {
        return "";
    }
    return monthStr + "-01";
}

std::string DateUtils::getMonthEndDate(const std::string& monthStr) {
    int year = 0, month = 0;
    if (!parseYearMonth(monthStr, year, month)) {
        return "";
    }
    int lastDay = getDaysInMonth(year, month);
    std::ostringstream oss;
    oss << monthStr << '-' << std::setfill('0') << std::setw(2) << lastDay;
    return oss.str();
}

std::string DateUtils::getPreviousMonth(const std::string& monthStr) {
    int year = 0, month = 0;
    if (!parseYearMonth(monthStr, year, month)) {
        return "";
    }
    if (month == 1) {
        year -= 1;
        month = 12;
    } else {
        month -= 1;
    }
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << year << '-' << std::setw(2) << month;
    return oss.str();
}

std::string DateUtils::makeClampedDate(const std::string& monthStr, int day) {
    int year = 0, month = 0;
    if (!parseYearMonth(monthStr, year, month)) {
        return "";
    }
    int maxDay = getDaysInMonth(year, month);
    int clamped = std::max(1, std::min(day, maxDay));
    std::ostringstream oss;
    oss << monthStr << '-' << std::setfill('0') << std::setw(2) << clamped;
    return oss.str();
}

} // namespace fintrack