#pragma once

#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "Money.h"
#include "Transaction.h"

enum class AccountType { Savings, Checking };

inline const char* accountTypeName(AccountType t) { return t == AccountType::Savings ? "Savings" : "Checking"; }
inline const char* accountTypeKey(AccountType t) { return t == AccountType::Savings ? "SAVINGS" : "CHECKING"; }
inline bool parseAccountType(const std::string& s, AccountType& out) {
    if (s == "SAVINGS") { out = AccountType::Savings; return true; }
    if (s == "CHECKING") { out = AccountType::Checking; return true; }
    return false;
}

// Base class. Subclasses differ only in how far below zero the balance may go.
class Account {
public:
    Account(int accountNo, int customerId, std::time_t openedAt)
        : accountNo_(accountNo), customerId_(customerId), openedAt_(openedAt) {}
    virtual ~Account() = default;

    int accountNo() const { return accountNo_; }
    int customerId() const { return customerId_; }
    std::time_t openedAt() const { return openedAt_; }
    Money balance() const { return balance_; }
    const std::vector<Transaction>& history() const { return history_; }

    virtual AccountType type() const = 0;
    virtual Money overdraftLimit() const = 0;  // how far below 0 the balance may go

    Money availableToWithdraw() const { return balance_ + overdraftLimit(); }
    bool canDebit(Money amount) const { return amount > 0 && amount <= availableToWithdraw(); }
    bool canCredit(Money amount) const { return amount > 0 && amount <= kMaxAmount && balance_ + amount <= kMaxBalance; }

    // Each returns false and changes nothing if the rules would be broken.
    bool credit(Money amount, TxnType type, int txnId, int counterpart, const std::string& note, std::time_t when);
    bool debit(Money amount, TxnType type, int txnId, int counterpart, const std::string& note, std::time_t when);

    // Newest first, at most n entries.
    std::vector<Transaction> recent(std::size_t n) const;

    // Used when loading from disk: re-applies a stored transaction and checks that
    // its recorded balance matches. Returns false if the data is inconsistent.
    bool replay(const Transaction& t);

private:
    int accountNo_;
    int customerId_;
    std::time_t openedAt_;
    Money balance_ = 0;
    std::vector<Transaction> history_;
};

class SavingsAccount : public Account {
public:
    using Account::Account;
    AccountType type() const override { return AccountType::Savings; }
    Money overdraftLimit() const override { return 0; }  // cannot go below zero
};

class CheckingAccount : public Account {
public:
    using Account::Account;
    AccountType type() const override { return AccountType::Checking; }
    Money overdraftLimit() const override { return 20000; }  // $200.00 overdraft
};

std::unique_ptr<Account> makeAccount(AccountType type, int accountNo, int customerId, std::time_t openedAt);
