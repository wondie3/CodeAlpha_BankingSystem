// CSV export that opens cleanly in Microsoft Excel.
#pragma once

#include <string>
#include <vector>

#include "Account.h"
#include "Bank.h"

// One CSV cell: quoted when needed, and neutralised against "formula injection"
// (Excel executes cells that start with = + - @, so such text gets a leading apostrophe).
std::string csvField(const std::string& text);

// 12345 -> "123.45", -7500 -> "-75.00" (plain numbers, so Excel can sum them).
std::string plainAmount(Money cents);

// Complete file content: UTF-8 BOM + header row + one row per transaction (oldest first),
// CRLF line endings. Columns: Account, Account Type, Transaction ID, Date, Type, Amount,
// Balance After, Other Account, Note.
std::string buildStatementCsv(const std::vector<const Account*>& accounts);

bool exportStatementCsv(const std::string& path, const std::vector<const Account*>& accounts,
                        std::string& error);
