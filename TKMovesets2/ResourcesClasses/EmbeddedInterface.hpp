#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

// TKMInterface data (fighter names, editor labels) is embedded in the exe (CMakeLists.txt, FileEmbedder/LZMA) and
// unpacked to TKMInterface/ next to the exe at launch (Unpack). Readers take the file on disk (working directory, then
// next to the exe) and fall back to the embedded copy when it cannot be written or read.
namespace EmbeddedInterface
{
	// At launch: write each embedded file to TKMInterface/ next to the exe when it is missing, or differs from the
	// embedded copy and is older than the exe (an older version's file). A file newer than the exe (edited) is kept.
	void Unpack();
	// The file's contents: TKMInterface/<name> on disk if present, else the embedded copy; nullopt if neither exists
	std::optional<std::string> Read(std::string_view name);
	// The disk file Read would use: the existing TKMInterface/<name> (working directory, then next to the exe), else
	// the path next to the exe where it would be created
	std::filesystem::path DiskPath(std::string_view name);
	// The embedded copy only
	std::optional<std::string> Embedded(std::string_view name);
}
