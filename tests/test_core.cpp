// Self-checking tests for the banking core (no Qt, no framework needed).
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Bank.h"
#include "Crypto.h"
#include "Csv.h"

static int failures = 0;
#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) { ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } \
    } while (0)

static std::string tempFile(const char* name) {
    return (std::filesystem::temp_directory_path() / name).string();
}
static std::string readAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main() {
    std::string err;

    // ---- SHA-256 known answers ----
    CHECK(crypto::toHex(crypto::sha256("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(crypto::toHex(crypto::sha256("")) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(crypto::randomSalt() != crypto::randomSalt());
    CHECK(crypto::hashSecret("4821", "aa") != crypto::hashSecret("4821", "bb"));

    // ---- PIN rules ----
    for (const char* bad : {"123", "123456789", "12a4", "0000", "7777", "1234", "4321", "56789", ""})
        CHECK(!Bank::validatePin(bad, err));
    for (const char* good : {"4821", "1357", "90210", "246801"})
        CHECK(Bank::validatePin(good, err));

    // ---- customers need a valid PIN ----
    const std::string path = tempFile("bank_gui_test_a.txt");
    std::filesystem::remove(path);
    Bank bank(path);
    CHECK(bank.createCustomer("Alice Smith", "0911223344", "1234", err) == nullptr);   // weak PIN
    CHECK(bank.createCustomer("A", "0911223344", "4821", err) == nullptr);              // bad name
    const Customer* alice = bank.createCustomer("Alice Smith", "+251 911-223344", "4821", err);
    CHECK(alice && alice->id() == 1001 && alice->hasPin());
    CHECK(alice->pinHash().size() == 64 && alice->pinHash() != "4821");
    CHECK(bank.createCustomer("Other", "+251911223344", "4821", err) == nullptr);       // duplicate phone
    const Customer* bob = bank.createCustomer("Bob O'Neil", "0922334455", "9013", err);
    CHECK(bob && bob->id() == 1002);

    // ---- authentication ----
    std::string msg;
    CHECK(bank.authenticate(1001, "4821", msg) == AuthResult::Ok);
    CHECK(bank.authenticate(1001, "0000", msg) == AuthResult::BadCredentials);
    const std::string badMsg = msg;
    CHECK(bank.authenticate(9999, "4821", msg) == AuthResult::BadCredentials && msg == badMsg);  // same message
    CHECK(bank.authenticate(1001, "4821", msg) == AuthResult::Ok);                       // success resets counter
    CHECK(alice->failedAttempts() == 0);

    // ---- lockout after 5 wrong PINs ----
    for (int i = 0; i < Bank::kMaxFailedAttempts - 1; ++i)
        CHECK(bank.authenticate(1002, "1111", msg) == AuthResult::BadCredentials);
    CHECK(bank.authenticate(1002, "1111", msg) == AuthResult::Locked);
    CHECK(bank.authenticate(1002, "9013", msg) == AuthResult::Locked);                   // even the right PIN
    CHECK(msg.find("minute") != std::string::npos);
    CHECK(bank.authenticate(1001, "4821", msg) == AuthResult::Ok);                       // others unaffected

    // ---- change PIN ----
    CHECK(!bank.changePin(1001, "9999", "5937", err));                                   // wrong current
    CHECK(!bank.changePin(1001, "4821", "1234", err));                                   // weak new
    CHECK(!bank.changePin(1001, "4821", "4821", err));                                   // same as old
    CHECK(bank.changePin(1001, "4821", "5937", err));
    CHECK(bank.authenticate(1001, "4821", msg) == AuthResult::BadCredentials);
    CHECK(bank.authenticate(1001, "5937", msg) == AuthResult::Ok);

    // ---- accounts and money (as before) ----
    const Account* sav = bank.openAccount(1001, AccountType::Savings, 10000, err);
    const Account* chk = bank.openAccount(1001, AccountType::Checking, 0, err);
    const Account* bobAcc = bank.openAccount(1002, AccountType::Savings, 5000, err);
    CHECK(sav && chk && bobAcc && sav->accountNo() == 5001);
    CHECK(bank.deposit(5001, 2550, err) && sav->balance() == 12550);
    CHECK(!bank.withdraw(5001, 99999, err));
    CHECK(bank.withdraw(5002, 20000, err) && chk->balance() == -20000);
    CHECK(!bank.withdraw(5002, 1, err));
    CHECK(bank.deposit(5002, 20000, err));
    CHECK(bank.transfer(5001, 5003, 5000, "=HYPERLINK(\"x\")", err));
    CHECK(sav->balance() == 12550 - 5000 && bobAcc->balance() == 10000);
    CHECK(!bank.transfer(5001, 5001, 100, "", err));
    CHECK(!bank.transfer(5001, 5003, 99999999, "", err));

    // ---- save / load round trip; the PIN is never written in clear ----
    CHECK(bank.save(err));
    const std::string content = readAll(path);
    CHECK(content.rfind("BANKDATA 2\n", 0) == 0);
    CHECK(content.find("5937") == std::string::npos);
    CHECK(content.find("|4821|") == std::string::npos);
    Bank loaded(path);
    CHECK(loaded.load(err));
    CHECK(loaded.authenticate(1001, "5937", msg) == AuthResult::Ok);
    CHECK(loaded.authenticate(1001, "4821", msg) == AuthResult::BadCredentials);
    CHECK(loaded.authenticate(1002, "9013", msg) == AuthResult::Locked);                 // lockout survives restart
    CHECK(loaded.findAccount(5001)->balance() == sav->balance());
    const Customer* carol = loaded.createCustomer("Carol Jones", "0933445566", "2468", err);
    CHECK(carol && carol->id() == 1003);

    // ---- corrupt / tampered files are rejected ----
    auto expectReject = [&](const std::string& body, const char* label) {
        const std::string p = tempFile("bank_gui_test_b.txt");
        { std::ofstream o(p); o << body; }
        Bank b(p);
        std::string e;
        if (b.load(e) || e.empty()) { ++failures; std::cout << "FAIL: not rejected: " << label << "\n"; }
    };
    std::string tampered = content;
    tampered.replace(tampered.find("ACCOUNT|5001|1001|SAVINGS|") + 25, 1, "9");
    expectReject(tampered, "tampered balance");
    expectReject("garbage\n", "bad header");
    expectReject(content + "ACCOUNT|7000|4242|SAVINGS|0|1\n", "unknown customer");
    expectReject(content + "NONSENSE\n", "unknown record");
    std::string badHash = content;
    const auto pos = badHash.find("CUSTOMER|1001|Alice Smith|+251911223344|");
    const auto hashStart = badHash.find('|', badHash.find('|', pos + 41) + 1) ;  // somewhere inside the PIN fields
    badHash.replace(hashStart + 1, 3, "zzz");
    expectReject(badHash, "invalid PIN hash data");

    // ---- version 1 (no PIN) files still load; PIN can be set once ----
    {
        const std::string p = tempFile("bank_gui_test_v1.txt");
        std::ofstream(p) << "BANKDATA 1\nCUSTOMER|1001|Old Customer|0911000111|1700000000\n"
                            "ACCOUNT|5001|1001|SAVINGS|0|1700000000\n";
        Bank v1(p);
        CHECK(v1.load(err));
        CHECK(v1.authenticate(1001, "4821", msg) == AuthResult::NoPin);
        CHECK(!v1.setInitialPin(1001, "1234", err));
        CHECK(v1.setInitialPin(1001, "4821", err));
        CHECK(!v1.setInitialPin(1001, "9013", err));                                      // only once
        CHECK(v1.authenticate(1001, "4821", msg) == AuthResult::Ok);
        std::filesystem::remove(p);
    }

    // ---- CSV export ----
    CHECK(plainAmount(-7500) == "-75.00");
    CHECK(plainAmount(5) == "0.05");
    CHECK(csvField("plain") == "plain");
    CHECK(csvField("a,b") == "\"a,b\"");
    CHECK(csvField("say \"hi\"") == "\"say \"\"hi\"\"\"");
    CHECK(csvField("=1+1") == "'=1+1");                 // formula injection neutralised
    CHECK(csvField("@SUM(A1)") == "'@SUM(A1)");
    const std::string csv = buildStatementCsv({loaded.findAccount(5001)});
    CHECK(csv.rfind("\xEF\xBB\xBF" "Account,Account Type,Transaction ID,Date,Type,Amount,Balance After,Other Account,Note\r\n", 0) == 0);
    CHECK(csv.find(",Transfer out,-50.00,75.50,5003,'=HYPERLINK(\"\"x\"\")") == std::string::npos);  // note is quoted below
    CHECK(csv.find(",Transfer out,-50.00,75.50,5003,\"'=HYPERLINK(\"\"x\"\")\"\r\n") != std::string::npos);
    CHECK(csv.find(",Deposit,100.00,100.00,,Opening deposit\r\n") != std::string::npos);
    const std::string csvPath = tempFile("bank_gui_test.csv");
    CHECK(exportStatementCsv(csvPath, {loaded.findAccount(5001)}, err));
    CHECK(readAll(csvPath) == csv);
    CHECK(!exportStatementCsv("/nonexistent/dir/x.csv", {}, err));

    for (const char* n : {"bank_gui_test_a.txt", "bank_gui_test_b.txt", "bank_gui_test.csv"})
        std::filesystem::remove(tempFile(n));
    std::cout << (failures == 0 ? "All tests passed.\n" : "Some checks FAILED.\n");
    return failures == 0 ? 0 : 1;
}
