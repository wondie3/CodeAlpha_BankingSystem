#include "Dialogs.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

namespace {

QLabel* makeErrorLabel(QWidget* parent) {
    auto* l = new QLabel(parent);
    l->setObjectName("errorLabel");
    l->setWordWrap(true);
    l->hide();
    return l;
}

void showError(QLabel* label, const QString& text) {
    label->setText(text);
    label->setVisible(!text.isEmpty());
}

QLineEdit* makePinEdit(QWidget* parent, const QString& placeholder = QString()) {
    auto* e = new QLineEdit(parent);
    e->setEchoMode(QLineEdit::Password);
    e->setMaxLength(8);
    e->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9]*"), e));
    e->setPlaceholderText(placeholder);
    return e;
}

QLineEdit* makeMoneyEdit(QWidget* parent) {
    auto* e = new QLineEdit(parent);
    e->setPlaceholderText("0.00");
    e->setMaxLength(12);
    e->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9]{0,9}(\\.[0-9]{0,2})?"), e));
    return e;
}

QString money(Money m) { return QString::fromStdString(formatMoney(m)); }

// "Bob O'Neil" -> "Bob O." : enough to confirm the recipient, without exposing full names.
QString maskName(const std::string& full) {
    const QStringList parts = QString::fromStdString(full).split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return QString();
    QString out = parts[0];
    for (int i = 1; i < parts.size(); ++i) out += QString(" %1.").arg(parts[i][0]);
    return out;
}

QDialogButtonBox* makeButtons(QDialog* dlg, const QString& okText) {
    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    box->button(QDialogButtonBox::Ok)->setText(okText);
    box->button(QDialogButtonBox::Ok)->setObjectName("primaryButton");
    box->button(QDialogButtonBox::Ok)->setDefault(true);
    QObject::connect(box, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    return box;
}

}  // namespace

// =========================================================================
//  RegisterDialog
// =========================================================================

RegisterDialog::RegisterDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("New customer"));
    setMinimumWidth(400);

    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("Create your customer profile and choose a PIN. "
                                "You will log in with your customer ID and this PIN."), this);
    intro->setWordWrap(true);
    intro->setObjectName("hint");
    layout->addWidget(intro);

    auto* form = new QFormLayout;
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("e.g. Alice Smith"));
    m_nameEdit->setMaxLength(50);
    m_phoneEdit = new QLineEdit(this);
    m_phoneEdit->setPlaceholderText(tr("e.g. +251 911 223344"));
    m_phoneEdit->setMaxLength(20);
    m_pinEdit = makePinEdit(this, tr("4-8 digits"));
    m_confirmEdit = makePinEdit(this, tr("repeat the PIN"));
    form->addRow(tr("Full name"), m_nameEdit);
    form->addRow(tr("Phone number"), m_phoneEdit);
    form->addRow(tr("PIN"), m_pinEdit);
    form->addRow(tr("Confirm PIN"), m_confirmEdit);
    layout->addLayout(form);

    m_error = makeErrorLabel(this);
    layout->addWidget(m_error);
    layout->addWidget(makeButtons(this, tr("Create customer")));
}

void RegisterDialog::accept() {
    std::string name = m_nameEdit->text().toStdString();
    std::string phone = m_phoneEdit->text().toStdString();
    const std::string pin = m_pinEdit->text().toStdString();
    std::string err;

    if (!Bank::normalizeName(name, err) || !Bank::normalizePhone(phone, err) || !Bank::validatePin(pin, err)) {
        showError(m_error, QString::fromStdString(err));
        return;
    }
    if (m_confirmEdit->text().toStdString() != pin) {
        showError(m_error, tr("The two PINs do not match."));
        return;
    }
    m_name = name;
    m_phone = phone;
    m_pin = pin;
    QDialog::accept();
}

// =========================================================================
//  PinDialog
// =========================================================================

PinDialog::PinDialog(bool askCurrent, QWidget* parent) : QDialog(parent), m_askCurrent(askCurrent) {
    setWindowTitle(askCurrent ? tr("Change PIN") : tr("Set your PIN"));
    setMinimumWidth(380);

    auto* layout = new QVBoxLayout(this);
    if (!askCurrent) {
        auto* intro = new QLabel(tr("Your profile was created before PINs were introduced. "
                                    "Please choose a PIN now. You will need it every time you log in."), this);
        intro->setWordWrap(true);
        intro->setObjectName("hint");
        layout->addWidget(intro);
    }

    auto* form = new QFormLayout;
    if (askCurrent) {
        m_currentEdit = makePinEdit(this);
        form->addRow(tr("Current PIN"), m_currentEdit);
    }
    m_newEdit = makePinEdit(this, tr("4-8 digits"));
    m_confirmEdit = makePinEdit(this, tr("repeat the new PIN"));
    form->addRow(tr("New PIN"), m_newEdit);
    form->addRow(tr("Confirm new PIN"), m_confirmEdit);
    layout->addLayout(form);

    m_error = makeErrorLabel(this);
    layout->addWidget(m_error);
    layout->addWidget(makeButtons(this, askCurrent ? tr("Change PIN") : tr("Save PIN")));
}

