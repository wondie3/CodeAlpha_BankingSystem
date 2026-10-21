#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>

#include "Bank.h"
#include "MainWindow.h"
#include "Style.h"

// Usage: BankingGUI [path/to/bank_data.txt]
// Without an argument the data file lives in the user's application-data folder.
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("CodeAlpha");
    QApplication::setApplicationName("Banking System");
    QApplication::setStyle("Fusion");  // identical look on Windows, macOS and Linux
    app.setStyleSheet(appStyleSheet());

    const QStringList args = QApplication::arguments();
    QString path = args.size() > 1
                       ? args[1]
                       : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/bank_data.txt";
    path = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    QDir().mkpath(QFileInfo(path).absolutePath());

    Bank bank(path.toStdString());
    std::string error;
    if (!bank.load(error)) {
        QMessageBox::critical(nullptr, QObject::tr("Could not load the bank data"),
                              QObject::tr("The data file could not be loaded:\n%1\n\n%2\n\n"
                                          "The file has NOT been changed. Fix or rename it, then start the "
                                          "program again.")
                                  .arg(QDir::toNativeSeparators(path), QString::fromStdString(error)));
        return 1;
    }

    MainWindow window(std::move(bank));
    window.show();
    return app.exec();
}
