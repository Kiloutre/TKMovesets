#include "FileIO.hpp"
#include <algorithm>
#include <windows.h>

bool FileIO::WriteAtomically(const std::filesystem::path& destination, std::span<const Byte> content)
{
    std::error_code error;
    auto parent = std::filesystem::absolute(destination, error).parent_path();
    if (error) return false;
    wchar_t temporary[MAX_PATH];
    if (!GetTempFileNameW(parent.c_str(), L"tkm", 0, temporary)) return false;
    try {
        bool written = false;
        const HANDLE file = CreateFileW(temporary, GENERIC_WRITE, 0, nullptr, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            written = true;
            for (size_t at = 0; written && at < content.size();) {
                const DWORD chunk = static_cast<DWORD>((std::min<size_t>)(content.size() - at, size_t(1) << 30));
                DWORD done = 0;
                written = WriteFile(file, content.data() + at, chunk, &done, nullptr) != FALSE && done == chunk;
                at += done;
            }
            // The data must reach the disk before the rename can expose it under the destination name,
            // otherwise a power loss may leave an empty or short file in place of the original.
            written = written && FlushFileBuffers(file) != FALSE;
            written = CloseHandle(file) != FALSE && written;
        }
        if (written && MoveFileExW(temporary, destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return true;
    } catch (...) {
        DeleteFileW(temporary);
        throw;
    }
    DeleteFileW(temporary);
    return false;
}

bool FileIO::WriteNew(const std::filesystem::path& destination, std::span<const Byte> content)
{
    const HANDLE file = CreateFileW(destination.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    bool written = true;
    for (size_t at = 0; written && at < content.size();) {
        const DWORD chunk = static_cast<DWORD>((std::min<size_t>)(content.size() - at, size_t(1) << 30));
        DWORD done = 0;
        written = WriteFile(file, content.data() + at, chunk, &done, nullptr) != FALSE && done == chunk;
        at += done;
    }
    written = CloseHandle(file) != FALSE && written;
    if (!written) DeleteFileW(destination.c_str());
    return written;
}