void PinDialog::accept() {
    const std::string current = m_askCurrent ? m_currentEdit->text().toStdString() : std::string();
    const std::string next = m_newEdit->text().toStdString();
    std::string err;

    if (m_askCurrent && current.empty()) {
        showError(m_error, tr("Enter your current PIN."));
        return;
    }
    if (!Bank::validatePin(next, err)) {
        showError(m_error, QString::fromStdString(err));
        return;
    }
    if (m_confirmEdit->text().toStdString() != next) {
        showError(m_error, tr("The two new PINs do not match."));
        return;
    }
    m_current = current;
    m_new = next;
    QDialog::accept();
}

// =========================================================================
//  OpenAccountDialog
// =========================================================================

OpenAccountDialog::OpenAccountDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Open a new account"));
    setMinimumWidth(400);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    m_type = new QComboBox(this);
    m_type->addItem(tr("Savings - balance can never go below zero"), int(AccountType::Savings));
    m_type->addItem(tr("Checking - overdraft up to %1").arg(money(20000)), int(AccountType::Checking));
    m_openingEdit = makeMoneyEdit(this);
    m_openingEdit->setPlaceholderText(tr("optional, e.g. 100.00"));
    form->addRow(tr("Account type"), m_type);
    form->addRow(tr("Opening deposit"), m_openingEdit);
    layout->addLayout(form);

    m_error = makeErrorLabel(this);
    layout->addWidget(m_error);
    layout->addWidget(makeButtons(this, tr("Open account")));
}

AccountType OpenAccountDialog::type() const {
    return static_cast<AccountType>(m_type->currentData().toInt());
}

void OpenAccountDialog::accept() {
    const QString text = m_openingEdit->text().trimmed();
    m_opening = 0;
    if (!text.isEmpty() && !parseMoney(text.toStdString(), m_opening)) {
        showError(m_error, tr("Enter a valid amount, for example 100 or 100.50 (or leave it empty)."));
        return;
    }
    QDialog::accept();
}

// =========================================================================
//  TransactionDialog
// =========================================================================

TransactionDialog::TransactionDialog(Mode mode, const Bank& bank, std::vector<const Account*> ownAccounts,
                                     int selectedAccountNo, PinVerifier verifier, QWidget* parent)
    : QDialog(parent), m_mode(mode), m_bank(bank), m_own(std::move(ownAccounts)), m_verify(std::move(verifier)) {
    const QString title = mode == Deposit ? tr("Deposit") : mode == Withdraw ? tr("Withdraw") : tr("Transfer funds");
    setWindowTitle(title);
    setMinimumWidth(430);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    m_accountCombo = new QComboBox(this);
    for (const Account* a : m_own) {
        m_accountCombo->addItem(tr("%1 #%2  (%3)").arg(accountTypeName(a->type())).arg(a->accountNo()).arg(money(a->balance())),
                                a->accountNo());
        if (a->accountNo() == selectedAccountNo) m_accountCombo->setCurrentIndex(m_accountCombo->count() - 1);
    }
    form->addRow(mode == Transfer ? tr("From account") : tr("Account"), m_accountCombo);

    m_info = new QLabel(this);
    m_info->setObjectName("hint");
    form->addRow(QString(), m_info);

    if (mode == Transfer) {
        m_toEdit = new QLineEdit(this);
        m_toEdit->setPlaceholderText(tr("recipient's account number"));
        m_toEdit->setMaxLength(10);
        m_toEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9]*"), m_toEdit));
        form->addRow(tr("To account #"), m_toEdit);
        m_recipient = new QLabel(this);
        m_recipient->setObjectName("hint");
        form->addRow(QString(), m_recipient);
    }

    m_amountEdit = makeMoneyEdit(this);
    form->addRow(tr("Amount"), m_amountEdit);

    if (mode == Transfer) {
        m_noteEdit = new QLineEdit(this);
        m_noteEdit->setMaxLength(40);
        m_noteEdit->setPlaceholderText(tr("optional"));
        form->addRow(tr("Note"), m_noteEdit);
    }
    if (mode != Deposit) {
        m_pinEdit = makePinEdit(this, tr("confirm with your PIN"));
        form->addRow(tr("PIN"), m_pinEdit);
    }
    layout->addLayout(form);

    m_error = makeErrorLabel(this);
    layout->addWidget(m_error);
    layout->addWidget(makeButtons(this, title));

    connect(m_accountCombo, &QComboBox::currentIndexChanged, this, &TransactionDialog::updateInfo);
    if (m_toEdit) connect(m_toEdit, &QLineEdit::textChanged, this, &TransactionDialog::updateInfo);
    updateInfo();
}

