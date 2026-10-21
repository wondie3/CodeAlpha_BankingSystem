#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;

// The sign-in screen: customer ID + PIN, and a link to create a new customer.
class LoginPage : public QWidget {
    Q_OBJECT
public:
    explicit LoginPage(QWidget* parent = nullptr);

    void showError(const QString& text);
    void clearError();
    void clearPin();
    void setCustomerId(int id);
    void reset();   // clears everything and focuses the ID field

signals:
    void loginRequested(int customerId, const QString& pin);
    void registerRequested();

private:
    void submit();

    QLineEdit* m_idEdit;
    QLineEdit* m_pinEdit;
    QLabel* m_error;
};
