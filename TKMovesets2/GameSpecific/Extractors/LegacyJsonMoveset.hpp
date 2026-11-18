#pragma once
#include "MovesetStructs.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Movesets saved by the old Python TekkenMovesetExtractor: a folder <name>/ holding <name>.json (every record list, records
// referencing each other by index), anim/<animation name>.bin and mota_<0..11>.bin.
// - "Tekken7" folders become T7 files: the moveset is laid out as the game holds it in memory, then exported by the T7
//   memory exporter (T7Export::ReadSnapshot), like T7Disk does with the game's files.
// - "Tag2" folders (Cemu Wii U and RPCS3 PS3 extractions) become TTT2 files laid out as ExtractorTTT2 writes them.
// The JSON format does not hold every field: what it lacks is left empty (see docs/legacy-json.md).
namespace LegacyJson {
struct Report {
    std::string name, game; uint32_t characterId = 0; uint64_t moves = 0, animations = 0;
    std::vector<std::string> borrowedAnimations; // animations missing from the folder, found in another extracted folder
    std::vector<std::string> replacedAnimations; // unreadable everywhere: their moves use the moveset's first animation
    uint64_t paddedAnimations = 0;               // animation files missing their last zero bytes, given them back
    uint64_t unswappedAnimations = 0;            // animation files the extractor mis-swapped, restored (UndoExtractorSwap)
    uint64_t unswappedMotaAnimations = 0;        // the same, inside the kept MOTA files
    std::vector<int> motas, skippedMotas;        // MOTA files kept, MOTA files that are not valid MOTA data
    uint64_t droppedReferences = 0;              // record indexes outside their list, written as "none"
    std::filesystem::path path; bool existed = false;
};
// The moveset file (uncompressed TKMovesetHeader + blocks) of an extracted folder.
std::vector<Byte> BuildFile(const std::filesystem::path& folder, Report* report = nullptr);
// Builds the folder's moveset and writes it, compressed with `codec`, in `directory` as "T7_<name> (MovesetExtractor).tkmvst"
// or "TTT2_<name> (Wii U|PS3).tkmvst". An existing file is left alone (report.existed).
Report WriteMovesetFile(const std::filesystem::path& folder, const std::filesystem::path& directory,
    TKMovesetCompressionType_ codec = TKMovesetCompressionType_LZ4);
}
