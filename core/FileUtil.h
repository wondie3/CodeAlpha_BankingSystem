#pragma once

#include <string>

// Writes `data` to `path` atomically: temp file first, then rename over the target,
// so a crash or full disk can never leave a half-written file behind.
bool writeFileAtomically(const std::string& path, const std::string& data, std::string& error);
