#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <ctime>

#include "Csv.h"
#include "Dialogs.h"
#include "LoginPage.h"

namespace {

constexpr int kIdleMinutes = 5;

QString qs(const std::string& s) { return QString::fromStdString(s); }
QString money(Money m) { return qs(formatMoney(m)); }

QString dateText(std::time_t t) {
    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(t)).toString("yyyy-MM-dd HH:mm");
}

QString describe(const Transaction& t) {
    QString d;
    switch (t.type()) {
        case TxnType::TransferOut: d = QObject::tr("To #%1").arg(t.counterpartAccount()); break;
        case TxnType::TransferIn: d = QObject::tr("From #%1").arg(t.counterpartAccount()); break;
        default: return t.note() == txnTypeName(t.type()) ? QString() : qs(t.note());
    }
    if (!t.note().empty()) d += " - " + qs(t.note());
    return d;
}

QTableWidget* makeTable(QWidget* parent, const QStringList& headers) {
    auto* t = new QTableWidget(0, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->hide();
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setAlternatingRowColors(true);
    t->setShowGrid(false);
    t->horizontalHeader()->setStretchLastSection(true);
    t->horizontalHeader()->setHighlightSections(false);
    t->verticalHeader()->setDefaultSectionSize(34);
    return t;
}

QTableWidgetItem* cell(const QString& text, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter,
                       const QColor& color = QColor()) {
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(align);
    if (color.isValid()) item->setForeground(color);
    return item;
}

const QColor kGreen("#15803d");
const QColor kRed("#b91c1c");

}  // namespace

// =========================================================================
//  Construction
// =========================================================================

MainWindow::MainWindow(Bank bank, QWidget* parent) : QMainWindow(parent), m_bank(std::move(bank)) {
    setWindowTitle(tr("CodeAlpha Banking System"));
    resize(1120, 720);
    setMinimumSize(1000, 640);

    m_stack = new QStackedWidget(this);
    m_login = new LoginPage(this);
    m_stack->addWidget(m_login);
    m_stack->addWidget(buildDashboard());
    setCentralWidget(m_stack);
    buildMenus();

    connect(m_login, &LoginPage::loginRequested, this, &MainWindow::onLogin);
    connect(m_login, &LoginPage::registerRequested, this, &MainWindow::onRegister);

    // Automatic log-out after a period without any mouse or keyboard activity.
    m_idleTimer = new QTimer(this);
    m_idleTimer->setObjectName("idleTimer");
    m_idleTimer->setSingleShot(true);
    m_idleTimer->setInterval(kIdleMinutes * 60 * 1000);
    connect(m_idleTimer, &QTimer::timeout, this, &MainWindow::idleTimeout);
    qApp->installEventFilter(this);

    updateEnabledState();
    statusBar()->showMessage(tr("Data file: %1").arg(QDir::toNativeSeparators(qs(m_bank.dataFile()))));
    m_login->reset();
}

