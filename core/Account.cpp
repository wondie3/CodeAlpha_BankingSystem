#include "Account.h"

bool Account::credit(Money amount, TxnType type, int txnId, int counterpart,
                     const std::string& note, std::time_t when) {
    if (!canCredit(amount)) return false;
    balance_ += amount;
    history_.emplace_back(txnId, accountNo_, type, amount, balance_, when, counterpart, note);
    return true;
}

bool Account::debit(Money amount, TxnType type, int txnId, int counterpart,
                    const std::string& note, std::time_t when) {
    if (!canDebit(amount)) return false;
    balance_ -= amount;
    history_.emplace_back(txnId, accountNo_, type, amount, balance_, when, counterpart, note);
    return true;
}

std::vector<Transaction> Account::recent(std::size_t n) const {
    std::vector<Transaction> out;
    for (auto it = history_.rbegin(); it != history_.rend() && out.size() < n; ++it) out.push_back(*it);
    return out;
}

bool Account::replay(const Transaction& t) {
    if (t.accountNo() != accountNo_ || t.amount() <= 0 || t.amount() > kMaxAmount) return false;
    const Money next = balance_ + (isCredit(t.type()) ? t.amount() : -t.amount());
    if (next != t.balanceAfter() || next < -overdraftLimit() || next > kMaxBalance) return false;
    balance_ = next;
    history_.push_back(t);
    return true;
}

std::unique_ptr<Account> makeAccount(AccountType type, int accountNo, int customerId, std::time_t openedAt) {
    if (type == AccountType::Savings) return std::make_unique<SavingsAccount>(accountNo, customerId, openedAt);
    return std::make_unique<CheckingAccount>(accountNo, customerId, openedAt);
}
