// Standalone retail T8 PANM export/import for the retail editor.
//
// Export writes embedded body PANMs (asset category 0) byte for byte.
// Import either reuses an identical embedded animation (native or custom) or stores the
// PANM as a custom asset (T7AnimToRetailT8::CustomAssetAddress) under a new instance of its
// own, with its own animation name: no other move's animation changes. The memory-only import
// registers it in the game's animation map under a free id, exactly like converted T7
// animations (GameSpecific/MovesetConverters/T7ToRetailT8Standalone.cpp).
#include "Editor_t8Retail.hpp"
#include "RetailNames.hpp"
#include "FileIO.hpp"
#include "RetailPanm.hpp"
#include "T7AnimToRetailT8.hpp"
#include "Animations.hpp"
#include <nlohmann/json.hpp>
#include <format>
#include <fstream>
#include <map>
#include <set>

namespace R = RetailT8Snapshot;
namespace
{
constexpr uint64_t MoveStride = 0x448;
constexpr uint64_t PanmFileLimit = 32ULL << 20;
// First instance given to an imported PANM (T7ToRetailT8Standalone numbers converted animations from here too).
constexpr uint32_t FirstCustomInstance = 0x00010000;
template <class T> T At(std::span<const Byte> bytes, size_t offset)
{
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
        throw std::out_of_range("Retail record extent");
    T value;
    memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}
template <class T> void Put(std::span<Byte> bytes, size_t offset, T value)
{
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
        throw std::out_of_range("Retail edit extent");
    memcpy(bytes.data() + offset, &value, sizeof(value));
}
size_t Align(size_t size)
{
    return (size + 7) & ~size_t(7);
}
std::string FileSafe(std::string text)
{
    for (auto &c : text)
        if (static_cast<unsigned char>(c) < 0x20 || std::string_view("<>:\"/\\|?*").find(c) != std::string_view::npos)
            c = '_';
    while (!text.empty() && (text.back() == '.' || text.back() == ' '))
        text.pop_back();
    if (text.size() > 96)
        text.resize(96);
    return text;
}
// Payload with the T8AN directory rewritten from `assets`; bytes[i] supplies asset i's data.
std::vector<Byte> WithAssets(std::span<const Byte> payload, const std::vector<R::AssetEntry> &assets,
                             const std::vector<std::span<const Byte>> &bytes)
{
    auto header = At<R::Header>(payload, 0);
    header.version = (std::max)(header.version, 2U); // tables-only documents gain an animation directory
    const auto last = At<R::Entry>(payload, header.directoryOffset + (R::Tables.size() - 1) * sizeof(R::Entry));
    const auto directoryOffset = Align(last.offset + last.count * last.stride);
    std::vector<Byte> result(payload.begin(), payload.begin() + directoryOffset);
    R::AssetDirectory directory{
        {'T', '8', 'A', 'N'}, uint32_t(assets.size()), directoryOffset + sizeof(R::AssetDirectory), 0, 0};
    directory.dataOffset = Align(directory.entriesOffset + assets.size() * sizeof(R::AssetEntry));
    result.resize(directory.dataOffset);
    Put(std::span<Byte>(result), directoryOffset, directory);
    for (size_t i = 0; i < assets.size(); ++i)
    {
        auto asset = assets[i];
        asset.offset = Align(result.size());
        asset.size = bytes[i].size();
        result.resize(asset.offset);
        result.insert(result.end(), bytes[i].begin(), bytes[i].end());
        Put(std::span<Byte>(result), directory.entriesOffset + i * sizeof(asset), asset);
    }
    header.size = result.size();
    Put(std::span<Byte>(result), 0, header);
    R::AppendExtension(result, R::ExtensionBytes(payload)); // v4 native-import data travels unchanged
    R::Validate(result);
    return result;
}
// Bounded walk over the native PANM FlatBuffers structure (root -> animation -> one bone group ->
// bones with one Transform track each). Unlike RetailPanm::DecodeRaw it accepts the game's
// compressed track encodings; it validates extents only, not compressed sample contents.
struct PanmShape
{
    uint32_t frames = 0;
    bool allRaw = true;
    std::set<std::string> bones;
};
PanmShape InspectPanm(std::span<const Byte> b)
{
    auto check = [](bool ok, const char *what) {
        if (!ok)
            throw std::runtime_error(std::string("Invalid PANM structure: ") + what);
    };
    auto u32 = [&](size_t at) { check(at <= b.size() && b.size() - at >= 4, "truncated field"); return At<uint32_t>(b, at); };
    auto field = [&](size_t table, size_t index) -> size_t {
        const auto vt = int64_t(table) - int64_t(int32_t(u32(table)));
        check(vt >= 8 && uint64_t(vt) + 4 <= b.size(), "vtable");
        const auto size = At<uint16_t>(b, size_t(vt)), object = At<uint16_t>(b, size_t(vt) + 2);
        check(size >= 4 && !(size & 1) && size_t(vt) + size <= b.size() && object >= 4 && table + object <= b.size(), "table extent");
        if (4 + 2 * index >= size)
            return 0;
        const auto offset = At<uint16_t>(b, size_t(vt) + 4 + 2 * index);
        check(!offset || offset + 4 <= object, "field offset");
        return offset ? table + offset : 0;
    };
    auto deref = [&](size_t at) {
        check(at != 0, "missing object");
        const auto relative = u32(at);
        check(relative && relative < b.size() - at, "object reference");
        return at + relative;
    };
    auto value = [&](size_t table, size_t index) { auto at = field(table, index); return at ? u32(at) : 0U; };
    auto count = [&](size_t vector, uint32_t limit) {
        const auto n = u32(vector);
        check(n && n <= limit && uint64_t(n) * 4 <= b.size() - vector - 4, "vector extent");
        return n;
    };
    auto text = [&](size_t at) {
        const auto n = u32(at);
        check(n && n <= 256 && at + 4 + n < b.size() && !b[at + 4 + n], "string extent");
        return std::string(reinterpret_cast<const char *>(b.data() + at + 4), n);
    };
    PanmShape shape;
    const size_t root = u32(0);
    check(root >= 8 && root < b.size(), "root reference");
    const auto anim = deref(field(root, 3));
    const uint64_t frames = uint64_t(value(anim, 1)) + 1;
    check(frames <= 1000000, "frame count");
    shape.frames = uint32_t(frames);
    const auto base = value(anim, 5), extent = value(anim, 6);
    check(base <= b.size() && extent == b.size() - base, "sample extent");
    const auto groups = deref(field(anim, 4));
    check(count(groups, 1) == 1, "bone group count");
    const auto bones = deref(field(deref(groups + 4), 2));
    for (uint32_t i = 0, n = count(bones, 255); i < n; ++i)
    {
        const auto bone = deref(bones + 4 + i * 4);
        check(shape.bones.insert(text(deref(field(bone, 1)))).second, "duplicate bone");
        const auto tracks = deref(field(bone, 2));
        check(count(tracks, 1) == 1, "bone track count");
        const auto track = deref(tracks + 4);
        check(text(deref(field(track, 1))) == "Transform" && value(track, 2) == 1, "track type");
        const auto encoding = value(track, 3), samples = value(track, 4), offset = value(track, 6), size = value(track, 7);
        check(encoding && samples && samples <= frames && size && offset <= extent && size <= extent - offset, "track samples");
        shape.allRaw &= encoding == 1 || encoding == 2;
    }
    return shape;
}
} // namespace

