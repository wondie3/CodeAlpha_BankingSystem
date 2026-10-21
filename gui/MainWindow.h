#pragma once

#include <QMainWindow>
#include <QTimer>

#include "Bank.h"
#include "Dialogs.h"   // PinVerifier

class QAction;
class QComboBox;
class QEvent;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class LoginPage;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(Bank bank, QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onLogin(int customerId, const QString& pin);
    void onRegister();
    void logout();
    void idleTimeout();

    void openAccount();
    void deposit();
    void withdraw();
    void transfer();
    void changePin();
    void exportStatement(bool allAccounts);
    void showAbout();
    void onSelectionChanged();

private:
    void buildMenus();
    QWidget* buildDashboard();
    void enterDashboard(int customerId);
    void refreshAll(int selectAccountNo = 0);
    void refreshAccounts(int selectAccountNo);
    void refreshDetails();
    void refreshTransactions();
    void updateEnabledState();
    int selectedAccountNo() const;
    const Account* selectedAccount() const;
    bool persist();                 // saves; shows an error dialog on failure
    void runTransactionDialog(int mode);
    PinVerifier makeVerifier();
    void logoutIfLocked();

    Bank m_bank;
    int m_customerId = 0;           // 0 = nobody logged in
    QTimer* m_idleTimer = nullptr;

    // widgets
    QStackedWidget* m_stack = nullptr;
    LoginPage* m_login = nullptr;
    QLabel* m_welcome = nullptr;
    QTableWidget* m_accountsTable = nullptr;
    QTableWidget* m_txnTable = nullptr;
    QComboBox* m_countCombo = nullptr;
    QLabel *m_dNumber = nullptr, *m_dType = nullptr, *m_dOpened = nullptr, *m_dBalance = nullptr,
           *m_dAvailable = nullptr, *m_dOverdraft = nullptr, *m_dCount = nullptr;
    QPushButton *m_depositBtn = nullptr, *m_withdrawBtn = nullptr, *m_transferBtn = nullptr,
                *m_exportBtn = nullptr;

    // actions
    QAction *m_actOpen = nullptr, *m_actDeposit = nullptr, *m_actWithdraw = nullptr, *m_actTransfer = nullptr,
            *m_actExport = nullptr, *m_actExportAll = nullptr, *m_actChangePin = nullptr, *m_actLogout = nullptr;
};
