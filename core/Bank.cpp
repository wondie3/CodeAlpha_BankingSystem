#include "Bank.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

#include "Crypto.h"
#include "FileUtil.h"

namespace {

std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Splits on '|' and keeps empty fields.
std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : s) {
        if (c == '|') { parts.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    parts.push_back(cur);
    return parts;
}

bool toLL(const std::string& s, long long& out) {
    if (s.empty()) return false;
    try {
        std::size_t pos = 0;
        out = std::stoll(s, &pos);
        return pos == s.size();
    } catch (...) {
        return false;
    }
}

bool toId(const std::string& s, int& out) {
    long long v = 0;
    if (!toLL(s, v) || v < 1 || v > 2'000'000'000LL) return false;
    out = static_cast<int>(v);
    return true;
}

// Keeps notes safe for the data file: no '|' and no control characters.
std::string sanitizeNote(const std::string& raw) {
    std::string s = trim(raw);
    for (char& c : s)
        if (c == '|' || static_cast<unsigned char>(c) < 32) c = ' ';
    if (s.size() > 40) s.resize(40);
    return s;
}

std::string lockedMessage(long long seconds) {
    const long long minutes = std::max<long long>(1, (seconds + 59) / 60);
    return "Too many failed attempts. Try again in " + std::to_string(minutes) +
           (minutes == 1 ? " minute." : " minutes.");
}

const char* const kBadLogin = "Incorrect customer ID or PIN.";

}  // namespace

// =========================================================================
//  Input rules
// =========================================================================

bool Bank::normalizeName(std::string& name, std::string& error) {
    name = trim(name);
    if (name.size() < 2 || name.size() > 50) {
        error = "Name must be 2-50 characters long.";
        return false;
    }
    for (unsigned char c : name) {
        if (!(std::isalpha(c) || c >= 0x80 || c == ' ' || c == '-' || c == '\'' || c == '.')) {
            error = "Name may contain only letters, spaces, hyphens, apostrophes and dots.";
            return false;
        }
    }
    return true;
}

bool Bank::normalizePhone(std::string& phone, std::string& error) {
    std::string out;
    for (char c : trim(phone))
        if (c != ' ' && c != '-') out.push_back(c);
    std::size_t digits = 0;
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (i == 0 && out[i] == '+') continue;
        if (!std::isdigit(static_cast<unsigned char>(out[i]))) {
            error = "Phone number may contain only digits (and a leading +).";
            return false;
        }
        ++digits;
    }
    if (digits < 7 || digits > 15) {
        error = "Phone number must have 7-15 digits.";
        return false;
    }
    phone = out;
    return true;
}

// A PIN is 4-8 digits. Obvious choices (0000, 1234, 4321 ...) are refused.
bool Bank::validatePin(const std::string& pin, std::string& error) {
    if (pin.size() < 4 || pin.size() > 8) {
        error = "PIN must be 4-8 digits.";
        return false;
    }
    for (unsigned char c : pin) {
        if (!std::isdigit(c)) {
            error = "PIN may contain digits only.";
            return false;
        }
    }
    bool same = true, up = true, down = true;
    for (std::size_t i = 1; i < pin.size(); ++i) {
        if (pin[i] != pin[0]) same = false;
        if (pin[i] != pin[i - 1] + 1) up = false;
        if (pin[i] != pin[i - 1] - 1) down = false;
    }
    if (same || up || down) {
        error = "That PIN is too easy to guess (like 0000 or 1234). Choose another.";
        return false;
    }
    return true;
}

// =========================================================================
//  Customers, PINs, accounts
// =========================================================================

const Customer* Bank::createCustomer(std::string name, std::string phone, const std::string& pin,
                                     std::string& error) {
    if (!normalizeName(name, error) || !normalizePhone(phone, error) || !validatePin(pin, error))
        return nullptr;
    for (const auto& [id, c] : customers_) {
        if (c.phone() == phone) {
            error = "A customer with this phone number already exists.";
            return nullptr;
        }
    }
    const int id = nextCustomerId_++;
    auto it = customers_.emplace(id, Customer(id, name, phone, std::time(nullptr))).first;
    const std::string salt = crypto::randomSalt();
    it->second.setPinHash(salt, crypto::hashSecret(pin, salt));
    return &it->second;
}