std::vector<Byte> EditorT8Retail::ReadAnimationFile(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("Cannot open animation file");
    const auto size = file.tellg();
    if (size < 8 || uint64_t(size) > PanmFileLimit)
        throw std::runtime_error("Animation file exceeds animation bounds");
    std::vector<Byte> bytes(static_cast<size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char *>(bytes.data()), bytes.size()))
        throw std::runtime_error("Cannot read complete animation file");
    if (!memcmp(bytes.data() + 4, "PANM", 4))
        return ReadPanmFile(path);
    // Tekken 7: 0x64 (compressed) or 0xC8 (raw) body animation, little- or big-endian.
    const auto type = bytes[0] ? bytes[0] : bytes[1];
    if (type != 0x64 && type != 0xC8)
        throw std::runtime_error("Not a retail PANM or a Tekken 7 0x64 / 0xC8 animation");
    const auto extent = TAnimUtils::FromMemory::Safe::GetAnimSize(bytes.data(), bytes.size());
    if (!extent)
        throw std::runtime_error("Invalid Tekken 7 animation");
    bytes.resize(static_cast<size_t>(extent));
    if (!TAnimUtils::FromMemory::IsLittleEndian(bytes.data()) && !TAnimUtils::FromMemory::ByteswapAnimation(bytes.data(), bytes.size()))
        throw std::runtime_error("Tekken 7 animation byte-order conversion failed");
    return T7AnimToRetailT8::Convert(RetailPanm::DecodeT7(bytes, T7AnimToRetailT8::MaxConvertibleFrames()));
}

