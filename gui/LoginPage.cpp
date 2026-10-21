#include "LoginPage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

LoginPage::LoginPage(QWidget* parent) : QWidget(parent) {
    setObjectName("loginPage");

    auto* card = new QFrame(this);
    card->setObjectName("card");
    card->setFixedWidth(400);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(32, 30, 32, 26);
    layout->setSpacing(10);

    auto* title = new QLabel(tr("CodeAlpha Bank"), card);
    title->setObjectName("title");
    title->setAlignment(Qt::AlignCenter);
    auto* subtitle = new QLabel(tr("Sign in with your customer ID and PIN"), card);
    subtitle->setObjectName("subtitle");
    subtitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addSpacing(10);

    layout->addWidget(new QLabel(tr("Customer ID"), card));
    m_idEdit = new QLineEdit(card);
    m_idEdit->setPlaceholderText(tr("e.g. 1001"));
    m_idEdit->setMaxLength(10);
    m_idEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9]*"), m_idEdit));
    layout->addWidget(m_idEdit);

    layout->addWidget(new QLabel(tr("PIN"), card));
    m_pinEdit = new QLineEdit(card);
    m_pinEdit->setEchoMode(QLineEdit::Password);
    m_pinEdit->setMaxLength(8);
    m_pinEdit->setPlaceholderText(tr("your PIN"));
    m_pinEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9]*"), m_pinEdit));
    layout->addWidget(m_pinEdit);

    m_error = new QLabel(card);
    m_error->setObjectName("errorLabel");
    m_error->setWordWrap(true);
    m_error->hide();
    layout->addWidget(m_error);

    auto* loginBtn = new QPushButton(tr("Log in"), card);
    loginBtn->setObjectName("primaryButton");
    loginBtn->setDefault(true);
    loginBtn->setCursor(Qt::PointingHandCursor);
    layout->addSpacing(4);
    layout->addWidget(loginBtn);

    auto* registerBtn = new QPushButton(tr("New here? Create a customer profile"), card);
    registerBtn->setObjectName("linkButton");
    registerBtn->setCursor(Qt::PointingHandCursor);
    layout->addWidget(registerBtn);

    connect(loginBtn, &QPushButton::clicked, this, &LoginPage::submit);
    connect(m_pinEdit, &QLineEdit::returnPressed, this, &LoginPage::submit);
    connect(m_idEdit, &QLineEdit::returnPressed, m_pinEdit, qOverload<>(&QWidget::setFocus));
    connect(registerBtn, &QPushButton::clicked, this, &LoginPage::registerRequested);

    // Centre the card in the window.
    auto* outer = new QHBoxLayout(this);
    outer->addStretch(1);
    auto* column = new QVBoxLayout;
    column->addStretch(1);
    column->addWidget(card);
    column->addStretch(1);
    outer->addLayout(column);
    outer->addStretch(1);
}

void LoginPage::submit() {
    clearError();
    bool ok = false;
    const int id = m_idEdit->text().toInt(&ok);
    if (!ok || id <= 0) {
        showError(tr("Enter your customer ID."));
        m_idEdit->setFocus();
        return;
    }
    if (m_pinEdit->text().isEmpty()) {
        showError(tr("Enter your PIN."));
        m_pinEdit->setFocus();
        return;
    }
    emit loginRequested(id, m_pinEdit->text());
}

void LoginPage::showError(const QString& text) {
    m_error->setText(text);
    m_error->setVisible(!text.isEmpty());
}

void LoginPage::clearError() { showError(QString()); }

void LoginPage::clearPin() {
    m_pinEdit->clear();
    m_pinEdit->setFocus();
}

void LoginPage::setCustomerId(int id) {
    m_idEdit->setText(QString::number(id));
    m_pinEdit->setFocus();
}

void LoginPage::reset() {
    m_idEdit->clear();
    m_pinEdit->clear();
    clearError();
    m_idEdit->setFocus();
}
