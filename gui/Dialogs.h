// The dialogs used by the banking GUI.
#pragma once

#include <QDialog>
#include <functional>
#include <string>
#include <vector>

#include "Bank.h"

class QComboBox;
class QLabel;
class QLineEdit;

// ---- Register a new customer ---------------------------------------------
class RegisterDialog : public QDialog {
    Q_OBJECT
public:
    explicit RegisterDialog(QWidget* parent = nullptr);
    std::string name() const { return m_name; }
    std::string phone() const { return m_phone; }
    std::string pin() const { return m_pin; }
    void accept() override;

private:
    QLineEdit *m_nameEdit, *m_phoneEdit, *m_pinEdit, *m_confirmEdit;
    QLabel* m_error;
    std::string m_name, m_phone, m_pin;
};

// ---- Set or change a PIN ---------------------------------------------------
class PinDialog : public QDialog {
    Q_OBJECT
public:
    // askCurrent = true  -> "change PIN" (asks for the current PIN first)
    // askCurrent = false -> "set PIN" for customers who don't have one yet
    PinDialog(bool askCurrent, QWidget* parent = nullptr);
    std::string currentPin() const { return m_current; }
    std::string newPin() const { return m_new; }
    void accept() override;

private:
    bool m_askCurrent;
    QLineEdit *m_currentEdit = nullptr, *m_newEdit, *m_confirmEdit;
    QLabel* m_error;
    std::string m_current, m_new;
};

// ---- Open a new account -----------------------------------------------------
class OpenAccountDialog : public QDialog {
    Q_OBJECT
public:
    explicit OpenAccountDialog(QWidget* parent = nullptr);
    AccountType type() const;
    Money openingDeposit() const { return m_opening; }
    void accept() override;

private:
    QComboBox* m_type;
    QLineEdit* m_openingEdit;
    QLabel* m_error;
    Money m_opening = 0;
};

// ---- Deposit / withdraw / transfer ------------------------------------------
// Verifies the customer's PIN. Returns the AuthResult and a message for the user.
using PinVerifier = std::function<AuthResult(const std::string& pin, QString& message)>;

class TransactionDialog : public QDialog {
    Q_OBJECT
public:
    enum Mode { Deposit, Withdraw, Transfer };

    TransactionDialog(Mode mode, const Bank& bank, std::vector<const Account*> ownAccounts,
                      int selectedAccountNo, PinVerifier verifier, QWidget* parent = nullptr);

    int accountNo() const { return m_accountNo; }
    Money amount() const { return m_amount; }
    int toAccountNo() const { return m_toAccountNo; }
    std::string note() const { return m_note; }
    void accept() override;

private slots:
    void updateInfo();

private:
    const Account* currentAccount() const;

    Mode m_mode;
    const Bank& m_bank;
    std::vector<const Account*> m_own;
    PinVerifier m_verify;

    QComboBox* m_accountCombo;
    QLabel* m_info;
    QLineEdit *m_toEdit = nullptr, *m_amountEdit, *m_noteEdit = nullptr, *m_pinEdit = nullptr;
    QLabel* m_recipient = nullptr;
    QLabel* m_error;

    int m_accountNo = 0;
    int m_toAccountNo = 0;
    Money m_amount = 0;
    std::string m_note;
};