std::vector<Byte> EditorT8Retail::ReadPanmFile(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("Cannot open animation file");
    const auto size = file.tellg();
    if (size < 8 || uint64_t(size) > PanmFileLimit)
        throw std::runtime_error("PANM file exceeds animation bounds");
    std::vector<Byte> bytes(static_cast<size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char *>(bytes.data()), bytes.size()))
        throw std::runtime_error("Cannot read complete animation file");
    if (R::PanmSize(bytes) != bytes.size())
        throw std::runtime_error("PANM extent differs from file size");
    return bytes;
}

void EditorT8Retail::ValidatePanm(std::span<const Byte> panm)
{
    if (panm.size() < 8 || panm.size() > PanmFileLimit || R::PanmSize(panm) != panm.size())
        throw std::runtime_error("PANM extent differs from file size");
    const auto shape = InspectPanm(panm);
    if (!shape.bones.count("Top") || !shape.bones.count("Trans") || shape.bones.size() < 20)
        throw std::runtime_error("PANM is not a body animation (needs Top/Trans and at least 20 bones)");
    // Compressed tracks only come from the game's own tools: keep those files byte for byte.
    if (!shape.allRaw)
        return;
    RetailPanm::Animation decoded;
    try
    {
        decoded = RetailPanm::DecodeRaw(panm);
    }
    catch (const std::exception &error)
    {
        throw std::runtime_error(std::string("PANM cannot be used as a custom animation: ") + error.what());
    }
    // Raw PANMs need the native body structure (also what the converters use as template). Native files and
    // RetailPanm::Encode output share the layout the game's decoder accepts (.docs/retail-t8-panm-layout.md).
    if (!T7AnimToRetailT8::UsableTemplate(panm))
        throw std::runtime_error("PANM is not a native body animation (needs one bone group with Top/Trans and "
                                    "at least 20 raw/static tracks)");
}

std::vector<Byte> EditorT8Retail::RewritePanm(std::span<const Byte> panm) const
{
    const auto source = RetailPanm::DecodeRaw(panm);
    auto payload = std::span<const Byte>(m_movesetData, static_cast<size_t>(m_movesetDataSize));
    std::optional<uint32_t> idle;
    try
    {
        idle = AnimationInstance(ResolveMove(0x8001));
    }
    catch (const std::exception &)
    {
    }
    // Prefer the idle stance (unmatched bones keep its frame-0 pose), as the T7 converter does.
    std::span<const Byte> chosen;
    for (const auto &asset : R::Assets(payload))
        if (asset.type == 0 && asset.size && (chosen.empty() || asset.instance == idle))
        {
            auto bytes = payload.subspan(asset.offset, asset.size);
            if (T7AnimToRetailT8::UsableTemplate(bytes))
            {
                chosen = bytes;
                if (asset.instance == idle)
                    break;
            }
        }
    if (chosen.empty())
        throw std::runtime_error("the document embeds no native raw body animation to use as template");
    auto result = T7AnimToRetailT8::Rewrite(source, chosen);
    ValidatePanm(result);
    return result;
}

