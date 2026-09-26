#pragma once
#include <cstdio>
#include <filesystem>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
namespace Ship {
inline bool AtomicConfigWrite(const std::filesystem::path& path, const std::string& contents) {
    auto temporary = path; temporary += ".pending";
#ifdef _WIN32
    FILE* file = _wfopen(temporary.c_str(), L"wb");
#else
    FILE* file = std::fopen(temporary.c_str(), "wb");
#endif
    if (!file) return false;
    bool ok = std::fwrite(contents.data(), 1, contents.size(), file) == contents.size();
    ok = std::fflush(file) == 0 && ok;
#ifdef _WIN32
    ok = _commit(_fileno(file)) == 0 && ok;
#else
    ok = fsync(fileno(file)) == 0 && ok;
#endif
    ok = std::fclose(file) == 0 && ok;
    if (ok) {
#ifdef _WIN32
        ok = MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
        ok = std::rename(temporary.c_str(), path.c_str()) == 0;
#endif
    }
    if (!ok) { std::error_code ignored; std::filesystem::remove(temporary, ignored); }
    return ok;
}
} // namespace Ship
