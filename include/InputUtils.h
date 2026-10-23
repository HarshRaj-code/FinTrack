#ifndef FINTRACK_INPUT_UTILS_H
#define FINTRACK_INPUT_UTILS_H

#include <cstdint>
#include <string>

namespace fintrack {

class InputUtils {
public:
    // String manipulation utilities
    static std::string trim(const std::string& str);
    static std::string toLower(const std::string& str);
    static bool equalsIgnoreCase(const std::string& a, const std::string& b);
    static bool containsIgnoreCase(const std::string& haystack, const std::string& needle);

    // Monetary parsing and formatting (1 Rupee = 100 Paise)
    // Rejects <= 0, alphabetic characters, > 2 decimals, and overflow
    static bool parseAmountToPaise(const std::string& input, std::int64_t& outPaise);

    // Formats paise with ₹ symbol and comma separators (e.g., "₹25,000.00")
    static std::string formatPaise(std::int64_t paise, bool includeCurrencySymbol = true);

    // Formats paise as plain decimal rupees without commas for CSV export (e.g., "1250.75")
    static std::string formatPaisePlain(std::int64_t paise);

    // Integer parsing
    static bool parsePositiveInt(const std::string& input, int& outValue);

    // Safe console input readers using std::getline
    static std::string readLine(const std::string& prompt);
    static int readMenuChoice(const std::string& prompt, int minOption, int maxOption);
    static bool readConfirmation(const std::string& prompt);
};

} // namespace fintrack

#endif // FINTRACK_INPUT_UTILS_H