std::optional<uint32_t> EditorT8Retail::AnimationInstance(uint32_t row) const
{
    const auto handle = At<uint64_t>(Record(12, row), 0x50);
    // Native moves keep the captured resource handle (nonzero high word, instance in the low word).
    // Converted T7 moves only hold a placeholder resolved at import.
    if (!(handle >> 32))
        return std::nullopt;
    return uint32_t(handle);
}

std::vector<Byte> EditorT8Retail::MoveAnimation(uint32_t row) const
{
    auto instance = AnimationInstance(row);
    if (!instance)
        throw std::runtime_error("This move's animation resource is only resolved at import (converted moveset)");
    auto payload = std::span<const Byte>(m_movesetData, static_cast<size_t>(m_movesetDataSize));
    for (const auto &asset : R::Assets(payload))
        if (asset.type == 0 && asset.instance == *instance)
        {
            if (!asset.size)
                throw std::runtime_error("This move's animation was not loaded when the moveset was exported");
            auto bytes = payload.subspan(asset.offset, asset.size);
            return {bytes.begin(), bytes.end()};
        }
    throw std::runtime_error("This move's animation is not embedded in the document; export with runtime assets");
}

std::optional<uint32_t> EditorT8Retail::FindBoundAnimation(std::span<const Byte> panm) const
{
    auto payload = std::span<const Byte>(m_movesetData, static_cast<size_t>(m_movesetDataSize));
    std::set<uint32_t> instances;
    for (const auto &asset : R::Assets(payload))
        if (asset.type == 0 && asset.size == panm.size() && !memcmp(payload.data() + asset.offset, panm.data(), panm.size()))
            instances.insert(asset.instance);
    if (!instances.empty())
        for (uint32_t row = 0; row < Count(12); ++row)
            if (auto instance = AnimationInstance(row); instance && instances.count(*instance))
                return row;
    return std::nullopt;
}

size_t EditorT8Retail::ExportAnimations(const std::filesystem::path &directory,
                                        const std::function<std::string(uint32_t)> &name) const
{
    auto payload = std::span<const Byte>(m_movesetData, static_cast<size_t>(m_movesetDataSize));
    std::map<uint32_t, R::AssetEntry> assets;
    for (const auto &asset : R::Assets(payload))
        if (asset.type == 0 && asset.size)
            assets.emplace(asset.instance, asset);
    std::map<uint32_t, std::vector<uint32_t>> users; // instance -> move rows
    for (uint32_t row = 0; row < Count(12); ++row)
        if (auto instance = AnimationInstance(row); instance && assets.count(*instance))
            users[*instance].push_back(row);
    if (users.empty())
        throw std::runtime_error("No move uses an embedded body animation; export with runtime assets first");
    if (!std::filesystem::create_directory(directory))
        throw std::runtime_error("Animation output folder must be new");
    nlohmann::json manifest = nlohmann::json::array();
    std::set<std::string> used;
    for (const auto &[instance, rows] : users)
    {
        const auto &asset = assets.at(instance);
        auto record = Record(12, rows.front());
        const auto hash = Decode(At<uint64_t>(record, 0x20), At<uint64_t>(record, 0x28)).value_or(0);
        auto label = FileSafe(name ? name(hash) : std::string());
        auto file = (label.empty() ? std::format("anim_{:08X}", hash) : label) + std::format("_{:08X}.panm", instance);
        for (unsigned suffix = 2; !used.insert(file).second; ++suffix)
            file = std::format("anim_{:08X}_{:08X}_{}.panm", hash, instance, suffix);
        if (!FileIO::WriteAtomically(directory / std::filesystem::u8path(file), payload.subspan(asset.offset, asset.size)))
            throw std::runtime_error("Cannot write exported PANM");
        manifest.push_back({{"file", file}, {"instance", instance}, {"animation_hash", hash}, {"bytes", asset.size},
                            {"custom", asset.address == T7AnimToRetailT8::CustomAssetAddress}, {"moves", rows}});
    }
    if (!FileIO::WriteAtomically(directory / "animations.json", manifest.dump(2) + "\n"))
        throw std::runtime_error("Cannot write animation manifest");
    return users.size();
}

