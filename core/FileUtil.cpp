#include "FileUtil.h"

#include <filesystem>
#include <fstream>

bool writeFileAtomically(const std::string& path, const std::string& data, std::string& error) {
    namespace fs = std::filesystem;
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "Cannot write to " + tmp;
            return false;
        }
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
        out.flush();
        if (!out) {
            error = "Writing to " + tmp + " failed (disk full?).";
            std::error_code ignore;
            fs::remove(tmp, ignore);
            return false;
        }
    }
    std::error_code ec;
    fs::rename(tmp, path, ec);
    if (ec) {  // some platforms refuse to rename over an existing file
        std::error_code ignore;
        fs::remove(path, ignore);
        fs::rename(tmp, path, ec);
    }
    if (ec) {
        error = "Could not replace " + path + ": " + ec.message();
        return false;
    }
    return true;
}
