#include "MovesetFolders.hpp"
#include "Settings.hpp"
#include "constants.h"
#include <nlohmann/json.hpp>
#include <mutex>
#include <set>
#include <cwctype>

namespace MovesetFolders {
namespace {
std::mutex mutex;
Configuration current;
constexpr auto key="moveset_folders_v1";
void Validate(const std::string& value) {
    if (!std::filesystem::u8path(value).is_absolute()) throw std::invalid_argument("Choose an absolute folder path.");
    if (value.find_first_of("\r\n")!=value.npos) throw std::invalid_argument("Invalid folder path.");
}
std::string Encode(const Configuration& c) {
    const auto json=nlohmann::json{{"extraction",c.extraction},{"additional",c.additional}}.dump();
    constexpr char hex[]="0123456789abcdef"; std::string encoded;
    for (unsigned char c:json) { encoded+=hex[c>>4]; encoded+=hex[c&15]; }
    return encoded; // Survives INI comments, backslashes and Unicode paths.
}
}
Configuration Get() { std::lock_guard lock(mutex); return current; }
void Load() {
    Configuration result;
    try {
        std::string encoded=Settings::Get(key,"");
        if (!encoded.empty()) {
            if (encoded.size()%2) throw std::invalid_argument("Invalid folder settings");
            auto nibble=[](char c)->int { if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::invalid_argument("Invalid folder settings"); };
            std::string decoded;
            for(size_t i=0;i<encoded.size();i+=2)decoded+=char(nibble(encoded[i])*16+nibble(encoded[i+1]));
            auto json=nlohmann::json::parse(decoded);
            result.extraction=json.at("extraction").get<std::string>();
            result.additional=json.at("additional").get<std::vector<std::string>>();
            if (!result.extraction.empty()) Validate(result.extraction);
            for (const auto& path:result.additional) Validate(path);
        }
    } catch (const std::exception&) { result={}; }
    std::lock_guard lock(mutex); current=std::move(result);
}
void Save(Configuration configuration) {
    if (!configuration.extraction.empty()) Validate(configuration.extraction);
    for (const auto& path:configuration.additional) Validate(path);
    auto encoded=Encode(configuration);
    Settings::Set(key,encoded.c_str());
    std::lock_guard lock(mutex); current=std::move(configuration);
}
std::filesystem::path ExtractionDirectory() {
    auto c=Get(); return c.extraction.empty() ? std::filesystem::path(MOVESET_DIRECTORY) : std::filesystem::u8path(c.extraction);
}
std::vector<std::filesystem::path> SearchDirectories() {
    auto c=Get(); std::vector<std::filesystem::path> result; std::set<std::wstring> seen;
    auto add=[&](std::filesystem::path path) {
        std::error_code ec; auto normalized=std::filesystem::weakly_canonical(path,ec);
        if(ec) { ec.clear();normalized=std::filesystem::absolute(path,ec).lexically_normal(); }
        if(ec)return;
        auto key=normalized.wstring(); for(auto& c:key)c=std::towlower(c);
        if(seen.insert(key).second)result.push_back(normalized);
    };
    add(MOVESET_DIRECTORY);
    if(!c.extraction.empty())add(std::filesystem::u8path(c.extraction));
    for(const auto& path:c.additional)add(std::filesystem::u8path(path));
    return result;
}
bool IsInSearchDirectory(const std::filesystem::path& file) {
    // Same normalization as SearchDirectories(), compared case-insensitively like Windows paths.
    auto lower=[](std::wstring text){for(auto& c:text)c=std::towlower(c);return text;};
    std::error_code ec; auto parent=std::filesystem::weakly_canonical(file,ec).parent_path();
    if(ec)return false;
    const auto key=lower(parent.wstring());
    for(const auto& directory:SearchDirectories())if(lower(directory.wstring())==key)return true;
    return false;
}
}