EditorT8Retail::PanmImport EditorT8Retail::ImportPanm(uint32_t row, std::span<const Byte> panm, std::string_view name)
{
    auto own = Record(12, row);
    if (R::PanmSize(panm) != panm.size())
        throw std::runtime_error("PANM extent differs from file size");
    PanmImport result{row};
    if (auto bound = FindBoundAnimation(panm))
    {
        // Same bytes already embedded: bind to it like the animation library does.
        if (*bound != row)
            SetAnimation(row, *bound);
        result.instance = *AnimationInstance(*bound);
        return result;
    }
    std::vector<Byte> rewritten;
    try
    {
        ValidatePanm(panm);
    }
    catch (const std::exception &error)
    {
        try
        {
            rewritten = RewritePanm(panm);
        }
        catch (const std::exception &rewrite)
        {
            throw std::runtime_error(std::string(error.what()) + "; it cannot be rewritten either: " + rewrite.what());
        }
        panm = rewritten;
        result.rewritten = true;
    }
    auto payload = std::span<const Byte>(m_movesetData, static_cast<size_t>(m_movesetDataSize));
    // Body handles keep their high word (character domain); the low word is the instance.
    const auto ownHandle = At<uint64_t>(own, 0x50);
    uint64_t domain = ownHandle >> 32 ? ownHandle & 0xffffffff00000000ULL : 0;
    for (uint32_t other = 0; !domain && other < Count(12); ++other)
        if (const auto handle = At<uint64_t>(Record(12, other), 0x50); handle >> 32)
            domain = handle & 0xffffffff00000000ULL;
    if (!domain)
        throw std::runtime_error("No move of this document has a resolved animation resource (converted with an older "
                                 "version: run repair-handles-t8 with its donor export)");
    auto assets = R::Assets(payload);
    std::set<uint32_t> taken; // every instance a move or an asset entry uses
    for (const auto &asset : assets)
        taken.insert(asset.instance);
    std::vector<uint32_t> users; // moves on this move's instance
    const auto ownInstance = AnimationInstance(row);
    for (uint32_t other = 0; other < Count(12); ++other)
        if (auto instance = AnimationInstance(other))
        {
            taken.insert(*instance);
            if (instance == ownInstance)
                users.push_back(other);
        }
    // The move's own custom animation, used by no other move, is replaced where it is. Anything else (a native
    // animation, or one shared with other moves) stays untouched: the PANM gets a new instance of its own, which the
    // memory-only import registers under a free id of the game's animation map.
    auto found = std::find_if(assets.begin(), assets.end(), [&](const auto &asset) {
        return ownInstance && asset.type == 0 && asset.instance == *ownInstance && asset.address == T7AnimToRetailT8::CustomAssetAddress;
    });
    result.replaced = found != assets.end() && users.size() == 1 && users.front() == row;
    uint32_t instance = result.replaced ? *ownInstance : 0;
    if (!result.replaced)
    {
        // Past every instance in use (the converter numbers its animations from the same base).
        const auto highest = (std::max)(taken.empty() ? 0U : *taken.rbegin(), FirstCustomInstance - 1);
        if (highest != UINT32_MAX)
            instance = highest + 1;
        else
            for (instance = FirstCustomInstance; taken.count(instance);)
                if (++instance == 0)
                    throw std::runtime_error("No free animation instance");
        assets.push_back({T7AnimToRetailT8::CustomAssetAddress, 0, 0, instance, 0});
        found = assets.end() - 1;
    }
    // Its own animation name (key +0x20), so the animation library lists it apart from the one it replaces.
    auto metadata = *m_metadata;
    std::set<uint32_t> usedHashes;
    for (uint32_t other = 0; other < Count(12); ++other)
        if (other != row)
            if (auto hash = Decode(At<uint64_t>(Record(12, other), 0x20), At<uint64_t>(Record(12, other), 0x28)))
                usedHashes.insert(*hash);
    std::string base = name.empty() ? std::string("custom animation") : std::string(name.substr(0, 240));
    std::string animationName = base;
    for (unsigned suffix = 2;; ++suffix)
    {
        const auto hash = RetailNames::Hash(animationName);
        const auto named = metadata.names.find(hash);
        const auto filed = metadata.fileNames.find(hash);
        if (!usedHashes.count(hash) && (named == metadata.names.end() || named->second == animationName) &&
            (filed == metadata.fileNames.end() || filed->second == animationName))
            break;
        animationName = std::format("{} ({})", base, suffix);
    }
    const auto hash = RetailNames::Hash(animationName);
    metadata.names[hash] = animationName;
    const auto frames = InspectPanm(panm).frames;
    MutateBatch([&] {
        std::vector<std::span<const Byte>> bytes;
        for (const auto &asset : assets)
            bytes.push_back(&asset == &*found ? panm : asset.size ? payload.subspan(asset.offset, asset.size) : std::span<const Byte>());
        const auto key = At<uint64_t>(own, 0x28);
        Publish(WithAssets(payload, assets, bytes), metadata);
        auto *move = m_movesetData + m_entries[12].offset + size_t(row) * MoveStride;
        auto record = std::span<Byte>(move, MoveStride);
        Put(record, 0x20, Encode(hash, key));
        SetPlainCopy(move + 0x20, row, hash);
        Put(record, 0x50, domain | instance);
        Put(record, 0x120, frames);
    });
    result.instance = instance;
    result.custom = true;
    result.name = animationName;
    return result;
}

