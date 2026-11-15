#pragma once
#include "RetailGameData.hpp"
#include "RetailSnapshot.hpp"
#include <cstdint>
#include <span>
#include <vector>
#include <array>
#include <filesystem>
#include <string>

// A retail moveset built from the game's own files, without the game loading it: the character's
// mothead/bin/<code>.motbin as the game's loader leaves it in memory (TK__Moveset_Relocate 0x1418B2490, ported
// field by field), with its animations from <code>.anmbin / com.anmbin. The result is the same T8RS v4 snapshot an
// export from memory produces. See .docs/retail-t8-extraction.md "Movesets from the game's files".
namespace RetailT8Disk {
struct Report {
    uint32_t domain = 0;                 // animation character (handle domain)
    uint64_t moves = 0, animations = 0;  // move rows; animations carried
    std::array<uint64_t, 6> dataless{};  // per category: declared without data in the game's files (size-0 markers)
};
// `categories`: the animation categories to carry (bit n = category n, as RetailAnimationSelection).
// `domain`: the game's character id (animation handle domain; Jin 6, Kazuya 8), not the motbin id stored in the moves.
std::vector<Byte> Build(std::span<const Byte> motbin, uint32_t domain, const RetailT8GameData::Anmbin& character,
    const RetailT8GameData::Anmbin& common, uint8_t categories, Report* report = nullptr);
// The notes text for animations a file does not carry ("" when none): shared by exports from memory and from disk.
// `fromArchive`: the game's files were consulted (so the missing ones have no data there).
std::string MissingAnimationsNote(const std::array<uint64_t, 6>& missing, bool fromArchive);
// Builds `code`'s moveset from the game's tkdata.bin and writes it, compressed with `codec`, to the new file `path`.
// `common` is com.anmbin, read once per batch by the caller.
Report WriteMovesetFile(const std::filesystem::path& tkdata, const std::string& code, const RetailT8GameData::Anmbin& common,
    const std::filesystem::path& path, uint8_t categories = 63, TKMovesetCompressionType_ codec = TKMovesetCompressionType_CompactZstd);
// The output name, the same as a memory export's: "T8_<character name>.tkmvst" (an existing file is skipped).
std::string FileName(const std::string& code);
// The relocation alone (exposed for tests): `image` is the motbin, relocated in place as if loaded at `root`.
void Relocate(std::vector<Byte>& image, uint64_t root, uint32_t domain, const RetailT8GameData::Anmbin& character,
    const RetailT8GameData::Anmbin& common, uint64_t placeholder);
}
