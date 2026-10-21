// Money is stored as a whole number of cents, so there are no floating-point
// rounding errors (0.1 + 0.2 style bugs) in balances.
#pragma once

#include <cctype>
#include <string>

using Money = long long;  // cents

constexpr Money kMaxAmount = 99'999'999'999LL;    // largest single operation: $999,999,999.99
constexpr Money kMaxBalance = 999'999'999'999LL;  // largest balance: $9,999,999,999.99

// 123456 -> "$1,234.56"   -500 -> "-$5.00"
inline std::string formatMoney(Money cents) {
    const bool negative = cents < 0;
    const unsigned long long v =
        negative ? 0ULL - static_cast<unsigned long long>(cents) : static_cast<unsigned long long>(cents);
    std::string dollars = std::to_string(v / 100);
    for (int i = static_cast<int>(dollars.size()) - 3; i > 0; i -= 3) dollars.insert(i, ",");
    const int c = static_cast<int>(v % 100);
    return std::string(negative ? "-$" : "$") + dollars + "." + (c < 10 ? "0" : "") + std::to_string(c);
}

// Parses user text such as "25", "25.5", "$25.50". Only positive amounts with
// at most 2 decimals are accepted. Returns false for anything else.
inline bool parseMoney(std::string text, Money& out) {
    const auto b = text.find_first_not_of(" \t");
    const auto e = text.find_last_not_of(" \t");
    if (b == std::string::npos) return false;
    text = text.substr(b, e - b + 1);
    if (text[0] == '$') text.erase(0, 1);
    if (text.empty()) return false;

    const auto dot = text.find('.');
    const std::string whole = text.substr(0, dot);
    const std::string frac = dot == std::string::npos ? "" : text.substr(dot + 1);
    if (whole.empty() && frac.empty()) return false;
    if (whole.size() > 9 || frac.size() > 2) return false;
    for (unsigned char ch : whole) if (!std::isdigit(ch)) return false;
    for (unsigned char ch : frac) if (!std::isdigit(ch)) return false;

    Money cents = (whole.empty() ? 0 : std::stoll(whole)) * 100;
    if (frac.size() == 1) cents += (frac[0] - '0') * 10;
    else if (frac.size() == 2) cents += (frac[0] - '0') * 10 + (frac[1] - '0');

    if (cents <= 0 || cents > kMaxAmount) return false;
    out = cents;
    return true;
}
