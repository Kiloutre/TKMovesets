#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace MovesetFolders {
struct Configuration { std::string extraction; std::vector<std::string> additional; };
void Load();
Configuration Get();
void Save(Configuration configuration);
std::filesystem::path ExtractionDirectory();
std::vector<std::filesystem::path> SearchDirectories();
// True if the file sits directly in one of the folders the moveset list is built from
bool IsInSearchDirectory(const std::filesystem::path& file);
}
