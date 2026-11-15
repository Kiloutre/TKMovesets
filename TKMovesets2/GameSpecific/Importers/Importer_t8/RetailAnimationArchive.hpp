#pragma once
#include "RetailImport.hpp"
#include <filesystem>

namespace RetailT8Resources {
struct ArchiveReport { uint64_t blobs = 0, bytes = 0; };
ArchiveReport VerifyArchives(std::span<const Byte> snapshot, const std::filesystem::path& directory, const std::string& code);
}