AuthResult Bank::checkPin(Customer& c, const std::string& pin, std::string& message) {
    const std::time_t now = std::time(nullptr);
    if (c.lockedUntil() > now) {
        message = lockedMessage(c.lockedUntil() - now);
        return AuthResult::Locked;
    }
    if (!c.hasPin()) {
        message = "No PIN has been set for this customer yet.";
        return AuthResult::NoPin;
    }
    if (crypto::constantTimeEquals(crypto::hashSecret(pin, c.pinSalt()), c.pinHash())) {
        c.setFailedAttempts(0);
        c.setLockedUntil(0);
        return AuthResult::Ok;
    }
    const int failed = c.failedAttempts() + 1;
    if (failed >= kMaxFailedAttempts) {
        c.setFailedAttempts(0);
        c.setLockedUntil(now + kLockSeconds);
        message = lockedMessage(kLockSeconds);
        return AuthResult::Locked;
    }
    c.setFailedAttempts(failed);
    message = kBadLogin;
    return AuthResult::BadCredentials;
}

AuthResult Bank::authenticate(int customerId, const std::string& pin, std::string& message) {
    Customer* c = mutableCustomer(customerId);
    if (!c) {
        // Do the same amount of work as for a real customer so timing reveals nothing.
        crypto::hashSecret(pin, "00000000000000000000000000000000");
        message = kBadLogin;
        return AuthResult::BadCredentials;
    }
    return checkPin(*c, pin, message);
}

bool Bank::setInitialPin(int customerId, const std::string& pin, std::string& error) {
    Customer* c = mutableCustomer(customerId);
    if (!c) { error = "Customer not found."; return false; }
    if (c->hasPin()) { error = "This customer already has a PIN."; return false; }
    if (!validatePin(pin, error)) return false;
    const std::string salt = crypto::randomSalt();
    c->setPinHash(salt, crypto::hashSecret(pin, salt));
    return true;
}

bool Bank::changePin(int customerId, const std::string& currentPin, const std::string& newPin,
                     std::string& error) {
    Customer* c = mutableCustomer(customerId);
    if (!c) { error = "Customer not found."; return false; }
    std::string msg;
    const AuthResult r = checkPin(*c, currentPin, msg);  // wrong guesses count here too
    if (r != AuthResult::Ok) {
        error = r == AuthResult::BadCredentials ? "The current PIN is incorrect." : msg;
        return false;
    }
    if (!validatePin(newPin, error)) return false;
    if (newPin == currentPin) {
        error = "The new PIN must be different from the current one.";
        return false;
    }
    const std::string salt = crypto::randomSalt();
    c->setPinHash(salt, crypto::hashSecret(newPin, salt));
    return true;
}

const Account* Bank::openAccount(int customerId, AccountType type, Money openingDeposit, std::string& error) {
    auto cit = customers_.find(customerId);
    if (cit == customers_.end()) {
        error = "Customer not found.";
        return nullptr;
    }
    if (openingDeposit < 0 || openingDeposit > kMaxAmount) {
        error = "Invalid opening deposit.";
        return nullptr;
    }
    const std::time_t now = std::time(nullptr);
    const int no = nextAccountNo_++;
    auto account = makeAccount(type, no, customerId, now);
    if (openingDeposit > 0)
        account->credit(openingDeposit, TxnType::Deposit, nextTxnId_++, 0, "Opening deposit", now);
    cit->second.addAccount(no);
    Account* raw = account.get();
    accounts_[no] = std::move(account);
    return raw;
}

// =========================================================================
//  Money movement
// =========================================================================

bool Bank::deposit(int accountNo, Money amount, std::string& error) {
    Account* a = mutableAccount(accountNo);
    if (!a) { error = "Account not found."; return false; }
    if (amount <= 0 || amount > kMaxAmount) { error = "Invalid amount."; return false; }
    if (!a->credit(amount, TxnType::Deposit, nextTxnId_, 0, "Deposit", std::time(nullptr))) {
        error = "This deposit would exceed the maximum allowed balance.";
        return false;
    }
    ++nextTxnId_;
    return true;
}

bool Bank::withdraw(int accountNo, Money amount, std::string& error) {
    Account* a = mutableAccount(accountNo);
    if (!a) { error = "Account not found."; return false; }
    if (amount <= 0 || amount > kMaxAmount) { error = "Invalid amount."; return false; }
    if (!a->debit(amount, TxnType::Withdrawal, nextTxnId_, 0, "Withdrawal", std::time(nullptr))) {
        error = "Insufficient funds. Available to withdraw: " +
                formatMoney(std::max<Money>(0, a->availableToWithdraw())) + ".";
        return false;
    }
    ++nextTxnId_;
    return true;
}

