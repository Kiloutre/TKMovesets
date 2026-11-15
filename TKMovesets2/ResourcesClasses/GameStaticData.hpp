#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Optional display metadata. This never changes game records or resolves aliases.
class GameStaticData {
public:
    struct Character {
        std::string code, name, japanese;
        std::optional<uint32_t> id, runtimeId;
        std::optional<bool> playable;
    };
    static std::filesystem::path DefaultPath(std::string_view profile);
    // Missing/invalid file: empty data with Error() set. The default path is served from LoadShared's cache.
    static GameStaticData LoadOptional(std::string_view profile, const std::filesystem::path& path = {});
    // The default file of `profile`, parsed once and shared (thread-safe); re-read when its size or mtime changes.
    static std::shared_ptr<const GameStaticData> LoadShared(std::string_view profile);
    const Character* FindCharacter(uint32_t id) const;
    const Character* FindRuntimeCharacter(uint32_t id) const;
    const Character* FindCharacter(std::string_view code) const;
    const std::string* FindName(uint32_t hash) const;
    std::string CharacterName(uint32_t id) const;
    const std::string& Error() const { return m_error; }
    size_t CharacterCount() const { return m_characters.size(); }
    const std::vector<Character>& Characters() const { return m_characters; }
    size_t NameCount() const { return m_names.size(); }
private:
    static GameStaticData Load(std::string_view profile, const std::filesystem::path& path);
    std::vector<Character> m_characters;
    std::map<uint32_t, size_t> m_ids;
    std::map<uint32_t, size_t> m_runtimeIds;
    std::map<std::string, size_t, std::less<>> m_codes;
    std::map<uint32_t, std::string> m_names;
    std::string m_error;
};
