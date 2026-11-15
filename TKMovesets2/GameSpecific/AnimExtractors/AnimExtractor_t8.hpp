#pragma once
#include "Extractor_t8/RetailSnapshot.hpp"
#include <filesystem>
#include <atomic>
#include <string>
#include <string_view>
#include <functional>
#include <vector>
#include "Extractor_t8/RetailGameData.hpp"
namespace AnimExtractorPaths {
// One file or folder name built from untrusted text (UTF-8 expected), safe to append to an output folder: path separators,
// reserved and control characters, invalid UTF-8 and characters outside the BMP (Helpers::to_unicode is UCS-2) become '_',
// a trailing dot or space is replaced, a reserved device name (CON, NUL, COM1...) gets a '_' prefix, so the result is never
// "." / ".." / empty. Cut at a character boundary to at most maxBytes bytes.
std::string SafeComponent(std::string_view value, size_t maxBytes = 120);
}
namespace RetailT8Animations {
// One embedded animation of a payload. `name`: the animation name when a move plays it (body only), else empty.
// `file`: its file name in an extraction (unique within the list returned by List).
struct Animation { uint8_t category = 0; uint32_t instance = 0; uint32_t bytes = 0; std::string name, file; };
inline constexpr const char* Categories[] = {"body", "hands", "face", "swing", "camera", "extra"};
// Every embedded animation (size > 0), in directory order. `nameForHash` names body animations from the animation
// name hash of the moves playing them (an empty result = unknown); without it the shipped t8-retail names are used.
std::vector<Animation> List(std::span<const Byte> payload, const std::function<std::string(uint32_t)>& nameForHash = {});
// `selection` (from List of the same payload) + animations.json, written to "<newDirectory>.partial" and renamed when
// complete. `interrupt` is checked per animation: the partial folder is removed and 0 is returned.
size_t Extract(std::span<const Byte> payload, const std::vector<Animation>& selection, const std::filesystem::path& newDirectory,
    const std::atomic<bool>* interrupt = nullptr, std::atomic<uint32_t>* done = nullptr);
// Every embedded animation (List + Extract). Throws when there is none.
size_t Extract(std::span<const Byte> payload,const std::filesystem::path& newDirectory,const bool* interrupt=nullptr,
    uint32_t* done=nullptr,uint32_t* total=nullptr);
// Every animation of one of the game's .anmbin archives (read from tkdata.bin) into a new directory, named like Extract
// (<category>_<instance>.panm + animations.json). Written to "<directory>.partial" and renamed when complete; an
// interrupt (checked per animation) removes the partial folder and returns false. `done`/`total` report progress.
bool ExtractArchive(const RetailT8GameData::Anmbin& archive, const std::filesystem::path& newDirectory,
    const std::atomic<bool>* interrupt = nullptr, std::atomic<uint32_t>* done = nullptr, std::atomic<uint32_t>* total = nullptr);
}