const Account* TransactionDialog::currentAccount() const {
    const int i = m_accountCombo->currentIndex();
    return i >= 0 && i < int(m_own.size()) ? m_own[i] : nullptr;
}

void TransactionDialog::updateInfo() {
    if (const Account* a = currentAccount()) {
        QString text = tr("Balance: %1").arg(money(a->balance()));
        if (m_mode != Deposit)
            text += tr("   |   Available: %1").arg(money(std::max<Money>(0, a->availableToWithdraw())));
        m_info->setText(text);
    }
    if (m_recipient) {
        const QString text = m_toEdit->text();
        QString msg;
        if (!text.isEmpty()) {
            const Account* to = m_bank.findAccount(text.toInt());
            const Account* from = currentAccount();
            if (!to) msg = tr("Account not found.");
            else if (from && to->accountNo() == from->accountNo()) msg = tr("That is the account you are sending from.");
            else {
                const Customer* owner = m_bank.findCustomer(to->customerId());
                msg = tr("Recipient: %1 (%2 account)").arg(owner ? maskName(owner->name()) : tr("?"),
                                                          accountTypeName(to->type()));
            }
        }
        m_recipient->setText(msg);
    }
}

void TransactionDialog::accept() {
    showError(m_error, QString());

    Money amount = 0;
    if (!parseMoney(m_amountEdit->text().toStdString(), amount)) {
        showError(m_error, tr("Enter a valid amount, for example 25 or 25.50."));
        return;
    }
    const Account* from = currentAccount();
    if (!from) return;

    if (m_mode == Deposit && !from->canCredit(amount)) {
        showError(m_error, tr("This deposit would exceed the maximum allowed balance."));
        return;
    }
    if (m_mode != Deposit && !from->canDebit(amount)) {
        showError(m_error, tr("Insufficient funds. Available: %1.")
                               .arg(money(std::max<Money>(0, from->availableToWithdraw()))));
        return;
    }

    int toNo = 0;
    if (m_mode == Transfer) {
        bool ok = false;
        toNo = m_toEdit->text().toInt(&ok);
        const Account* to = ok ? m_bank.findAccount(toNo) : nullptr;
        if (!to) {
            showError(m_error, tr("Enter an existing destination account number."));
            return;
        }
        if (to->accountNo() == from->accountNo()) {
            showError(m_error, tr("You cannot transfer to the same account."));
            return;
        }
        if (!to->canCredit(amount)) {
            showError(m_error, tr("The destination account cannot accept this amount."));
            return;
        }
        const Customer* owner = m_bank.findCustomer(to->customerId());
        const auto answer = QMessageBox::question(
            this, tr("Confirm transfer"),
            tr("Send %1 to %2 (account #%3)?").arg(money(amount), owner ? maskName(owner->name()) : tr("?")).arg(toNo));
        if (answer != QMessageBox::Yes) return;
    }

    if (m_mode != Deposit) {
        const std::string pin = m_pinEdit->text().toStdString();
        if (pin.empty()) {
            showError(m_error, tr("Enter your PIN to confirm."));
            return;
        }
        QString message;
        const AuthResult r = m_verify(pin, message);
        if (r == AuthResult::BadCredentials) {
            showError(m_error, tr("Incorrect PIN."));
            m_pinEdit->clear();
            m_pinEdit->setFocus();
            return;
        }
        if (r != AuthResult::Ok) {  // locked out
            QMessageBox::warning(this, tr("Access blocked"), message);
            reject();
            return;
        }
    }

    m_accountNo = from->accountNo();
    m_amount = amount;
    m_toAccountNo = toNo;
    m_note = m_noteEdit ? m_noteEdit->text().toStdString() : std::string();
    QDialog::accept();
}
