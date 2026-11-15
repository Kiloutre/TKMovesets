#pragma once
#include "MovesetStructs.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// Tekken 7 movesets built from the game's own files, without the game loading them. TekkenGame/Content/Binary/tkdata.bin
// (`_TEKKEN7CSFILES_`: AES-256-ECB + zlib, names in clear) holds per character mothead/bin/<code>mot.bin (the HID
// moveset, MovesetInfo layout, offsets relative to +0x2E8), mothead/mask/<code>rotsm.arc (its MOTAs for the odd slots)
// and mothead/movelist/<code>.mvl (training move list); mothead/mask/common.arc is the shared animation MOTA. The image
// is relocated as the game holds it in memory, then exported by the T7 memory exporter (T7Export::ReadSnapshot).
// See .docs/t7-memory-layout.md "Movesets from the game's files".
namespace T7Disk {
// TekkenGame/Binaries/Win64/<exe> -> TekkenGame/Content/Binary/tkdata.bin
std::filesystem::path TkdataPath(const std::filesystem::path& gameExecutable);
// Every file name the archive lists ("mothead\\bin\\jinmot.bin").
std::vector<std::string> TkdataNames(const std::filesystem::path& tkdata);
// One file, decrypted and decompressed. Throws when missing or invalid.
std::vector<Byte> ReadTkdataFile(const std::filesystem::path& tkdata, std::string_view name);
// Character codes with a moveset in the archive ("jin"), sorted.
std::vector<std::string> Characters(const std::filesystem::path& tkdata);
// The same codes with each moveset's display name (read from its header), in one pass over the archive.
std::vector<std::pair<std::string, std::string>> CharacterNames(const std::filesystem::path& tkdata);

struct Report { std::string name; uint32_t characterId = 0; uint64_t moves = 0, animations = 0; std::filesystem::path path; bool existed = false; };
// The T7 moveset file (uncompressed, TKMovesetHeader + blocks) of `code`, with every MOTA and the training move list.
// `common` is mothead/mask/common.arc, read once per batch by the caller.
std::vector<Byte> BuildFile(const std::filesystem::path& tkdata, const std::string& code, const std::vector<Byte>& common, Report* report = nullptr);
// "T7_<display name>.tkmvst", the same as a memory export's (an existing file is skipped)
std::string FileName(const std::string& displayName);
// Builds `code`'s moveset and writes it, compressed with `codec`, as `directory`/FileName(name); an existing file of the
// same character is left alone (report.existed). When that file holds another character with the same display name, the
// moveset is written as FileName("<name> (<code>)") instead.
Report WriteMovesetFile(const std::filesystem::path& tkdata, const std::string& code, const std::vector<Byte>& common, const std::filesystem::path& directory,
    TKMovesetCompressionType_ codec = TKMovesetCompressionType_LZ4);
}
