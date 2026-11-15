#include "RetailAnimationArchive.hpp"
#include <fstream>
#include <map>
#include <cstring>

namespace RetailT8Resources {
namespace {
template<class T> T At(std::span<const Byte> bytes, size_t offset) {
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset) throw std::runtime_error("Truncated ANMBIN field");
    T value; memcpy(&value, bytes.data() + offset, sizeof(value)); return value;
}
class Archive {
    std::ifstream file;
    uint64_t size;
public:
    std::map<std::pair<uint32_t, uint32_t>, std::vector<uint64_t>> entries;
    std::vector<Byte> Read(uint64_t at, size_t length) {
        if (at > size || length > size - at) throw std::runtime_error("ANMBIN extent is outside file");
        file.clear(); file.seekg(at);
        std::vector<Byte> bytes(length);
        if (!file.read(reinterpret_cast<char*>(bytes.data()), length)) throw std::runtime_error("Cannot read ANMBIN region");
        return bytes;
    }
    explicit Archive(const std::filesystem::path& path) : file(path, std::ios::binary), size(std::filesystem::file_size(path)) {
        if (!file || size < 0x98 || size > (2ULL << 30)) throw std::runtime_error("Invalid ANMBIN file size");
        auto header = Read(0, 0x98);
        for (uint32_t type = 0; type < 6; ++type) {
            auto count = At<uint32_t>(header, 4 + 4 * type), references = At<uint32_t>(header, 0x1c + 4 * type);
            auto pool = At<uint64_t>(header, 0x38 + 8 * type), listing = At<uint64_t>(header, 0x68 + 8 * type);
            if (count > 100000 || references > 100000 || listing > size || references * 4ULL > size - listing)
                throw std::runtime_error("Invalid ANMBIN category bounds");
            auto records = Read(pool, count * 0x38);
            for (size_t i = 0; i < count; ++i) {
                auto instance = static_cast<uint32_t>(At<uint64_t>(records, i * 0x38));
                auto offset = At<uint64_t>(records, i * 0x38 + 8);
                if (!offset) continue; // shared reference, never a blob at file offset zero
                if (offset > size || size - offset < 8) throw std::runtime_error("Invalid ANMBIN PANM offset");
                entries[{type, instance}].push_back(offset);
            }
        }
    }
    bool Match(uint32_t type, uint32_t instance, std::span<const Byte> expected) {
        auto found = entries.find({type, instance}); if (found == entries.end()) return false;
        for (auto offset : found->second) {
            uint64_t extent = 0;
            for (size_t length = 128; length <= (1 << 20); length *= 4) {
                auto prefix = Read(offset, (std::min)(length, static_cast<size_t>(size - offset)));
                try { extent = RetailT8Snapshot::PanmSize(prefix); break; } catch (const std::runtime_error&) {}
                if (prefix.size() < length) break;
            }
            if (extent != expected.size() || extent > size - offset) continue;
            bool equal = true;
            for (size_t at = 0; at < expected.size(); at += 1 << 20) {
                auto length = (std::min)(size_t(1 << 20), expected.size() - at);
                auto bytes = Read(offset + at, length);
                if (memcmp(bytes.data(), expected.data() + at, length)) { equal = false; break; }
            }
            if (equal) return true;
        }
        return false;
    }
};
}
ArchiveReport VerifyArchives(std::span<const Byte> snapshot, const std::filesystem::path& directory, const std::string& code) {
    if (code.empty() || code.size() > 16 || code.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != code.npos)
        throw std::runtime_error("Invalid ANMBIN character code");
    const auto assets = RetailT8Snapshot::Assets(snapshot);
    if (assets.empty()) throw std::runtime_error("ANMBIN loading requires a snapshot with captured animation identities/data");
    Archive common(directory / "com.anmbin"), character(directory / (code + ".anmbin"));
    ArchiveReport result;
    for (const auto& asset : assets) {
        if (!asset.size) continue;
        auto bytes = snapshot.subspan(asset.offset, asset.size);
        if (!common.Match(asset.type, asset.instance, bytes) && !character.Match(asset.type, asset.instance, bytes))
            throw std::runtime_error("Snapshot PANM does not match character/common ANMBIN data");
        ++result.blobs; result.bytes += asset.size;
    }
    if (!result.blobs) throw std::runtime_error("Snapshot has no available animations to validate");
    return result;
}
}
