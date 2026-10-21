#pragma once

#include <ctime>
#include <string>
#include <utility>
#include <vector>

class Customer {
public:
    Customer(int id, std::string name, std::string phone, std::time_t createdAt)
        : id_(id), name_(std::move(name)), phone_(std::move(phone)), createdAt_(createdAt) {}

    int id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& phone() const { return phone_; }
    std::time_t createdAt() const { return createdAt_; }
    const std::vector<int>& accountNumbers() const { return accountNos_; }
    void addAccount(int accountNo) { accountNos_.push_back(accountNo); }

    // ---- PIN (only a salted hash is ever stored, never the PIN itself) ----
    bool hasPin() const { return !pinHash_.empty(); }
    const std::string& pinSalt() const { return pinSalt_; }
    const std::string& pinHash() const { return pinHash_; }
    void setPinHash(std::string salt, std::string hash) {
        pinSalt_ = std::move(salt);
        pinHash_ = std::move(hash);
        failedAttempts_ = 0;
        lockedUntil_ = 0;
    }

    // ---- brute-force protection ----
    int failedAttempts() const { return failedAttempts_; }
    void setFailedAttempts(int n) { failedAttempts_ = n; }
    std::time_t lockedUntil() const { return lockedUntil_; }
    void setLockedUntil(std::time_t t) { lockedUntil_ = t; }

private:
    int id_;
    std::string name_;
    std::string phone_;
    std::time_t createdAt_;
    std::vector<int> accountNos_;
    std::string pinSalt_;
    std::string pinHash_;
    int failedAttempts_ = 0;
    std::time_t lockedUntil_ = 0;
};
