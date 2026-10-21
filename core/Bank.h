// The Bank owns all customers and accounts, enforces the business rules,
// authenticates customers by PIN and saves/loads everything to one data file.
// This layer has no GUI code, so it can be unit-tested on its own.
#pragma once

#include <iosfwd>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Account.h"
#include "Customer.h"

enum class AuthResult { Ok, BadCredentials, Locked, NoPin };

class Bank {
public:
    static constexpr int kMaxFailedAttempts = 5;  // wrong PINs before a lockout
    static constexpr int kLockSeconds = 300;      // lockout length (5 minutes)

    explicit Bank(std::string dataFile) : dataFile_(std::move(dataFile)) {}

    // ---- persistence ----
    // Returns true if the file loaded, or if there is no file yet (fresh bank).
    // On failure the bank stays empty and the file is left untouched.
    bool load(std::string& error);
    // Atomic: writes a temp file, then renames it over the real one.
    bool save(std::string& error) const;
    const std::string& dataFile() const { return dataFile_; }

    // ---- customers, PINs and accounts ----
    // Each returns nullptr / false and fills `error` when a rule fails.
    const Customer* createCustomer(std::string name, std::string phone, const std::string& pin,
                                   std::string& error);
    const Account* openAccount(int customerId, AccountType type, Money openingDeposit, std::string& error);

    // Checks a customer ID + PIN. Wrong PINs are counted; after kMaxFailedAttempts the
    // customer is locked for kLockSeconds. The message is safe to show to the user and
    // is identical for "unknown customer" and "wrong PIN". The caller should save()
    // afterwards so failed-attempt counters survive a restart.
    AuthResult authenticate(int customerId, const std::string& pin, std::string& message);
    // For customers loaded from an older data file that has no PIN yet.
    bool setInitialPin(int customerId, const std::string& pin, std::string& error);
    bool changePin(int customerId, const std::string& currentPin, const std::string& newPin,
                   std::string& error);

    // ---- money movement ----
    bool deposit(int accountNo, Money amount, std::string& error);
    bool withdraw(int accountNo, Money amount, std::string& error);
    bool transfer(int fromAccount, int toAccount, Money amount, const std::string& note, std::string& error);

    // ---- queries ----
    const Customer* findCustomer(int id) const;
    const Account* findAccount(int accountNo) const;
    std::vector<const Customer*> customers() const;
    std::vector<const Account*> accountsOf(int customerId) const;

    // ---- input rules (also used by the UI) ----
    static bool normalizeName(std::string& name, std::string& error);
    static bool normalizePhone(std::string& phone, std::string& error);
    static bool validatePin(const std::string& pin, std::string& error);

private:
    bool loadFrom(std::istream& in, std::string& error);
    Account* mutableAccount(int accountNo);
    Customer* mutableCustomer(int id);
    AuthResult checkPin(Customer& c, const std::string& pin, std::string& message);

    std::string dataFile_;
    std::map<int, Customer> customers_;
    std::map<int, std::unique_ptr<Account>> accounts_;
    int nextCustomerId_ = 1001;
    int nextAccountNo_ = 5001;
    int nextTxnId_ = 1;
};
