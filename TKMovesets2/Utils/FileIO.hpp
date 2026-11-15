#pragma once
#include <filesystem>
#include <span>
#include <string_view>
#include "GameTypes.h"

namespace FileIO {
    // Stage in the destination directory and install only after a complete write.
    bool WriteAtomically(const std::filesystem::path& destination, std::span<const Byte> content);
    inline bool WriteAtomically(const std::filesystem::path& destination, std::string_view content)
    {
        return WriteAtomically(destination, std::span(reinterpret_cast<const Byte*>(content.data()), content.size()));
    }
    // A new file (fails if it exists), without flushing it to disk. For many files staged in a folder that is
    // renamed once complete: a flush per file costs ~0.1 s each on a hard drive.
    bool WriteNew(const std::filesystem::path& destination, std::span<const Byte> content);
    inline bool WriteNew(const std::filesystem::path& destination, std::string_view content)
    {
        return WriteNew(destination, std::span(reinterpret_cast<const Byte*>(content.data()), content.size()));
    }
}
