#include "GameStaticData.hpp"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void Check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
using Json = nlohmann::json;
Json Fixture() {
    return Json{{"schema_version", 1}, {"profile", "t8-retail"},
        {"characters", Json::array({Json{{"code","ant"},{"id",6},{"chr_id","chr_0006"},{"name","Jin Kazama"},{"name_jp","\xe4\xbb\x81"},{"playable",true}},
            Json{{"code","test"},{"id",nullptr},{"chr_id",""},{"name",""},{"name_jp",""},{"playable",nullptr}}})},
        {"names", Json{{"0x6AF2B483","motOrigin"},{"0x0A27D720","Ycmn_dm_harai"}}}};
}
void Write(const std::filesystem::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary); file << text;
    Check(static_cast<bool>(file), "Cannot write static data fixture");
}
}
void RunGameStaticDataTests() {
    auto folder = std::filesystem::temp_directory_path() / ("tkmovesets-static-data-" + std::to_string(GetCurrentProcessId()));
    Check(std::filesystem::create_directory(folder), "Static data test folder exists");
    try {
        auto path = folder / "names.json";
        auto missing = GameStaticData::LoadOptional("t8-retail", path);
        Check(missing.CharacterName(6) == "Character 6" && !missing.FindName(1), "Missing optional file did not fall back");
        auto data = Fixture(); Write(path, data.dump());
        auto loaded = GameStaticData::LoadOptional("t8-retail", path);
        Check(loaded.Error().empty() && loaded.CharacterCount() == 2 && loaded.NameCount() == 2, "Valid static data rejected");
        Check(loaded.CharacterName(6) == "Jin Kazama" && loaded.CharacterName(999) == "Character 999", "Character label lookup failed");
        auto character = loaded.FindCharacter("ant");
        Check(character && character->id == 6 && character->japanese == "\xe4\xbb\x81" && character->playable == true, "Character identity or Unicode metadata lost");
        Check(loaded.FindCharacter("test") && !loaded.FindCharacter("test")->id && !loaded.FindCharacter("test")->playable,
            "Unidentified fighter was given a guessed id or playable flag");
        Check(loaded.FindName(0x6af2b483) && *loaded.FindName(0x6af2b483) == "motOrigin" && !loaded.FindName(0), "Name hash lookup or fallback failed");
        Check(!GameStaticData::LoadOptional("t7", path).FindCharacter(6), "Retail names leaked into another game profile");
        auto reject = [&](Json corrupt) {
            Write(path, corrupt.dump()); auto result = GameStaticData::LoadOptional("t8-retail", path);
            Check(!result.Error().empty() && result.CharacterCount() == 0 && result.NameCount() == 0 && result.CharacterName(6) == "Character 6",
                "Malformed static data partially published");
        };
        auto bad = data; bad["schema_version"] = 2; reject(bad);
        bad = data; bad["schema_version"] = 1.0; reject(bad);
        bad = data; bad["characters"].push_back(bad["characters"][0]); reject(bad);
        bad = data; bad["characters"][0]["id"] = -1; reject(bad);
        bad = data; bad["characters"][0]["id"] = 4294967296ULL; reject(bad);
        bad = data; bad["characters"][0]["runtime_id"] = -1; reject(bad);
        bad = data; bad["characters"][0]["runtime_id"] = 4294967296ULL; reject(bad);
        bad = data; bad["characters"][0]["runtime_id"] = 6.0; reject(bad);
        bad = data; bad["characters"][0]["runtime_id"] = 27; bad["characters"][1]["runtime_id"] = 27; reject(bad);
        bad = data; bad["characters"][0]["chr_id"] = "chr_0005"; reject(bad);
        bad = data; bad["characters"][0]["name"] = std::string("bad\0name", 8); reject(bad);
        bad = data; bad["characters"][0]["name"] = std::string(129, 'a'); reject(bad);
        bad = data; bad["names"]["123"] = "invalid hash"; reject(bad);
        bad = data; bad["names"]["0x6af2b483"] = "duplicate numeric hash"; reject(bad);
        bad = data; bad["names"]["0x00000002"] = ""; reject(bad);
        Write(path, "{\"schema_version\":1,\"schema_version\":2}");
        Check(!GameStaticData::LoadOptional("t8-retail", path).Error().empty(), "Duplicate JSON key accepted");
        Write(path, "{"); Check(!GameStaticData::LoadOptional("t8-retail", path).Error().empty(), "Truncated JSON accepted");
        Write(path, std::string((8 << 20) + 1, ' '));
        Check(!GameStaticData::LoadOptional("t8-retail", path).Error().empty(), "Oversized optional file accepted");
        Check(loaded.CharacterName(6) == "Jin Kazama", "Loaded metadata borrowed mutable file storage");
        auto shipped = GameStaticData::LoadOptional("t8-retail");
        Check(shipped.CharacterCount() == 53 && shipped.NameCount() == 47095 && shipped.CharacterName(6) == "Jin Kazama" &&
            shipped.CharacterName(8) == "Kazuya Mishima" && shipped.CharacterName(15) == "Sergei Dragunov" &&
            shipped.FindCharacter("zbr") && shipped.FindCharacter("zbr")->name == "Reina", "Shipped retail data did not load beside executable");
        Check(shipped.FindName(0x0a27d720) && *shipped.FindName(0x0a27d720) == "Ycmn_dm_harai", "Shipped animation name hash failed");
        for (const auto& [id, code] : std::map<uint32_t, std::string>{{25,"crw"},{26,"jly"},{27,"aml"},
                {31,"bbn"},{33,"dog"},{34,"cbr"},{35,"bee"},{36,"okm"},{37,"kgr"},{38,"tgr"},{39,"knk"},{40,"wkz"},{41,"ker"},{42,"usi"},{43,"inu"}}) {
            const auto* character = shipped.FindCharacter(code);
            Check(character && character == shipped.FindRuntimeCharacter(id) && shipped.CharacterName(id) == character->name,
                "Observed runtime character did not resolve to its moveset code");
        }
        Check(shipped.FindCharacter(38) == shipped.FindCharacter("dog"), "Runtime correction changed animation asset identity");
        std::cout << "Static data tests passed: 53 fighters, 47095 names\n";
    } catch (...) { std::filesystem::remove_all(folder); throw; }
    std::filesystem::remove_all(folder);
}