size_t EditorT8Retail::AdoptAnimationHandles(std::span<const Byte> donorMoves)
{
    if (donorMoves.size() % MoveStride)
        throw std::invalid_argument("Donor move table dimensions differ");
    std::map<uint32_t, uint64_t> handles; // animation hash -> native body handle
    for (size_t at = 0; at < donorMoves.size(); at += MoveStride)
    {
        auto move = donorMoves.subspan(at, MoveStride);
        const auto handle = At<uint64_t>(move, 0x50);
        if (auto hash = Decode(At<uint64_t>(move, 0x20), At<uint64_t>(move, 0x28)); hash && (handle >> 32))
            handles.try_emplace(*hash, handle);
    }
    std::vector<std::pair<uint32_t, uint64_t>> changes;
    for (uint32_t row = 0; row < Count(12); ++row)
    {
        auto record = Record(12, row);
        if (At<uint64_t>(record, 0x50) >> 32)
            continue;
        if (auto hash = Decode(At<uint64_t>(record, 0x20), At<uint64_t>(record, 0x28)))
            if (auto found = handles.find(*hash); found != handles.end())
                changes.push_back({row, found->second});
    }
    if (!changes.empty())
        Mutate([&] {
            for (auto [row, handle] : changes)
                Put(std::span<Byte>(m_movesetData + m_entries[12].offset + row * MoveStride, MoveStride), 0x50, handle);
        });
    return changes.size();
}

uint32_t EditorT8Retail::PanmFrames(std::span<const Byte> panm)
{
    return InspectPanm(panm).frames;
}
