#include "AnimExtractor_t8.hpp"
#include "FileIO.hpp"
#include "GameStaticData.hpp"
#include "Editor_t8Retail/Editor_t8Retail.hpp"
#include <map>
#include <set>
#include <nlohmann/json.hpp>
#include <format>
#include <algorithm>
#include <cctype>
#include <cstring>
namespace AnimExtractorPaths {
namespace {
// Length of the valid UTF-8 sequence of a BMP character at value[at]; 0 when invalid or outside the BMP
size_t BmpSequenceLength(std::string_view value, size_t at) {
    const auto lead = static_cast<unsigned char>(value[at]);
    if (lead < 0x80) return 1;
    size_t length; uint32_t codepoint;
    if ((lead & 0xE0) == 0xC0) { length = 2; codepoint = lead & 0x1F; }
    else if ((lead & 0xF0) == 0xE0) { length = 3; codepoint = lead & 0x0F; }
    else return 0; // Continuation byte, 4-byte sequence (outside the BMP) or invalid lead byte
    if (value.size() - at < length) return 0;
    for (size_t k = 1; k < length; ++k) {
        const auto next = static_cast<unsigned char>(value[at + k]);
        if ((next & 0xC0) != 0x80) return 0;
        codepoint = (codepoint << 6) | (next & 0x3F);
    }
    if ((length == 2 && codepoint < 0x80) || (length == 3 && codepoint < 0x800) || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) return 0;
    return length;
}
}
std::string SafeComponent(std::string_view value, size_t maxBytes) {
    if (maxBytes < 2) maxBytes = 2;
    std::string result;
    for (size_t at = 0; at < value.size();) {
        const auto length = BmpSequenceLength(value, at);
        if (result.size() + (length ? length : 1) > maxBytes) break;
        if (length == 0) { result += '_'; ++at; continue; }
        if (length == 1) {
            const char c = value[at];
            result += static_cast<unsigned char>(c) < 0x20 || c == 0x7F || strchr("<>:\"/\\|?*", c) ? '_' : c;
        }
        else result.append(value.substr(at, length));
        at += length;
    }
    // Windows drops a trailing dot or space ("..", "name."): never end on one
    if (!result.empty() && (result.back() == '.' || result.back() == ' ')) result.back() = '_';
    if (result.empty()) result = "_";
    // Device names are reserved whatever their extension ("CON", "nul.txt", "COM1 .anm")
    std::string base = result.substr(0, result.find('.'));
    while (!base.empty() && base.back() == ' ') base.pop_back();
    for (auto& c : base) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    bool reserved = base == "CON" || base == "PRN" || base == "AUX" || base == "NUL" || base == "CONIN$" || base == "CONOUT$";
    if (base.size() == 4 && (base.starts_with("COM") || base.starts_with("LPT")) && base[3] >= '0' && base[3] <= '9') reserved = true;
    if (reserved) result.insert(result.begin(), '_');
    return result;
}
}
namespace RetailT8Animations {
namespace {
// Body animation id -> the animation name hash of the moves playing it (encrypted field +0x20, memory form).
// Animations no move plays (hands, face, ...) have no name source.
std::map<uint32_t, uint32_t> AnimationHashes(std::span<const Byte> payload) {
    std::map<uint32_t, uint32_t> hashes;
    const auto moves = RetailT8Snapshot::Table(payload, 12);
    for (size_t row = 0; row + 0x448 <= moves.size(); row += 0x448) {
        uint64_t handle, value, key;
        memcpy(&handle, moves.data() + row + 0x50, 8); memcpy(&value, moves.data() + row + 0x20, 8); memcpy(&key, moves.data() + row + 0x28, 8);
        if (!(handle >> 32)) continue;
        if (const auto hash = EditorT8Retail::Decode(value, key)) hashes.try_emplace(static_cast<uint32_t>(handle), *hash);
    }
    return hashes;
}
std::string SafeFileName(const std::string& value) {
    // Backslash included ("\\": "\|" was an invalid escape read as '|'), plus "..", reserved names and invalid UTF-8
    return AnimExtractorPaths::SafeComponent(value);
}
// Stages every file in "<directory>.partial" and renames it once complete. `stop` is polled per animation.
template<class Stop, class Progress>
size_t WriteAnimations(std::span<const Byte> payload, const std::vector<Animation>& selection, const std::filesystem::path& directory,
    Stop stop, Progress progress) {
    if(selection.empty())throw std::runtime_error("No animation selected");
    if(std::filesystem::exists(directory))throw std::runtime_error("Animation output folder must be new");
    std::map<std::pair<uint8_t,uint32_t>,RetailT8Snapshot::AssetEntry> assets;
    for(const auto& asset:RetailT8Snapshot::Assets(payload))if(asset.size && asset.type<6)assets.try_emplace({asset.type,asset.instance},asset);
    auto partial=directory;partial+=L".partial";
    std::error_code error;std::filesystem::remove_all(partial,error);
    if(!std::filesystem::create_directories(partial))throw std::runtime_error("Cannot create animation output folder");
    nlohmann::json manifest=nlohmann::json::array();size_t count=0;
    try {
        for(const auto& animation:selection) {
            if(stop()){std::filesystem::remove_all(partial,error);return 0;}
            const auto found=assets.find({animation.category,animation.instance});
            if(found==assets.end())throw std::runtime_error(std::format("Animation {}_{:08X} is not embedded in this moveset",Categories[animation.category%6],animation.instance));
            const auto& asset=found->second;
            if(!FileIO::WriteNew(partial/std::filesystem::u8path(animation.file),payload.subspan(asset.offset,asset.size)))
                throw std::runtime_error("Cannot write extracted PANM "+animation.file);
            manifest.push_back({{"category",Categories[asset.type]},{"instance",asset.instance},{"bytes",asset.size},{"file",animation.file},{"name",animation.name}});
            progress(++count);
        }
        if(!FileIO::WriteNew(partial/"animations.json",manifest.dump(2)+"\n"))throw std::runtime_error("Cannot write animation manifest");
        std::filesystem::rename(partial,directory);
    } catch(...) { std::filesystem::remove_all(partial,error); throw; }
    return count;
}
}
std::vector<Animation> List(std::span<const Byte> payload, const std::function<std::string(uint32_t)>& nameForHash) {
    std::function<std::string(uint32_t)> lookup=nameForHash;
    if(!lookup) {
        auto dictionary=std::make_shared<GameStaticData>(GameStaticData::LoadOptional("t8-retail"));
        lookup=[dictionary](uint32_t hash){const auto* name=dictionary->FindName(hash);return name?*name:std::string();};
    }
    const auto hashes=AnimationHashes(payload);
    std::vector<Animation> result;std::set<std::string> taken;
    for(const auto& asset:RetailT8Snapshot::Assets(payload)) {
        if(!asset.size || asset.type>=6)continue;
        Animation animation{asset.type,asset.instance,static_cast<uint32_t>(asset.size)};
        if(asset.type==0)if(const auto hash=hashes.find(asset.instance);hash!=hashes.end())animation.name=lookup(hash->second);
        // Named after the animation when a move plays it (body), else by category and id; a repeated name gets its id.
        animation.file=!animation.name.empty()?SafeFileName(animation.name)+".panm":std::format("{}_{:08X}.panm",Categories[asset.type],asset.instance);
        if(!taken.insert(animation.file).second){
            animation.file=std::format("{}_{:08X}.panm",animation.file.substr(0,animation.file.size()-5),asset.instance);
            taken.insert(animation.file);
        }
        result.push_back(std::move(animation));
    }
    return result;
}
size_t Extract(std::span<const Byte> payload, const std::vector<Animation>& selection, const std::filesystem::path& directory,
    const std::atomic<bool>* interrupt, std::atomic<uint32_t>* done) {
    if(done)*done=0;
    return WriteAnimations(payload,selection,directory,[&]{return interrupt && interrupt->load();},[&](size_t count){if(done)*done=static_cast<uint32_t>(count);});
}
size_t Extract(std::span<const Byte> payload,const std::filesystem::path& directory,const bool* interrupt,uint32_t* done,uint32_t* total) {
    const auto animations=List(payload);
    if(animations.empty())throw std::runtime_error("No embedded animations; export the required animation categories first");
    if(total)*total=static_cast<uint32_t>(animations.size());
    return WriteAnimations(payload,animations,directory,[&]{return interrupt && *interrupt;},[&](size_t count){if(done)*done=static_cast<uint32_t>(count);});
}
bool ExtractArchive(const RetailT8GameData::Anmbin& archive,const std::filesystem::path& directory,
    const std::atomic<bool>* interrupt,std::atomic<uint32_t>* done,std::atomic<uint32_t>* total) {
    if(std::filesystem::exists(directory))throw std::runtime_error("Animation output folder must be new");
    auto partial=directory;partial+=L".partial";
    std::error_code error;std::filesystem::remove_all(partial,error);
    if(!std::filesystem::create_directories(partial))throw std::runtime_error("Cannot create animation output folder");
    constexpr const char* categories[]={"body","hands","face","swing","camera","extra"};
    const auto identities=archive.Identities();
    if(total)*total=static_cast<uint32_t>(identities.size());
    if(done)*done=0;
    nlohmann::json manifest=nlohmann::json::array();
    try {
        for(const auto& [type,instance]:identities) {
            if(interrupt && *interrupt){std::filesystem::remove_all(partial,error);return false;}
            const auto panm=archive.Panm(type,instance);
            if(!panm || type>=6)continue;
            auto name=std::format("{}_{:08X}.panm",categories[type],instance);
            if(!FileIO::WriteNew(partial/name,*panm))throw std::runtime_error("Cannot write extracted PANM");
            manifest.push_back({{"category",categories[type]},{"instance",instance},{"bytes",panm->size()},{"file",name},{"source","tkdata.bin"}});
            if(done)++*done;
        }
        if(!FileIO::WriteNew(partial/"animations.json",manifest.dump(2)+"\n"))throw std::runtime_error("Cannot write animation manifest");
        std::filesystem::rename(partial,directory);
    } catch(...) { std::filesystem::remove_all(partial,error); throw; }
    return true;
}
}
