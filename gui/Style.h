// Application-wide Qt style sheet.
#pragma once

inline const char* appStyleSheet() {
    return R"(
        QMainWindow, QDialog, QWidget#dashboard, QWidget#loginPage { background: #f1f5f9; }
        QLabel { color: #1e293b; }
        QLabel#welcome { font-size: 15pt; font-weight: 600; color: #0f172a; }
        QLabel#title { font-size: 20pt; font-weight: 700; color: #0f172a; }
        QLabel#subtitle { color: #64748b; }
        QLabel#errorLabel { color: #b91c1c; background: #fee2e2; border-radius: 6px; padding: 6px 8px; }
        QLabel#hint { color: #64748b; }
        QLabel#balanceBig { font-size: 18pt; font-weight: 700; }
        QFrame#card { background: #ffffff; border: 1px solid #e2e8f0; border-radius: 14px; }
        QGroupBox {
            font-weight: 600; color: #334155; background: #ffffff;
            border: 1px solid #e2e8f0; border-radius: 10px;
            margin-top: 14px; padding: 16px 12px 12px 12px;
        }
        QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 6px; }
        QLineEdit, QComboBox {
            background: #ffffff; color: #0f172a; border: 1px solid #cbd5e1;
            border-radius: 6px; padding: 7px 9px; selection-background-color: #bfdbfe;
        }
        QLineEdit:focus, QComboBox:focus { border: 2px solid #2563eb; padding: 6px 8px; }
        QPushButton {
            background: #ffffff; color: #1e293b; border: 1px solid #cbd5e1;
            border-radius: 6px; padding: 8px 14px;
        }
        QPushButton:hover { background: #f1f5f9; }
        QPushButton:pressed { background: #e2e8f0; }
        QPushButton:disabled { color: #94a3b8; background: #f8fafc; }
        QPushButton#primaryButton {
            background: #2563eb; color: #ffffff; border: none; font-weight: 600; padding: 9px 16px;
        }
        QPushButton#primaryButton:hover { background: #1d4ed8; }
        QPushButton#primaryButton:disabled { background: #93c5fd; }
        QPushButton#depositButton { background: #16a34a; color: #fff; border: none; font-weight: 600; padding: 10px; }
        QPushButton#depositButton:hover { background: #15803d; }
        QPushButton#withdrawButton { background: #ea580c; color: #fff; border: none; font-weight: 600; padding: 10px; }
        QPushButton#withdrawButton:hover { background: #c2410c; }
        QPushButton#transferButton { background: #2563eb; color: #fff; border: none; font-weight: 600; padding: 10px; }
        QPushButton#transferButton:hover { background: #1d4ed8; }
        QPushButton#depositButton:disabled, QPushButton#withdrawButton:disabled,
        QPushButton#transferButton:disabled { background: #cbd5e1; color: #f8fafc; }
        QPushButton#linkButton { border: none; background: transparent; color: #2563eb; padding: 4px; }
        QPushButton#linkButton:hover { text-decoration: underline; background: transparent; }
        QTableWidget {
            background: #ffffff; alternate-background-color: #f8fafc; border: 1px solid #e2e8f0;
            border-radius: 6px; gridline-color: #e2e8f0; selection-background-color: #dbeafe;
            selection-color: #0f172a;
        }
        QHeaderView::section {
            background: #f1f5f9; color: #475569; font-weight: 600; border: none;
            border-bottom: 1px solid #e2e8f0; padding: 6px;
        }
        QStatusBar { color: #475569; }
        QMenuBar { background: #ffffff; }
    )";
}