bool Bank::transfer(int fromAccount, int toAccount, Money amount, const std::string& note, std::string& error) {
    Account* from = mutableAccount(fromAccount);
    Account* to = mutableAccount(toAccount);
    if (!from) { error = "Source account not found."; return false; }
    if (!to) { error = "Destination account not found."; return false; }
    if (from == to) { error = "You cannot transfer to the same account."; return false; }
    if (amount <= 0 || amount > kMaxAmount) { error = "Invalid amount."; return false; }

    // Check everything first, so the transfer either fully happens or not at all.
    if (!from->canDebit(amount)) {
        error = "Insufficient funds. Available to transfer: " +
                formatMoney(std::max<Money>(0, from->availableToWithdraw())) + ".";
        return false;
    }
    if (!to->canCredit(amount)) {
        error = "The destination account cannot accept this amount (balance limit).";
        return false;
    }

    const std::string cleanNote = sanitizeNote(note);
    const std::time_t now = std::time(nullptr);
    from->debit(amount, TxnType::TransferOut, nextTxnId_++, toAccount, cleanNote, now);
    to->credit(amount, TxnType::TransferIn, nextTxnId_++, fromAccount, cleanNote, now);
    return true;
}

// =========================================================================
//  Queries
// =========================================================================

Account* Bank::mutableAccount(int accountNo) {
    auto it = accounts_.find(accountNo);
    return it == accounts_.end() ? nullptr : it->second.get();
}

Customer* Bank::mutableCustomer(int id) {
    auto it = customers_.find(id);
    return it == customers_.end() ? nullptr : &it->second;
}

const Customer* Bank::findCustomer(int id) const {
    auto it = customers_.find(id);
    return it == customers_.end() ? nullptr : &it->second;
}

const Account* Bank::findAccount(int accountNo) const {
    auto it = accounts_.find(accountNo);
    return it == accounts_.end() ? nullptr : it->second.get();
}

std::vector<const Customer*> Bank::customers() const {
    std::vector<const Customer*> out;
    for (const auto& [id, c] : customers_) out.push_back(&c);
    return out;
}

std::vector<const Account*> Bank::accountsOf(int customerId) const {
    std::vector<const Account*> out;
    const Customer* c = findCustomer(customerId);
    if (!c) return out;
    for (int no : c->accountNumbers())
        if (const Account* a = findAccount(no)) out.push_back(a);
    return out;
}

// =========================================================================
//  Persistence
//
//  BANKDATA 2
//  CUSTOMER|id|name|phone|createdAt|pinSalt|pinHash|failedAttempts|lockedUntil
//  ACCOUNT|accountNo|customerId|SAVINGS or CHECKING|balanceCents|openedAt
//  TXN|id|accountNo|type|amountCents|balanceAfterCents|timestamp|counterpart|note
//
//  (Version 1 files - customers without PIN fields - still load; those customers
//   are asked to set a PIN the first time they log in.)
// =========================================================================

bool Bank::save(std::string& error) const {
    std::ostringstream out;
    out << "BANKDATA 2\n";
    for (const auto& [id, c] : customers_)
        out << "CUSTOMER|" << id << '|' << c.name() << '|' << c.phone() << '|'
            << static_cast<long long>(c.createdAt()) << '|' << c.pinSalt() << '|' << c.pinHash() << '|'
            << c.failedAttempts() << '|' << static_cast<long long>(c.lockedUntil()) << '\n';
    for (const auto& [no, a] : accounts_)
        out << "ACCOUNT|" << no << '|' << a->customerId() << '|' << accountTypeKey(a->type()) << '|'
            << a->balance() << '|' << static_cast<long long>(a->openedAt()) << '\n';
    for (const auto& [no, a] : accounts_)
        for (const Transaction& t : a->history())
            out << "TXN|" << t.id() << '|' << t.accountNo() << '|' << txnTypeKey(t.type()) << '|'
                << t.amount() << '|' << t.balanceAfter() << '|' << static_cast<long long>(t.timestamp())
                << '|' << t.counterpartAccount() << '|' << t.note() << '\n';
    return writeFileAtomically(dataFile_, out.str(), error);
}

bool Bank::load(std::string& error) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(dataFile_, ec)) return true;  // first run: nothing to load

    if (fs::file_size(dataFile_, ec) > 50ULL * 1024 * 1024) {
        error = "Data file is unexpectedly large.";
        return false;
    }
    std::ifstream in(dataFile_);
    if (!in) {
        error = "Cannot open " + dataFile_;
        return false;
    }

    Bank fresh(dataFile_);
    if (!fresh.loadFrom(in, error)) return false;
    *this = std::move(fresh);  // only replace our state once everything checked out
    return true;
}

