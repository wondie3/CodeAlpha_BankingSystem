// Password/PIN hashing. SHA-256 is implemented here so the core has no dependencies.
// (A production system would use Argon2/bcrypt/scrypt from a vetted library.)
#pragma once

#include <cstddef>
#include <string>

namespace crypto {

std::string sha256(const std::string& data);   // returns the 32 raw digest bytes
std::string toHex(const std::string& raw);
std::string randomSalt();                       // 16 random bytes as 32 hex characters
// Salted, stretched hash (10,000 rounds) as 64 hex characters.
std::string hashSecret(const std::string& secret, const std::string& saltHex);
bool constantTimeEquals(const std::string& a, const std::string& b);
bool isHex(const std::string& s, std::size_t length);

}  // namespace crypto
