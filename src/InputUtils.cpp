#include "InputUtils.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace fintrack {

namespace {
    // Maximum allowed single transaction or budget: ₹9,999,999,999.99
    constexpr std::int64_t MAX_RUPEES_PART = 9999999999LL;
}

std::string InputUtils::trim(const std::string& str) {
    std::size_t first = 0;
    while (first < str.size() && std::isspace(static_cast<unsigned char>(str[first]))) {
        ++first;
    }
    if (first == str.size()) {
        return "";
    }
    std::size_t last = str.size() - 1;
    while (last > first && std::isspace(static_cast<unsigned char>(str[last]))) {
        --last;
    }
    return str.substr(first, last - first + 1);
}

std::string InputUtils::toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

bool InputUtils::equalsIgnoreCase(const std::string& a, const std::string& b) {
    return toLower(trim(a)) == toLower(trim(b));
}

bool InputUtils::containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    std::string cleanNeedle = toLower(trim(needle));
    if (cleanNeedle.empty()) {
        return true;
    }
    return toLower(haystack).find(cleanNeedle) != std::string::npos;
}

bool InputUtils::parseAmountToPaise(const std::string& input, std::int64_t& outPaise) {
    std::string cleaned = trim(input);
    if (cleaned.empty()) {
        return false;
    }
    if (cleaned[0] == '+') {
        cleaned.erase(0, 1);
    }
    if (cleaned.empty() || cleaned[0] == '-') {
        return false;
    }

    std::size_t dotPos = cleaned.find('.');
    std::string rupeesPart;
    std::string paisePart;

    if (dotPos == std::string::npos) {
        rupeesPart = cleaned;
    } else {
        // Reject multiple decimal points
        if (cleaned.find('.', dotPos + 1) != std::string::npos) {
            return false;
        }
        rupeesPart = cleaned.substr(0, dotPos);
        paisePart = cleaned.substr(dotPos + 1);
        // Reject trailing dot ("100.") or more than 2 decimal digits ("10.555")
        if (paisePart.empty() || paisePart.size() > 2) {
            return false;
        }
    }

    if (rupeesPart.empty()) {
        rupeesPart = "0";
    }

    for (char c : rupeesPart) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    for (char c : paisePart) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    // Strip leading zeros to safely check length against overflow
    std::size_t nonZeroPos = rupeesPart.find_first_not_of('0');
    std::string normalizedRupees = (nonZeroPos == std::string::npos) ? "0" : rupeesPart.substr(nonZeroPos);
    if (normalizedRupees.size() > 10) {
        return false;
    }

    std::int64_t rupees = std::stoll(normalizedRupees);
    if (rupees > MAX_RUPEES_PART) {
        return false;
    }

    std::int64_t paise = 0;
    if (paisePart.size() == 1) {
        paise = static_cast<std::int64_t>(paisePart[0] - '0') * 10;
    } else if (paisePart.size() == 2) {
        paise = std::stoll(paisePart);
    }

    std::int64_t totalPaise = (rupees * 100LL) + paise;
    if (totalPaise <= 0) {
        return false;
    }

    outPaise = totalPaise;
    return true;
}

std::string InputUtils::formatPaise(std::int64_t paise, bool includeCurrencySymbol) {
    bool isNegative = (paise < 0);
    std::uint64_t absPaise = isNegative
        ? static_cast<std::uint64_t>(-(paise + 1)) + 1ULL
        : static_cast<std::uint64_t>(paise);

    std::uint64_t rupees = absPaise / 100ULL;
    std::uint64_t remPaise = absPaise % 100ULL;

    std::string rupeeStr = std::to_string(rupees);
    std::string formattedRupees;
    int count = 0;
    for (int i = static_cast<int>(rupeeStr.size()) - 1; i >= 0; --i) {
        formattedRupees.push_back(rupeeStr[static_cast<std::size_t>(i)]);
        ++count;
        if (count == 3 && i > 0) {
            formattedRupees.push_back(',');
            count = 0;
        }
    }
    std::reverse(formattedRupees.begin(), formattedRupees.end());

    std::ostringstream oss;
    if (isNegative) {
        oss << "-";
    }
    if (includeCurrencySymbol) {
        oss << "\xE2\x82\xB9"; // UTF-8 for ₹
    }
    oss << formattedRupees << '.' << std::setfill('0') << std::setw(2) << remPaise;
    return oss.str();
}

std::string InputUtils::formatPaisePlain(std::int64_t paise) {
    bool isNegative = (paise < 0);
    std::uint64_t absPaise = isNegative
        ? static_cast<std::uint64_t>(-(paise + 1)) + 1ULL
        : static_cast<std::uint64_t>(paise);

    std::ostringstream oss;
    if (isNegative) {
        oss << "-";
    }
    oss << (absPaise / 100ULL) << '.' << std::setfill('0') << std::setw(2) << (absPaise % 100ULL);
    return oss.str();
}

bool InputUtils::parsePositiveInt(const std::string& input, int& outValue) {
    std::string cleaned = trim(input);
    if (cleaned.empty() || cleaned.size() > 9) {
        return false;
    }
    for (char c : cleaned) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    long val = std::stol(cleaned);
    if (val < 0 || val > 2000000000L) {
        return false;
    }
    outValue = static_cast<int>(val);
    return true;
}

std::string InputUtils::readLine(const std::string& prompt) {
    if (!prompt.empty()) {
        std::cout << prompt;
        std::cout.flush();
    }
    std::string line;
    if (!std::getline(std::cin, line)) {
        std::cout << "\n[INFO] End of input detected. Exiting safely.\n";
        std::exit(0);
    }
    return trim(line);
}

int InputUtils::readMenuChoice(const std::string& prompt, int minOption, int maxOption) {
    while (true) {
        std::string input = readLine(prompt);
        int value = 0;
        if (parsePositiveInt(input, value) && value >= minOption && value <= maxOption) {
            return value;
        }
        std::cout << "  [!] Invalid selection. Please enter a number between "
                  << minOption << " and " << maxOption << ".\n";
    }
}

bool InputUtils::readConfirmation(const std::string& prompt) {
    std::string input = toLower(readLine(prompt + " (y/n): "));
    return (input == "y" || input == "yes");
}

} // namespace fintrack