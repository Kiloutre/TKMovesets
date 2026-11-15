#include "GameStaticData.hpp"
#include "EmbeddedInterface.hpp"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <array>
#include <charconv>
#include <fstream>
#include <mutex>
#include <set>
#include <stdexcept>

namespace {
constexpr size_t MaxBytes = 8 << 20;
void Check(bool ok, const char* reason) { if (!ok) throw std::invalid_argument(reason); }
std::string Text(const nlohmann::json& value, size_t limit) {
    auto text = value.get<std::string>();
    Check(text.size() <= limit, "Invalid static name text");
    for (unsigned char c : text) Check(c >= 32, "Control character in static name text");
    return text;
}
bool Profile(std::string_view value) {
    if (value.empty() || value.size() > 64) return false;
    for (char c : value) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    return true;
}
}
std::filesystem::path GameStaticData::DefaultPath(std::string_view profile) {
    if (!Profile(profile)) return {};
    std::array<wchar_t, 32768> executable{};
    auto size = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (!size || size >= executable.size()) return {};
    return std::filesystem::path(executable.data()).parent_path() / "TKMInterface" / (std::string(profile) + "_names.json");
}
GameStaticData GameStaticData::LoadOptional(std::string_view profile, const std::filesystem::path& requested) {
    if (requested.empty()) return *LoadShared(profile);
    return Load(profile, requested);
}
std::shared_ptr<const GameStaticData> GameStaticData::LoadShared(std::string_view profile) {
    // Callers construct this per operation (e.g. one importer per preview refresh): the file is ~1 MB of JSON.
    struct Entry {
        std::filesystem::path path; bool exists = false; uintmax_t size = 0;
        std::filesystem::file_time_type time{}; std::shared_ptr<const GameStaticData> data;
    };
    static std::mutex mutex;
    static std::map<std::string, Entry, std::less<>> cache;
    const auto path = DefaultPath(profile);
    std::error_code error;
    const bool exists = !path.empty() && std::filesystem::is_regular_file(path, error);
    const auto size = exists ? std::filesystem::file_size(path, error) : uintmax_t(0);
    const auto time = exists ? std::filesystem::last_write_time(path, error) : std::filesystem::file_time_type{};
    std::lock_guard lock(mutex);
    auto& entry = cache[std::string(profile)];
    if (!entry.data || entry.path != path || entry.exists != exists || entry.size != size || entry.time != time)
        entry = {path, exists, size, time, std::make_shared<const GameStaticData>(Load(profile, path))};
    return entry.data;
}
GameStaticData GameStaticData::Load(std::string_view profile, const std::filesystem::path& path) {
    try {
        Check(Profile(profile), "Invalid static data profile");
        std::string bytes;
        if (std::ifstream input(path, std::ios::binary | std::ios::ate); input) {
            auto size = input.tellg();
            Check(size > 0 && size <= static_cast<std::streamoff>(MaxBytes), "Static name file exceeds 8 MiB");
            bytes.assign(static_cast<size_t>(size), '\0');
            input.seekg(0); Check(static_cast<bool>(input.read(bytes.data(), bytes.size())), "Cannot read static name data");
        }
        else {
            // no file: the copy embedded in the exe (the default names file only; an explicit other path must exist)
            auto embedded = path == DefaultPath(profile) || path.empty() ? EmbeddedInterface::Embedded(std::string(profile) + "_names.json") : std::nullopt;
            Check(embedded.has_value(), "Optional static name file is missing");
            bytes = std::move(*embedded);
            Check(!bytes.empty() && bytes.size() <= MaxBytes, "Static name file exceeds 8 MiB");
        }
        std::vector<std::set<std::string>> objects;
        auto callback = [&](int depth, nlohmann::json::parse_event_t event, nlohmann::json& value) {
            Check(depth <= 16, "Static name nesting exceeds limit");
            if (event == nlohmann::json::parse_event_t::object_start) objects.emplace_back();
            if (event == nlohmann::json::parse_event_t::key)
                Check(!objects.empty() && objects.back().insert(value.get<std::string>()).second, "Duplicate static data key");
            if (event == nlohmann::json::parse_event_t::object_end) objects.pop_back();
            return true;
        };
        auto data = nlohmann::json::parse(bytes, callback);
        Check(data.at("schema_version").is_number_integer() && data.at("schema_version") == 1 && Text(data.at("profile"), 64) == profile,
            "Unsupported static name schema or profile");
        const auto& characters = data.at("characters");
        Check(characters.is_array() && characters.size() <= 1024, "Invalid static character list");
        GameStaticData result;
        for (const auto& row : characters) {
            Character character;
            character.code = Text(row.at("code"), 16);
            Check(!character.code.empty(), "Empty character code");
            character.name = Text(row.at("name"), 128);
            character.japanese = Text(row.at("name_jp"), 256);
            if (row.contains("runtime_id")) {
                const auto& value = row.at("runtime_id");
                Check(value.is_number_integer(), "Invalid runtime character id");
                const auto id = value.get<int64_t>();
                Check(id >= 0 && id <= UINT32_MAX, "Runtime character id outside range");
                character.runtimeId = static_cast<uint32_t>(id);
                Check(result.m_runtimeIds.emplace(*character.runtimeId, result.m_characters.size()).second,
                    "Duplicate runtime character id");
            }
            if (!row.at("id").is_null()) {
                Check(row.at("id").is_number_unsigned() || row.at("id").is_number_integer(), "Invalid character id");
                auto id = row.at("id").get<int64_t>(); Check(id >= 0 && id <= UINT32_MAX, "Character id outside range");
                character.id = static_cast<uint32_t>(id);
                auto digits = std::to_string(id);
                if (digits.size() < 4) digits.insert(0, 4 - digits.size(), '0');
                Check(row.at("chr_id") == "chr_" + digits, "Character id disagrees with chr_ identifier");
                Check(result.m_ids.emplace(*character.id, result.m_characters.size()).second, "Duplicate character id");
            } else Check(row.at("chr_id") == "", "Unidentified character has a chr_ identifier");
            if (!row.at("playable").is_null()) character.playable = row.at("playable").get<bool>();
            Check(result.m_codes.emplace(character.code, result.m_characters.size()).second, "Duplicate character code");
            result.m_characters.push_back(std::move(character));
        }
        for (size_t i = 0; i < result.m_characters.size(); ++i) {
            const auto& character = result.m_characters[i];
            if (!character.runtimeId && character.id) result.m_runtimeIds.emplace(*character.id, i);
        }
        const auto& names = data.at("names");
        Check(names.is_object() && names.size() <= 100000, "Invalid static name dictionary");
        for (const auto& [key, value] : names.items()) {
            Check(key.size() == 10 && key[0] == '0' && key[1] == 'x', "Invalid name hash encoding");
            uint32_t hash = 0;
            auto parsed = std::from_chars(key.data() + 2, key.data() + key.size(), hash, 16);
            Check(parsed.ec == std::errc{} && parsed.ptr == key.data() + key.size(), "Invalid 32-bit name hash");
            auto name = Text(value, 256); Check(!name.empty(), "Empty resolved name");
            Check(result.m_names.emplace(hash, std::move(name)).second, "Duplicate name hash");
        }
        return result;
    } catch (const std::exception& error) {
        GameStaticData empty; empty.m_error = error.what(); return empty;
    }
}
const GameStaticData::Character* GameStaticData::FindCharacter(uint32_t id) const {
    auto found = m_ids.find(id); return found == m_ids.end() ? nullptr : &m_characters[found->second];
}
const GameStaticData::Character* GameStaticData::FindCharacter(std::string_view code) const {
    auto found = m_codes.find(code); return found == m_codes.end() ? nullptr : &m_characters[found->second];
}
const GameStaticData::Character* GameStaticData::FindRuntimeCharacter(uint32_t id) const {
    auto found = m_runtimeIds.find(id); return found == m_runtimeIds.end() ? nullptr : &m_characters[found->second];
}
const std::string* GameStaticData::FindName(uint32_t hash) const {
    auto found = m_names.find(hash); return found == m_names.end() ? nullptr : &found->second;
}
std::string GameStaticData::CharacterName(uint32_t id) const {
    auto character = FindRuntimeCharacter(id);
    return character && !character->name.empty() ? character->name : "Character " + std::to_string(id);
}
