#include "Csv.h"

#include <ctime>

#include "FileUtil.h"

namespace {

std::string timestampText(std::time_t t) {
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tmv);
    return buf;
}

}  // namespace

std::string csvField(const std::string& text) {
    std::string s = text;
    if (!s.empty() && (s[0] == '=' || s[0] == '+' || s[0] == '-' || s[0] == '@' || s[0] == '\t' || s[0] == '\r'))
        s.insert(s.begin(), '\'');
    if (s.find_first_of(",\"\r\n") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    out += '"';
    return out;
}

std::string plainAmount(Money cents) {
    const bool neg = cents < 0;
    const unsigned long long v =
        neg ? 0ULL - static_cast<unsigned long long>(cents) : static_cast<unsigned long long>(cents);
    const int c = static_cast<int>(v % 100);
    return std::string(neg ? "-" : "") + std::to_string(v / 100) + "." + (c < 10 ? "0" : "") + std::to_string(c);
}

std::string buildStatementCsv(const std::vector<const Account*>& accounts) {
    std::string csv = "\xEF\xBB\xBF";  // UTF-8 BOM so Excel detects the encoding
    csv += "Account,Account Type,Transaction ID,Date,Type,Amount,Balance After,Other Account,Note\r\n";
    for (const Account* a : accounts) {
        for (const Transaction& t : a->history()) {
            const Money signedAmount = isCredit(t.type()) ? t.amount() : -t.amount();
            csv += std::to_string(a->accountNo()) + ',' + accountTypeName(a->type()) + ',' +
                   std::to_string(t.id()) + ',' + timestampText(t.timestamp()) + ',' + txnTypeName(t.type()) +
                   ',' + plainAmount(signedAmount) + ',' + plainAmount(t.balanceAfter()) + ',' +
                   (t.counterpartAccount() > 0 ? std::to_string(t.counterpartAccount()) : "") + ',' +
                   csvField(t.note()) + "\r\n";
        }
    }
    return csv;
}

bool exportStatementCsv(const std::string& path, const std::vector<const Account*>& accounts,
                        std::string& error) {
    return writeFileAtomically(path, buildStatementCsv(accounts), error);
}