bool Bank::loadFrom(std::istream& in, std::string& error) {
    auto fail = [&](int line, const std::string& msg) {
        error = "Line " + std::to_string(line) + ": " + msg;
        return false;
    };

    std::string line;
    int lineNo = 1;
    int version = 0;
    if (std::getline(in, line)) {
        const std::string header = trim(line);
        if (header == "BANKDATA 1") version = 1;
        else if (header == "BANKDATA 2") version = 2;
    }
    if (version == 0) {
        error = "Not a valid bank data file (bad header).";
        return false;
    }

    std::map<int, Money> expectedBalance;
    std::set<int> txnIds;
    int maxCustomer = 1000, maxAccount = 5000, maxTxn = 0;

    while (std::getline(in, line)) {
        ++lineNo;
        line = trim(line);
        if (line.empty()) continue;
        const auto f = split(line);

        if (f[0] == "CUSTOMER" && f.size() == (version == 1 ? 5u : 9u)) {
            int id = 0;
            long long created = 0;
            std::string name = f[2], phone = f[3], err;
            if (!toId(f[1], id) || !toLL(f[4], created)) return fail(lineNo, "bad number in CUSTOMER record.");
            if (!normalizeName(name, err) || !normalizePhone(phone, err) || name != f[2] || phone != f[3])
                return fail(lineNo, "invalid customer details.");
            if (customers_.count(id)) return fail(lineNo, "duplicate customer ID.");

            Customer c(id, name, phone, static_cast<std::time_t>(created));
            if (version == 2) {
                const std::string& salt = f[5];
                const std::string& hash = f[6];
                long long failed = 0, lockedUntil = 0;
                const bool hasPin = !salt.empty() || !hash.empty();
                if (hasPin && (!crypto::isHex(salt, 32) || !crypto::isHex(hash, 64)))
                    return fail(lineNo, "invalid PIN data.");
                if (!toLL(f[7], failed) || failed < 0 || failed > 1000 || !toLL(f[8], lockedUntil) ||
                    lockedUntil < 0)
                    return fail(lineNo, "invalid lockout data.");
                if (hasPin) c.setPinHash(salt, hash);
                c.setFailedAttempts(static_cast<int>(failed));
                c.setLockedUntil(static_cast<std::time_t>(lockedUntil));
            }
            customers_.emplace(id, std::move(c));
            maxCustomer = std::max(maxCustomer, id);

        } else if (f[0] == "ACCOUNT" && f.size() == 6) {
            int no = 0, custId = 0;
            long long balance = 0, opened = 0;
            AccountType type;
            if (!toId(f[1], no) || !toId(f[2], custId) || !parseAccountType(f[3], type) ||
                !toLL(f[4], balance) || !toLL(f[5], opened))
                return fail(lineNo, "bad ACCOUNT record.");
            if (accounts_.count(no)) return fail(lineNo, "duplicate account number.");
            auto cit = customers_.find(custId);
            if (cit == customers_.end()) return fail(lineNo, "account belongs to an unknown customer.");
            accounts_[no] = makeAccount(type, no, custId, static_cast<std::time_t>(opened));
            cit->second.addAccount(no);
            expectedBalance[no] = balance;
            maxAccount = std::max(maxAccount, no);

        } else if (f[0] == "TXN" && f.size() == 9) {
            int id = 0, accNo = 0, counterpart = 0;
            long long amount = 0, after = 0, ts = 0, cp = 0;
            TxnType type;
            if (!toId(f[1], id) || !toId(f[2], accNo) || !parseTxnType(f[3], type) || !toLL(f[4], amount) ||
                !toLL(f[5], after) || !toLL(f[6], ts) || !toLL(f[7], cp) || cp < 0 || cp > 2'000'000'000LL)
                return fail(lineNo, "bad TXN record.");
            counterpart = static_cast<int>(cp);
            Account* a = mutableAccount(accNo);
            if (!a) return fail(lineNo, "transaction for an unknown account.");
            if (!txnIds.insert(id).second) return fail(lineNo, "duplicate transaction ID.");
            Transaction t(id, accNo, type, amount, after, static_cast<std::time_t>(ts), counterpart, f[8]);
            if (!a->replay(t)) return fail(lineNo, "transaction does not match the account balance.");
            maxTxn = std::max(maxTxn, id);

        } else {
            return fail(lineNo, "unrecognised or malformed record.");
        }
    }

    for (const auto& [no, a] : accounts_) {
        if (a->balance() != expectedBalance[no]) {
            error = "Account " + std::to_string(no) + ": stored balance does not match its transaction history.";
            return false;
        }
    }
    nextCustomerId_ = maxCustomer + 1;
    nextAccountNo_ = maxAccount + 1;
    nextTxnId_ = maxTxn + 1;
    return true;
}