void MainWindow::buildMenus() {
    auto make = [this](const QString& text, auto slot, const QKeySequence& key = QKeySequence()) {
        auto* a = new QAction(text, this);
        if (!key.isEmpty()) a->setShortcut(key);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };

    m_actExport = make(tr("&Export statement (this account)…"), [this] { exportStatement(false); },
                       QKeySequence(Qt::CTRL | Qt::Key_E));
    m_actExportAll = make(tr("Export &all accounts…"), [this] { exportStatement(true); },
                          QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    m_actLogout = make(tr("&Log out"), [this] { logout(); }, QKeySequence(Qt::CTRL | Qt::Key_L));
    auto* actQuit = make(tr("E&xit"), [this] { close(); }, QKeySequence::Quit);

    m_actOpen = make(tr("&Open new account…"), [this] { openAccount(); }, QKeySequence(Qt::CTRL | Qt::Key_N));
    m_actDeposit = make(tr("&Deposit…"), [this] { deposit(); }, QKeySequence(Qt::CTRL | Qt::Key_D));
    m_actWithdraw = make(tr("&Withdraw…"), [this] { withdraw(); }, QKeySequence(Qt::CTRL | Qt::Key_W));
    m_actTransfer = make(tr("&Transfer…"), [this] { transfer(); }, QKeySequence(Qt::CTRL | Qt::Key_T));
    m_actChangePin = make(tr("Change &PIN…"), [this] { changePin(); });
    auto* actAbout = make(tr("&About"), [this] { showAbout(); });

    QMenu* file = menuBar()->addMenu(tr("&File"));
    file->addAction(m_actExport);
    file->addAction(m_actExportAll);
    file->addSeparator();
    file->addAction(m_actLogout);
    file->addAction(actQuit);

    QMenu* account = menuBar()->addMenu(tr("&Account"));
    account->addAction(m_actOpen);
    account->addSeparator();
    account->addAction(m_actDeposit);
    account->addAction(m_actWithdraw);
    account->addAction(m_actTransfer);
    account->addSeparator();
    account->addAction(m_actChangePin);

    menuBar()->addMenu(tr("&Help"))->addAction(actAbout);
}

QWidget* MainWindow::buildDashboard() {
    auto* page = new QWidget(this);
    page->setObjectName("dashboard");
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(20, 16, 20, 14);
    root->setSpacing(10);

    // ---- header ----
    auto* header = new QHBoxLayout;
    m_welcome = new QLabel(page);
    m_welcome->setObjectName("welcome");
    auto* pinBtn = new QPushButton(tr("Change PIN"), page);
    auto* logoutBtn = new QPushButton(tr("Log out"), page);
    connect(pinBtn, &QPushButton::clicked, this, &MainWindow::changePin);
    connect(logoutBtn, &QPushButton::clicked, this, &MainWindow::logout);
    header->addWidget(m_welcome, 1);
    header->addWidget(pinBtn);
    header->addWidget(logoutBtn);
    root->addLayout(header);

    auto* body = new QHBoxLayout;
    body->setSpacing(16);

    // ---- left column: accounts + actions ----
    auto* left = new QVBoxLayout;
    auto* accountsBox = new QGroupBox(tr("My accounts"), page);
    auto* accountsLayout = new QVBoxLayout(accountsBox);
    m_accountsTable = makeTable(accountsBox, {tr("Account"), tr("Type"), tr("Balance")});
    m_accountsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_accountsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    connect(m_accountsTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::onSelectionChanged);
    accountsLayout->addWidget(m_accountsTable);
    auto* openBtn = new QPushButton(tr("+  Open new account"), accountsBox);
    connect(openBtn, &QPushButton::clicked, this, &MainWindow::openAccount);
    accountsLayout->addWidget(openBtn);
    left->addWidget(accountsBox, 1);

    auto* actionsBox = new QGroupBox(tr("Make a transaction"), page);
    auto* actionsLayout = new QHBoxLayout(actionsBox);
    m_depositBtn = new QPushButton(tr("Deposit"), actionsBox);
    m_depositBtn->setObjectName("depositButton");
    m_withdrawBtn = new QPushButton(tr("Withdraw"), actionsBox);
    m_withdrawBtn->setObjectName("withdrawButton");
    m_transferBtn = new QPushButton(tr("Transfer"), actionsBox);
    m_transferBtn->setObjectName("transferButton");
    for (QPushButton* b : {m_depositBtn, m_withdrawBtn, m_transferBtn}) {
        b->setCursor(Qt::PointingHandCursor);
        actionsLayout->addWidget(b);
    }
    connect(m_depositBtn, &QPushButton::clicked, this, &MainWindow::deposit);
    connect(m_withdrawBtn, &QPushButton::clicked, this, &MainWindow::withdraw);
    connect(m_transferBtn, &QPushButton::clicked, this, &MainWindow::transfer);
    left->addWidget(actionsBox);

    auto* leftWidget = new QWidget(page);
    leftWidget->setLayout(left);
    leftWidget->setFixedWidth(420);
    left->setContentsMargins(0, 0, 0, 0);
    body->addWidget(leftWidget);

    // ---- right column: details + transactions ----
    auto* right = new QVBoxLayout;
    auto* detailsBox = new QGroupBox(tr("Account details"), page);
    auto* form = new QFormLayout(detailsBox);
    m_dNumber = new QLabel(detailsBox);
    m_dType = new QLabel(detailsBox);
    m_dOpened = new QLabel(detailsBox);
    m_dBalance = new QLabel(detailsBox);
    m_dBalance->setObjectName("balanceBig");
    m_dAvailable = new QLabel(detailsBox);
    m_dOverdraft = new QLabel(detailsBox);
    m_dCount = new QLabel(detailsBox);
    form->addRow(tr("Account number"), m_dNumber);
    form->addRow(tr("Type"), m_dType);
    form->addRow(tr("Opened"), m_dOpened);
    form->addRow(tr("Balance"), m_dBalance);
    form->addRow(tr("Available to withdraw"), m_dAvailable);
    form->addRow(tr("Overdraft limit"), m_dOverdraft);
    form->addRow(tr("Transactions"), m_dCount);
    right->addWidget(detailsBox);

    auto* txnBox = new QGroupBox(tr("Recent transactions"), page);
    auto* txnLayout = new QVBoxLayout(txnBox);
    auto* controls = new QHBoxLayout;
    controls->addWidget(new QLabel(tr("Show:"), txnBox));
    m_countCombo = new QComboBox(txnBox);
    for (int n : {10, 25, 50, 100}) m_countCombo->addItem(tr("last %1").arg(n), n);
    m_countCombo->addItem(tr("all"), 0);
    connect(m_countCombo, &QComboBox::currentIndexChanged, this, [this] { refreshTransactions(); });
    controls->addWidget(m_countCombo);
    controls->addStretch(1);
    m_exportBtn = new QPushButton(tr("Export to CSV (Excel)…"), txnBox);
    connect(m_exportBtn, &QPushButton::clicked, this, [this] { exportStatement(false); });
    controls->addWidget(m_exportBtn);
    txnLayout->addLayout(controls);

    m_txnTable = makeTable(txnBox, {tr("Date"), tr("Type"), tr("Amount"), tr("Balance"), tr("Details")});
    m_txnTable->setColumnWidth(0, 130);
    m_txnTable->setColumnWidth(1, 100);
    m_txnTable->setColumnWidth(2, 110);
    m_txnTable->setColumnWidth(3, 110);
    m_txnTable->setSelectionMode(QAbstractItemView::NoSelection);
    txnLayout->addWidget(m_txnTable);
    right->addWidget(txnBox, 1);

    body->addLayout(right, 1);
    root->addLayout(body, 1);
    return page;
}

// =========================================================================
//  Login, registration, logout
// =========================================================================

bool MainWindow::persist() {
    std::string err;
    if (m_bank.save(err)) return true;
    QMessageBox::critical(
        this, tr("Could not save your data"),
        tr("The change was made, but it could not be written to:\n%1\n\n%2\n\n"
           "Please check the disk space and folder permissions.")
            .arg(QDir::toNativeSeparators(qs(m_bank.dataFile())), qs(err)));
    return false;
}

void MainWindow::onRegister() {
    RegisterDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    std::string err;
    const Customer* c = m_bank.createCustomer(dlg.name(), dlg.phone(), dlg.pin(), err);
    if (!c) {
        QMessageBox::warning(this, tr("Could not create customer"), qs(err));
        return;
    }
    const int id = c->id();
    persist();
    QMessageBox::information(
        this, tr("Welcome!"),
        tr("<h3>Your profile has been created.</h3>"
           "<p>Your customer ID is <b style='font-size:16pt'>%1</b></p>"
           "<p>Write it down: you need this ID <b>and your PIN</b> to log in.</p>")
            .arg(id));
    m_login->clearError();
    m_login->setCustomerId(id);
}

void MainWindow::onLogin(int customerId, const QString& pin) {
    std::string msg;
    AuthResult r = m_bank.authenticate(customerId, pin.toStdString(), msg);

    if (r == AuthResult::NoPin) {  // profile from before PINs existed: choose one now
        PinDialog dlg(false, this);
        if (dlg.exec() != QDialog::Accepted) return;
        std::string err;
        if (!m_bank.setInitialPin(customerId, dlg.newPin(), err)) {
            m_login->showError(qs(err));
            return;
        }
        persist();
        enterDashboard(customerId);
        return;
    }

    std::string ignored;
    m_bank.save(ignored);  // keep the failed-attempt counter on disk, so a restart can't reset it

    if (r != AuthResult::Ok) {
        m_login->showError(qs(msg));
        m_login->clearPin();
        return;
    }
    enterDashboard(customerId);
}

void MainWindow::enterDashboard(int customerId) {
    m_customerId = customerId;
    const Customer* c = m_bank.findCustomer(customerId);
    m_welcome->setText(tr("Welcome, %1  (customer ID %2)").arg(c ? qs(c->name()) : QString()).arg(customerId));
    m_login->reset();
    m_stack->setCurrentIndex(1);
    refreshAll();
    m_idleTimer->start();
    statusBar()->showMessage(tr("Logged in. You will be logged out automatically after %1 minutes of inactivity.")
                                 .arg(kIdleMinutes));
}

void MainWindow::logout() {
    if (m_customerId == 0) return;
    m_customerId = 0;
    m_idleTimer->stop();
    m_accountsTable->setRowCount(0);
    m_txnTable->setRowCount(0);
    m_stack->setCurrentIndex(0);
    m_login->reset();
    updateEnabledState();
    statusBar()->showMessage(tr("You have been logged out."));
}

void MainWindow::idleTimeout() {
    if (m_customerId == 0) return;
    if (QWidget* modal = QApplication::activeModalWidget()) modal->close();
    logout();
    m_login->showError(tr("You were logged out after %1 minutes of inactivity.").arg(kIdleMinutes));
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (m_customerId != 0) {
        switch (event->type()) {
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonDblClick:
            case QEvent::KeyPress:
            case QEvent::Wheel:
                m_idleTimer->start();  // activity: restart the countdown
                break;
            default: break;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::logoutIfLocked() {
    const Customer* c = m_bank.findCustomer(m_customerId);
    if (c && c->lockedUntil() > std::time(nullptr)) {
        logout();
        m_login->showError(tr("Too many incorrect PIN attempts. Access is blocked for a few minutes."));
    }
}

// =========================================================================
//  Showing data
// =========================================================================

int MainWindow::selectedAccountNo() const {
    const int row = m_accountsTable->currentRow();
    if (row < 0 || !m_accountsTable->item(row, 0)) return 0;
    return m_accountsTable->item(row, 0)->data(Qt::UserRole).toInt();
}

const Account* MainWindow::selectedAccount() const {
    const int no = selectedAccountNo();
    return no ? m_bank.findAccount(no) : nullptr;
}

void MainWindow::refreshAll(int selectAccountNo) {
    refreshAccounts(selectAccountNo ? selectAccountNo : selectedAccountNo());
    refreshDetails();
    refreshTransactions();
    updateEnabledState();
}

void MainWindow::refreshAccounts(int selectAccountNo) {
    const auto accounts = m_bank.accountsOf(m_customerId);
    QSignalBlocker blocker(m_accountsTable);
    m_accountsTable->setRowCount(static_cast<int>(accounts.size()));
    int rowToSelect = accounts.empty() ? -1 : 0;
    for (int i = 0; i < static_cast<int>(accounts.size()); ++i) {
        const Account* a = accounts[i];
        auto* number = cell(QString("#%1").arg(a->accountNo()));
        number->setData(Qt::UserRole, a->accountNo());
        m_accountsTable->setItem(i, 0, number);
        m_accountsTable->setItem(i, 1, cell(accountTypeName(a->type())));
        m_accountsTable->setItem(i, 2, cell(money(a->balance()), Qt::AlignRight | Qt::AlignVCenter,
                                            a->balance() < 0 ? kRed : QColor()));
        if (a->accountNo() == selectAccountNo) rowToSelect = i;
    }
    if (rowToSelect >= 0) m_accountsTable->selectRow(rowToSelect);
    else m_accountsTable->setCurrentCell(-1, -1);
}

void MainWindow::refreshDetails() {
    const Account* a = selectedAccount();
    const QString dash = QString::fromUtf8("\xE2\x80\x94");
    if (!a) {
        for (QLabel* l : {m_dNumber, m_dType, m_dOpened, m_dBalance, m_dAvailable, m_dOverdraft, m_dCount})
            l->setText(dash);
        m_dBalance->setStyleSheet(QString());
        return;
    }
    m_dNumber->setText(QString::number(a->accountNo()));
    m_dType->setText(accountTypeName(a->type()));
    m_dOpened->setText(dateText(a->openedAt()));
    m_dBalance->setText(money(a->balance()));
    m_dBalance->setStyleSheet(a->balance() < 0 ? "color: #b91c1c;" : "color: #0f172a;");
    m_dAvailable->setText(money(std::max<Money>(0, a->availableToWithdraw())));
    m_dOverdraft->setText(money(a->overdraftLimit()));
    m_dCount->setText(QString::number(a->history().size()));
}

void MainWindow::refreshTransactions() {
    m_txnTable->setRowCount(0);
    const Account* a = selectedAccount();
    if (!a) return;

    const int n = m_countCombo->currentData().toInt();  // 0 = all
    const auto txns = a->recent(n > 0 ? static_cast<std::size_t>(n) : a->history().size());
    m_txnTable->setRowCount(static_cast<int>(txns.size()));
    for (int i = 0; i < static_cast<int>(txns.size()); ++i) {
        const Transaction& t = txns[i];
        const bool credit = isCredit(t.type());
        const Qt::Alignment right = Qt::AlignRight | Qt::AlignVCenter;
        m_txnTable->setItem(i, 0, cell(dateText(t.timestamp())));
        m_txnTable->setItem(i, 1, cell(txnTypeName(t.type())));
        m_txnTable->setItem(i, 2, cell((credit ? "+" : "-") + money(t.amount()), right, credit ? kGreen : kRed));
        m_txnTable->setItem(i, 3, cell(money(t.balanceAfter()), right));
        m_txnTable->setItem(i, 4, cell(describe(t)));
    }
}

void MainWindow::updateEnabledState() {
    const bool loggedIn = m_customerId != 0;
    const bool hasAccount = loggedIn && selectedAccount() != nullptr;
    for (QAction* a : {m_actOpen, m_actChangePin, m_actLogout}) a->setEnabled(loggedIn);
    for (QAction* a : {m_actDeposit, m_actWithdraw, m_actTransfer, m_actExport, m_actExportAll})
        a->setEnabled(hasAccount);
    for (QPushButton* b : {m_depositBtn, m_withdrawBtn, m_transferBtn, m_exportBtn}) b->setEnabled(hasAccount);
}

void MainWindow::onSelectionChanged() {
    refreshDetails();
    refreshTransactions();
    updateEnabledState();
}

// =========================================================================
//  Operations
// =========================================================================

void MainWindow::openAccount() {
    OpenAccountDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    std::string err;
    const Account* a = m_bank.openAccount(m_customerId, dlg.type(), dlg.openingDeposit(), err);
    if (!a) {
        QMessageBox::warning(this, tr("Could not open account"), qs(err));
        return;
    }
    const int no = a->accountNo();
    persist();
    refreshAll(no);
    statusBar()->showMessage(tr("%1 account #%2 opened.").arg(accountTypeName(a->type())).arg(no));
}

PinVerifier MainWindow::makeVerifier() {
    return [this](const std::string& pin, QString& message) {
        std::string msg;
        const AuthResult r = m_bank.authenticate(m_customerId, pin, msg);
        message = qs(msg);
        std::string ignored;
        m_bank.save(ignored);  // failed attempts must survive a restart
        return r;
    };
}

void MainWindow::runTransactionDialog(int mode) {
    const auto accounts = m_bank.accountsOf(m_customerId);
    if (accounts.empty()) {
        QMessageBox::information(this, tr("No accounts"), tr("Please open an account first."));
        return;
    }
    const auto m = static_cast<TransactionDialog::Mode>(mode);
    TransactionDialog dlg(m, m_bank, accounts, selectedAccountNo(),
                          m == TransactionDialog::Deposit ? PinVerifier() : makeVerifier(), this);
    const int result = dlg.exec();
    logoutIfLocked();  // too many wrong PINs inside the dialog ends the session
    if (m_customerId == 0 || result != QDialog::Accepted) return;

    std::string err;
    bool ok = false;
    QString done;
    switch (m) {
        case TransactionDialog::Deposit:
            ok = m_bank.deposit(dlg.accountNo(), dlg.amount(), err);
            done = tr("Deposited %1 into account #%2.").arg(money(dlg.amount())).arg(dlg.accountNo());
            break;
        case TransactionDialog::Withdraw:
            ok = m_bank.withdraw(dlg.accountNo(), dlg.amount(), err);
            done = tr("Withdrew %1 from account #%2.").arg(money(dlg.amount())).arg(dlg.accountNo());
            break;
        case TransactionDialog::Transfer:
            ok = m_bank.transfer(dlg.accountNo(), dlg.toAccountNo(), dlg.amount(), dlg.note(), err);
            done = tr("Transferred %1 from #%2 to #%3.")
                       .arg(money(dlg.amount())).arg(dlg.accountNo()).arg(dlg.toAccountNo());
            break;
    }
    if (!ok) {
        QMessageBox::warning(this, tr("Could not complete"), qs(err));
        return;
    }
    persist();
    refreshAll(dlg.accountNo());
    statusBar()->showMessage(done);
}

void MainWindow::deposit() { runTransactionDialog(TransactionDialog::Deposit); }
void MainWindow::withdraw() { runTransactionDialog(TransactionDialog::Withdraw); }
void MainWindow::transfer() { runTransactionDialog(TransactionDialog::Transfer); }

void MainWindow::changePin() {
    PinDialog dlg(true, this);
    if (dlg.exec() != QDialog::Accepted) return;
    std::string err;
    const bool ok = m_bank.changePin(m_customerId, dlg.currentPin(), dlg.newPin(), err);
    persist();  // wrong guesses are counted, so save either way
    if (!ok) {
        QMessageBox::warning(this, tr("PIN not changed"), qs(err));
        logoutIfLocked();
        return;
    }
    QMessageBox::information(this, tr("PIN changed"), tr("Your PIN has been changed."));
}

// =========================================================================
//  Export, about
// =========================================================================

void MainWindow::exportStatement(bool allAccounts) {
    std::vector<const Account*> accounts;
    QString defaultName;
    if (allAccounts) {
        accounts = m_bank.accountsOf(m_customerId);
        defaultName = "statement_all_accounts.csv";
    } else if (const Account* a = selectedAccount()) {
        accounts.push_back(a);
        defaultName = QString("statement_%1.csv").arg(a->accountNo());
    }
    if (accounts.empty()) {
        QMessageBox::information(this, tr("Nothing to export"), tr("Select an account first."));
        return;
    }

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString path = QFileDialog::getSaveFileName(this, tr("Export to CSV"), dir + "/" + defaultName,
                                                tr("CSV files for Excel (*.csv);;All files (*)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += ".csv";

    std::string err;
    if (!exportStatementCsv(path.toStdString(), accounts, err)) {
        QMessageBox::critical(this, tr("Could not export"),
                              tr("The CSV file could not be written to\n%1\n\n%2")
                                  .arg(QDir::toNativeSeparators(path), qs(err)));
        return;
    }
    statusBar()->showMessage(tr("Exported to %1").arg(QDir::toNativeSeparators(path)));

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(tr("Export complete"));
    box.setText(tr("Your statement was exported to:\n%1").arg(QDir::toNativeSeparators(path)));
    QPushButton* openBtn = box.addButton(tr("Open file"), QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Close);
    box.exec();
    if (box.clickedButton() == openBtn) QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::showAbout() {
    QMessageBox::about(
        this, tr("About CodeAlpha Banking System"),
        tr("<h3>CodeAlpha Banking System</h3>"
           "<p>Qt 6 Widgets GUI for the C++ banking project.</p>"
           "<p>PINs are stored only as salted hashes. After %1 wrong PINs a profile is locked "
           "for %2 minutes. You are logged out automatically after %3 idle minutes.</p>"
           "<p>Data file:<br><code>%4</code></p>")
            .arg(Bank::kMaxFailedAttempts)
            .arg(Bank::kLockSeconds / 60)
            .arg(kIdleMinutes)
            .arg(QDir::toNativeSeparators(qs(m_bank.dataFile())).toHtmlEscaped()));
}
