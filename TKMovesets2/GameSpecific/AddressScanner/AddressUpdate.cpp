#include "AddressUpdate.hpp"
#include "FileIO.hpp"

#include <map>
#include <stdexcept>

namespace ExeSignatures {

const char* OutcomeName(Outcome outcome)
{
    switch (outcome) {
    case Outcome::Updated: return "updated";
    case Outcome::Unchanged: return "unchanged";
    case Outcome::NotFound: return "not_found";
    case Outcome::Conflict: return "conflict";
    case Outcome::NotInIni: return "not_in_ini";
    }
    return "unknown";
}

std::optional<int64_t> LoadedValue(const GameAddressesFile& addresses, const std::string& gameKey, const std::string& key)
{
    int64_t value;
    if (addresses.GetValueEx(gameKey, key.c_str(), value)) return value;
    bool relative = false; const std::vector<gameAddr>* path = nullptr;
    if (addresses.GetPtrPathEx(gameKey, key.c_str(), relative, path) && relative && path && !path->empty()) return static_cast<int64_t>((*path)[0]);
    return std::nullopt;
}

ScanReport ScanGame(const GameAddressesFile& addresses, const std::string& gameKey, const std::filesystem::path& exe)
{
    const auto patterns = addresses.GetStringsWithPrefix(gameKey, "sig_");
    if (patterns.empty()) throw std::runtime_error("game_addresses.ini holds no byte patterns for " + gameKey);
    return Scan(ReadImage(exe), patterns);
}

UpdateResult ApplyScan(GameAddressesFile& addresses, const std::string& gameKey, const ScanReport& report,
    const std::filesystem::path& iniPath, bool write)
{
    UpdateResult result; result.peTimestamp = report.peTimestamp;
    std::map<std::string, int64_t> updates;
    for (const auto& key : report.keys) {
        UpdateRow row{ key, LoadedValue(addresses, gameKey, key.key) };
        if (key.status == KeyStatus::Conflict) row.outcome = Outcome::Conflict;
        else if (key.status == KeyStatus::NotFound) row.outcome = Outcome::NotFound;
        else if (!row.before) row.outcome = Outcome::NotInIni;
        else if (*row.before == key.value) row.outcome = Outcome::Unchanged;
        else { row.outcome = Outcome::Updated; updates[key.key] = key.value; }
        result.failed += row.outcome == Outcome::NotFound || row.outcome == Outcome::Conflict;
        result.rows.push_back(std::move(row));
    }
    if (updates.empty()) return result;

    // The file first: if it cannot be written, nothing changes in memory either.
    std::vector<std::string> missing;
    const auto text = RewriteIni(GameAddressesFile::LoadSourceText(iniPath), gameKey, updates, missing);
    for (const auto& key : missing) {
        updates.erase(key);
        for (auto& row : result.rows) if (row.scan.key == key) row.outcome = Outcome::NotInIni;
    }
    result.changes = updates.size();
    if (!write || updates.empty()) return result;
    if (!FileIO::WriteAtomically(iniPath, text)) {
        result.writeFailed = true;
        for (auto& row : result.rows) if (row.outcome == Outcome::Updated) row.outcome = Outcome::Unchanged;
        return result;
    }
    for (const auto& [key, value] : updates) addresses.UpdateInPlace(gameKey, key, value);
    addresses.RefreshSharedMemory();
    result.updated = updates.size();
    return result;
}

}
