#ifndef FINTRACK_DATE_UTILS_H
#define FINTRACK_DATE_UTILS_H

#include <string>

namespace fintrack {

class DateUtils {
public:
    static bool isLeapYear(int year);
    static int getDaysInMonth(int year, int month);

    // Validates strict YYYY-MM-DD format and calendar existence (1900-2100)
    static bool isValidDate(const std::string& dateStr);

    // Validates strict YYYY-MM format (1900-2100)
    static bool isValidMonth(const std::string& monthStr);

    // Extracts YYYY-MM from a valid YYYY-MM-DD string
    static std::string extractMonth(const std::string& dateStr);

    // Extracts day of month (1-31) from a valid YYYY-MM-DD string
    static int extractDay(const std::string& dateStr);

    // Returns current local date in YYYY-MM-DD
    static std::string getCurrentDate();

    // Returns current local month in YYYY-MM
    static std::string getCurrentMonth();

    // Returns first and last calendar date of a valid YYYY-MM month
    static std::string getMonthStartDate(const std::string& monthStr);
    static std::string getMonthEndDate(const std::string& monthStr);

    // Returns the previous month in YYYY-MM format (e.g., "2026-03" -> "2026-02")
    static std::string getPreviousMonth(const std::string& monthStr);

    // Builds YYYY-MM-DD from components, clamping day to the month's max days
    static std::string makeClampedDate(const std::string& monthStr, int day);

private:
    static bool parseYearMonthDay(const std::string& dateStr, int& year, int& month, int& day);
    static bool parseYearMonth(const std::string& monthStr, int& year, int& month);
};

} // namespace fintrack

#endif // FINTRACK_DATE_UTILS_H