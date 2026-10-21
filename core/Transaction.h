#pragma once

#include <ctime>
#include <string>
#include <utility>

#include "Money.h"

enum class TxnType { Deposit, Withdrawal, TransferIn, TransferOut };

inline bool isCredit(TxnType t) { return t == TxnType::Deposit || t == TxnType::TransferIn; }

inline const char* txnTypeName(TxnType t) {
    switch (t) {
        case TxnType::Deposit: return "Deposit";
        case TxnType::Withdrawal: return "Withdrawal";
        case TxnType::TransferIn: return "Transfer in";
        case TxnType::TransferOut: return "Transfer out";
    }
    return "?";
}

// Names used in the data file.
inline const char* txnTypeKey(TxnType t) {
    switch (t) {
        case TxnType::Deposit: return "DEPOSIT";
        case TxnType::Withdrawal: return "WITHDRAWAL";
        case TxnType::TransferIn: return "TRANSFER_IN";
        case TxnType::TransferOut: return "TRANSFER_OUT";
    }
    return "?";
}

inline bool parseTxnType(const std::string& s, TxnType& out) {
    for (TxnType t : {TxnType::Deposit, TxnType::Withdrawal, TxnType::TransferIn, TxnType::TransferOut})
        if (s == txnTypeKey(t)) { out = t; return true; }
    return false;
}

// One immutable record in an account's history.
class Transaction {
public:
    Transaction(int id, int accountNo, TxnType type, Money amount, Money balanceAfter,
                std::time_t timestamp, int counterpartAccount, std::string note)
        : id_(id), accountNo_(accountNo), type_(type), amount_(amount),
          balanceAfter_(balanceAfter), timestamp_(timestamp),
          counterpartAccount_(counterpartAccount), note_(std::move(note)) {}

    int id() const { return id_; }
    int accountNo() const { return accountNo_; }
    TxnType type() const { return type_; }
    Money amount() const { return amount_; }
    Money balanceAfter() const { return balanceAfter_; }
    std::time_t timestamp() const { return timestamp_; }
    int counterpartAccount() const { return counterpartAccount_; }  // other side of a transfer, else 0
    const std::string& note() const { return note_; }

private:
    int id_;
    int accountNo_;
    TxnType type_;
    Money amount_;
    Money balanceAfter_;
    std::time_t timestamp_;
    int counterpartAccount_;
    std::string note_;
};
