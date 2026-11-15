#pragma once
#include "ExeSignatures.hpp"
#include "GameAddressesFile.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// The address update shared by Tools > Addresses and `--headless update-addresses-t8`:
// scan the exe file with the game's patterns, then write the values that changed to game_addresses.ini and to
// the loaded addresses. Values that were not found, or whose patterns disagree, keep their current value.
namespace ExeSignatures {

enum class Outcome { Updated, Unchanged, NotFound, Conflict, NotInIni };

struct UpdateRow {
    KeyResult scan;
    std::optional<int64_t> before; // loaded value before the update, if the key exists
    Outcome outcome = Outcome::NotFound;
};

struct UpdateResult {
    uint32_t peTimestamp = 0;
    std::vector<UpdateRow> rows;
    size_t updated = 0;   // rows written (0 when `write` was false or the write failed)
    size_t changes = 0;   // rows whose value differs from the loaded one
    size_t failed = 0;    // NotFound + Conflict
    bool writeFailed = false;
};

const char* OutcomeName(Outcome outcome); // "updated", "unchanged", "not_found", "conflict", "not_in_ini"

// Loaded value of an address key: a `val:` value, or the module offset of a relative pointer path.
std::optional<int64_t> LoadedValue(const GameAddressesFile& addresses, const std::string& gameKey, const std::string& key);

// Reads `exe` and scans it with the game's `sig_` patterns. Throws when the game has no patterns or the exe is unreadable.
ScanReport ScanGame(const GameAddressesFile& addresses, const std::string& gameKey, const std::filesystem::path& exe);

// write = false: report what would change, touching nothing. write = true: rewrite `iniPath` (from that file, or from the
// embedded copy if it does not exist yet), then, only if the file was written, update `addresses` in place.
UpdateResult ApplyScan(GameAddressesFile& addresses, const std::string& gameKey, const ScanReport& report,
    const std::filesystem::path& iniPath, bool write);

}
