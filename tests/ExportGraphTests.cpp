#include "Extractor_t7/T7ExportSnapshot.hpp"
#include "Extractor_t7/Extractor_t7.hpp"
#include "Importer_t7/Importer_t7_Utils.hpp"
#include "Editor_t7/Editor_t7.hpp"
#include "MovesetFile.hpp"
#include "GameIDs.hpp"
#include "FileIO.hpp"
#include "Animations.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <iostream>
#include <nlohmann/json.hpp>

std::vector<Byte> SemanticFixture();
namespace {
namespace G = StructsT7_gameAddr;
using namespace StructsT7;
namespace fs = std::filesystem;
void Check(bool value, const char* error) { if (!value) throw std::runtime_error(error); }
std::vector<Byte> ReadFile(const fs::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    Check(static_cast<bool>(input), "cannot read export test file");
    auto size = input.tellg(); Check(size >= 0, "cannot size export test file");
    std::vector<Byte> bytes(static_cast<size_t>(size)); input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    Check(static_cast<bool>(input), "short export test file read"); return bytes;
}
class Memory : public GameProcess {
public:
    std::map<gameAddr, std::vector<Byte>> regions;
    mutable std::vector<std::pair<gameAddr, size_t>> reads;
    gameAddr unavailable = 0, changingHeader = 0, changingPointer = 0;
    mutable int headerReads = 0, pointerReads = 0;
protected:
    bool ReadMemoryRaw(gameAddr address, void* data, size_t size) const override {
        reads.emplace_back(address, size);
        auto it = regions.upper_bound(address);
        if (it == regions.begin()) return false;
        --it;
        if (it->first == unavailable || address - it->first > it->second.size() ||
            size > it->second.size() - (address - it->first)) return false;
        memcpy(data, it->second.data() + address - it->first, size);
        if (address == changingHeader && size == 0x2e0 && ++headerReads > 1)
            static_cast<Byte*>(data)[offsetof(G::MovesetInfo, table)] ^= 8;
        if (address == changingPointer && size == sizeof(gameAddr) && ++pointerReads > 1)
            static_cast<Byte*>(data)[0] ^= 8;
        return true;
    }
};
constexpr gameAddr Header = 0x980000000ULL, Player = 0x880000000ULL, Training = 0x780000000ULL;
constexpr ExtractSettings Flags = 7 | ExtractSettings_DisplayableMovelist;
struct Graph {
    Memory memory;
    G::MovesetInfo info{};
    gameAddr animation = 0, mota = 0;
    // Lists are reversed, separated by gigabytes, and interleaved with the header.
    explicit Graph(bool reversed = true) {
        auto file = SemanticFixture();
        auto h = reinterpret_cast<const TKMovesetHeader*>(file.data());
        auto b = reinterpret_cast<const TKMovesetHeaderBlocks*>(file.data() + h->block_list);
        auto payload = file.data() + h->moveset_data_start;
        info = *reinterpret_cast<const G::MovesetInfo*>(payload);
        auto original = info.table;
        for (size_t i = 0; i < T7Export::RecordSizes.size(); ++i) {
            auto address = 0x100000000ULL + (reversed ? 18 - i : i) * 0x100000000ULL;
            auto size = T7Export::RecordSizes[i] * original.entries[i].listCount;
            memory.regions[address] = {payload + b->movesetBlock + original.entries[i].listAddr,
                payload + b->movesetBlock + original.entries[i].listAddr + size};
            info.table.entries[i].listAddr = address;
        }
        // Old Python imports do not populate the two beginning/ending property lists.
        memory.regions.erase(info.table.moveBeginningProp);
        memory.regions.erase(info.table.moveEndingProp);
        info.table.moveBeginningPropCount = info.table.moveEndingPropCount = 0;
        info.table.moveBeginningProp = info.table.moveEndingProp = UINT64_MAX;
        gameAddr next = 0x3000000000ULL;
        auto text = [&](const std::string& value) {
            // NUL at a page boundary exercises the bounded look-ahead path.
            auto address = next + 4096 - value.size() - 1; next += 0x10000;
            auto& bytes = memory.regions[address]; bytes.assign(value.begin(), value.end()); bytes.push_back(0);
            return address;
        };
        info.character_name_addr = reinterpret_cast<char*>(text("[SCATTERED]"));
        info.character_creator_addr = reinterpret_cast<char*>(text("legacy importer"));
        info.date_addr = reinterpret_cast<char*>(text("2020-01-01"));
        info.fulldate_addr = reinterpret_cast<char*>(text("2020-01-01 12:00:00"));
        std::map<uint64_t, gameAddr> strings;
        for (auto& move : Records<G::Move>(info.table.move, info.table.moveCount)) {
            for (auto pointer : {&move.name_addr, &move.anim_name_addr}) {
                auto [it, inserted] = strings.emplace(*pointer, 0);
                if (inserted) it->second = text(reinterpret_cast<const char*>(payload + b->nameBlock + *pointer));
                *pointer = it->second;
            }
            animation = next; next += 0x10000;
            auto size = TAnimUtils::FromMemory::Safe::GetAnimSize(payload + b->animationBlock + move.anim_addr,
                b->motaBlock - b->animationBlock - move.anim_addr);
            Check(size != 0, "bad source animation fixture");
            memory.regions[animation] = {payload + b->animationBlock + move.anim_addr, payload + b->animationBlock + move.anim_addr + size};
            move.anim_addr = animation;
#define PTR(field, target, Type) field = field == UINT64_MAX ? 0 : info.table.target + field * sizeof(Type)
            PTR(move.cancel_addr, cancel, Cancel); PTR(move._0x28_cancel_addr, cancel, Cancel);
            PTR(move._0x38_cancel_addr, cancel, Cancel); PTR(move._0x48_cancel_addr, cancel, Cancel);
            PTR(move.hit_condition_addr, hitCondition, HitCondition); PTR(move.voicelip_addr, voiceclip, Voiceclip);
            PTR(move.extra_move_property_addr, extraMoveProperty, ExtraMoveProperty);
            move.move_start_extraprop_addr = move.move_end_extraprop_addr = 0;
        }
        for (auto& item : Records<G::Projectile>(info.table.projectile, info.table.projectileCount)) {
            PTR(item.cancel_addr, cancel, Cancel); PTR(item.hit_condition_addr, hitCondition, HitCondition);
        }
        for (auto address : {info.table.cancel, info.table.groupCancel}) {
            auto count = address == info.table.cancel ? info.table.cancelCount : info.table.groupCancelCount;
            for (auto& item : Records<G::Cancel>(address, count)) {
                PTR(item.requirements_addr, requirement, Requirement); PTR(item.extradata_addr, cancelExtradata, CancelExtradata);
            }
        }
        for (auto& item : Records<G::Reactions>(info.table.reactions, info.table.reactionsCount))
            for (auto& pointer : item.pushbacks) { PTR(pointer, pushback, Pushback); }
        for (auto& item : Records<G::HitCondition>(info.table.hitCondition, info.table.hitConditionCount)) {
            PTR(item.requirements_addr, requirement, Requirement); PTR(item.reactions_addr, reactions, Reactions);
        }
        for (auto& item : Records<G::InputSequence>(info.table.inputSequence, info.table.inputSequenceCount)) { PTR(item.input_addr, input, Input); }
        for (auto& item : Records<G::ThrowCamera>(info.table.throwCameras, info.table.throwCamerasCount)) { PTR(item.cameradata_addr, cameraData, CameraData); }
        for (auto& item : Records<G::Pushback>(info.table.pushback, info.table.pushbackCount)) { PTR(item.extradata_addr, pushbackExtradata, PushbackExtradata); }
#undef PTR
        mota = next; next += 0x10000;
        memory.regions[mota] = {payload + b->motaBlock, payload + b->movelistBlock};
        memset(&info.motas, 0xff, sizeof(info.motas));
        info.motas.motas[0] = info.motas.motas[2] = mota;
        info.motas.motas[1] = next;
        auto& empty = memory.regions[next]; empty.resize(20); memcpy(empty.data(), "MOTA", 4); empty[4] = empty[17] = 1;
        auto head = *reinterpret_cast<const MvlHead*>(payload + b->movelistBlock);
        auto sourceMvl = payload + b->movelistBlock;
        auto rowAddress = Training + 0x10000, playableAddress = Training + 0x30000, inputAddress = Training + 0x80000;
        auto& rows = memory.regions[rowAddress];
        rows = {sourceMvl + head.displayables_offset, sourceMvl + head.displayables_offset + sizeof(MvlDisplayable)};
        auto textAddress = rowAddress - 0x5000;
        memory.regions[textAddress] = {'t', 'e', 'x', 't', 0};
        for (auto& offset : reinterpret_cast<MvlDisplayable*>(rows.data())->all_translation_offsets)
            offset = static_cast<int32_t>(textAddress - rowAddress);
        auto& playables = memory.regions[playableAddress];
        playables = {sourceMvl + head.playables_offset, sourceMvl + head.playables_offset + sizeof(MvlPlayable)};
        reinterpret_cast<MvlPlayable*>(playables.data())->input_sequence_offset = static_cast<uint32_t>(inputAddress - playableAddress);
        memory.regions[inputAddress] = {sourceMvl + head.inputs_offset, sourceMvl + head.inputs_offset + sizeof(MvlInput)};
        head.displayables_offset = 0x10000; head.playables_offset = 0x30000; head.inputs_offset = 0x80000;
        auto& training = memory.regions[Training]; training.resize(32); memcpy(training.data(), &head, 32);
        Sync();
    }
    void Sync() {
        auto& header = memory.regions[Header]; header.resize(0x2e0); memcpy(header.data(), &info, header.size());
    }
    template<class T> std::span<T> Records(gameAddr address, size_t count) {
        return {reinterpret_cast<T*>(memory.regions.at(address).data()), count};
    }
    auto Snapshot(ExtractSettings flags = Flags) { return T7Export::ReadSnapshot(memory, Header, flags, Training); }
};
template<class Change> void Reject(Change change, ExtractSettings flags = Flags) {
    Graph graph; change(graph); graph.Sync(); auto before = graph.memory.regions;
    bool failed = false;
    try { graph.Snapshot(flags); } catch (const std::exception&) { failed = true; }
    Check(failed, "invalid remote moveset was accepted");
    Check(graph.memory.regions == before, "failed export wrote to source process");
}
class Exporter : public ExtractorT7 {
    std::string directory;
public:
    Exporter(GameProcess& process, GameData& game, const GameInfo* info, const fs::path& path)
        : ExtractorT7(process, game, info), directory(path.string()) { cm_extractionDir = directory.c_str(); }
    uint32_t GetCharacterID(gameAddr) override { return 7; }
};
struct Directory {
    fs::path path = fs::temp_directory_path() / ("tkmovesets-export-" + std::to_string(GetCurrentProcessId()));
    Directory() { Check(fs::create_directory(path), "cannot create export test directory"); }
    ~Directory() { std::error_code error; fs::remove_all(path, error); }
};
void VerifyFile(const std::vector<Byte>& bytes) {
    auto header = reinterpret_cast<const TKMovesetHeader*>(bytes.data());
    auto blocks = reinterpret_cast<const TKMovesetHeaderBlocks*>(bytes.data() + header->block_list);
    std::string error;
    Check(T7ImportUtils::ValidateT7Semantics(*blocks, bytes.data() + header->moveset_data_start,
        bytes.size() - header->moveset_data_start, &error), error.c_str());
    GameAddressesFile addresses; GameProcess process; GameData game; game.SetProcess(&process, &addresses);
    GameInfo info{}; info.dataString = info.minorDataString = "t7";
    EditorT7 editor(process, game, &info);
    auto copy = static_cast<Byte*>(malloc(bytes.size())); Check(copy != nullptr, "export editor allocation failed");
    memcpy(copy, bytes.data(), bytes.size());
    Check(editor.LoadMoveset(copy, bytes.size()), "export cannot be loaded in editor");
    Check(editor.CalculateCRC32() == header->crc32, "export CRC does not match editor");
    auto relocated = std::vector<Byte>(bytes.begin() + header->moveset_data_start, bytes.end());
    auto table = reinterpret_cast<G::MovesetTable*>(relocated.data() + blocks->tableBlock);
    Check(T7ImportUtils::ConvertMovesetIndexes(relocated.data(), reinterpret_cast<gameAddr>(relocated.data()), table, blocks),
        "export cannot be relocated for import");
    auto moves = reinterpret_cast<const G::Move*>(relocated.data() + blocks->movesetBlock + table->move);
    Check(moves->cancel_addr == reinterpret_cast<gameAddr>(relocated.data()) + blocks->movesetBlock + table->cancel,
        "relocated move points outside reconstructed cancel list");
}
}
void RunExportGraphTests() {
    Graph graph; auto before = graph.memory.regions;
    auto snapshot = graph.Snapshot();
    Check(graph.memory.regions == before, "successful export wrote to source process");
    Graph other(false);
    Check(other.Snapshot().payload == snapshot.payload, "source allocation order affects serialized moveset");
    Check(snapshot.characterName == "[SCATTERED]", "character name was read beside header");
    auto info = reinterpret_cast<const G::MovesetInfo*>(snapshot.payload.data());
    Check(info->motas.motas[0] == info->motas.motas[2] && info->motas.motas[12] == UINT64_MAX,
        "shared or absent thirteenth MOTA handled incorrectly");
    for (size_t i = 0; i < T7Export::RecordSizes.size(); ++i) {
        auto entry = graph.info.table.entries[i]; if (!entry.listCount) continue;
        Check(std::count(graph.memory.reads.begin(), graph.memory.reads.end(),
            std::pair<gameAddr, size_t>{entry.listAddr, entry.listCount * T7Export::RecordSizes[i]}) == 1,
            "a record list was not copied in one read");
    }
    TKMovesetHeader header; header.gameId = GameId_T7;
    VerifyFile(T7Export::Serialize(snapshot, header));
    {
        Graph shared;
        auto& bytes = shared.memory.regions.at(shared.info.table.move);
        const auto originalMove = bytes;
        bytes.insert(bytes.end(), originalMove.begin(), originalMove.end());
        shared.info.table.moveCount = 2; shared.Sync();
        const auto data = shared.Snapshot();
        auto metadata = reinterpret_cast<const G::MovesetInfo*>(data.payload.data());
        auto moves = reinterpret_cast<const G::Move*>(data.payload.data() + data.blocks.movesetBlock + metadata->table.move);
        Check(moves[0].anim_addr == moves[1].anim_addr && moves[0].name_addr == moves[1].name_addr,
            "shared animations/strings were duplicated");
        Check(std::count(shared.memory.reads.begin(), shared.memory.reads.end(),
            std::pair<gameAddr, size_t>{shared.animation, 36}) == 1, "shared animation payload copied more than once");
    }
    // Normalization preserves data in both byte orders.
    Graph swapped;
    Check(TAnimUtils::FromMemory::ByteswapAnimation(swapped.memory.regions.at(swapped.animation).data(), 36), "animation swap failed");
    auto& mota = swapped.memory.regions.at(swapped.mota);
    Check(TAnimUtils::FromMemory::ByteswapMota(mota.data(), mota.size()), "MOTA swap failed");
    Check(swapped.Snapshot().payload == snapshot.payload, "endian normalization changed animation data");
    {
        Graph empty;
        empty.memory.regions.erase(empty.info.table.input);
        empty.info.table.input = UINT64_MAX; empty.info.table.inputCount = 0;
        auto& sequence = empty.Records<G::InputSequence>(empty.info.table.inputSequence, 1)[0];
        sequence.input_amount = 0; sequence.input_addr = 0xdeadbeef;
        empty.Sync(); empty.Snapshot();
    }
    {
        Graph unordered;
        auto animation = unordered.memory.regions.at(unordered.animation);
        auto& bytes = unordered.memory.regions.at(unordered.mota); bytes.assign(108, 0);
        memcpy(bytes.data(), "MOTA", 4); bytes[4] = bytes[17] = 1;
        uint32_t count = 3, offsets[] = {72, 32, 72};
        memcpy(bytes.data() + 12, &count, 4); memcpy(bytes.data() + 20, offsets, sizeof(offsets));
        memcpy(bytes.data() + 32, animation.data(), 36); memcpy(bytes.data() + 72, animation.data(), 36);
        auto value = unordered.Snapshot();
        Check(value.blocks.movelistBlock - value.blocks.motaBlock >= 128, "unordered MOTA was truncated");
    }
    {
        Graph broken;
        const auto animation = broken.memory.regions.at(broken.animation);
        auto& archive = broken.memory.regions.at(broken.mota);
        archive.assign(104, 0);
        memcpy(archive.data(), "MOTA", 4); archive[4] = archive[17] = 1;
        const uint32_t count = 2, offsets[] = {28, 68};
        memcpy(archive.data() + 12, &count, 4); memcpy(archive.data() + 20, offsets, sizeof(offsets));
        memcpy(archive.data() + 28, animation.data(), 36); memcpy(archive.data() + 68, animation.data(), 36);
        archive[28] = 0; // Undecodable first member; the final member is valid.
        const auto snapshot = broken.Snapshot();
        auto metadata = reinterpret_cast<const G::MovesetInfo*>(snapshot.payload.data());
        Check(metadata->motas.motas[0] == UINT64_MAX && metadata->motas.motas[2] == UINT64_MAX &&
            snapshot.discardedMotas == std::vector<uint8_t>({0, 2}),
            "Invalid MOTA was not discarded consistently for all aliases");
        TKMovesetHeader header; header.gameId = GameId_T7;
        VerifyFile(T7Export::Serialize(snapshot, header));
    }
    Reject([](Graph& g) { g.memory.unavailable = g.info.table.cancel; });
    Reject([](Graph& g) { g.info.table.moveCount = UINT64_MAX; });
    Reject([](Graph& g) { g.info.table.reactions = 0; });
    Reject([](Graph& g) { g.Records<G::Move>(g.info.table.move, 1)[0].cancel_addr += 1; });
    Reject([](Graph& g) { g.Records<G::Move>(g.info.table.move, 1)[0].cancel_addr += 2 * sizeof(Cancel); });
    Reject([](Graph& g) { g.memory.regions.at(reinterpret_cast<gameAddr>(g.info.character_name_addr)).back() = 'x'; });
    Reject([](Graph& g) { g.memory.regions.at(g.animation).pop_back(); });
    Reject([](Graph& g) { g.memory.regions.at(g.mota).resize(23); });
    Reject([](Graph& g) { uint32_t count = UINT32_MAX; memcpy(g.memory.regions.at(g.mota).data() + 12, &count, 4); });
    Reject([](Graph& g) { uint32_t offset = 4; memcpy(g.memory.regions.at(g.mota).data() + 20, &offset, 4); });
    Reject([](Graph& g) { g.memory.changingHeader = Header; });
    Reject([](Graph& g) { g.memory.unavailable = Training + 0x80000; });
    Reject([](Graph& g) { g.memory.regions.at(g.mota)[0] = 'X'; }, Flags | ExtractSettings_UnknownMotas);
    // An opaque, unselected archive has no inferable extent and must not be probed.
    { Graph opaque; opaque.memory.unavailable = opaque.mota; opaque.Snapshot(2); }
    Directory directory;
    Graph output;
    GameAddressesFile addresses; GameData game; game.SetProcess(&output.memory, &addresses);
    GameInfo gameInfo{}; gameInfo.dataString = gameInfo.minorDataString = "t7";
    gameInfo.name = "Tekken 7"; gameInfo.movesetNamePrefix = "T7_"; gameInfo.gameId = GameId_T7;
    Exporter exporter(output.memory, game, &gameInfo, directory.path);
    auto pointerOffset = game.GetValue("motbin_offset");
    auto& player = output.memory.regions[Player]; player.resize(pointerOffset + sizeof(gameAddr));
    memcpy(player.data() + pointerOffset, &Header, sizeof(Header));
    Check(exporter.GetPlayerCharacterName(Player) == "Scattered", "export UI name ignores metadata pointer");
    const auto target = directory.path / "T7_Scattered.tkmvst";
    uint8_t progress = 0;
    for (auto compression : {ExtractSettings(0), ExtractSettings(ExtractSettings_CompressLZ4), ExtractSettings(ExtractSettings_CompressLZMA)}) {
        auto result = exporter.Extract(Player, 7 | ExtractSettings_OVERWRITE_SAME_FILENAME | compression, progress);
        Check(result == ExtractionErrcode_Successful && progress == 100, "scattered exporter failed");
        MovesetFile saved(target); saved.Validate(); VerifyFile({saved.Bytes().begin(), saved.Bytes().end()});
    }
    auto saved = ReadFile(target);
    output.memory.unavailable = output.info.table.requirement;
    Check(exporter.Extract(Player, 7 | ExtractSettings_OVERWRITE_SAME_FILENAME, progress) == ExtractionErrcode_MovesetDataError &&
        progress < 100 && ReadFile(target) == saved, "failed source read replaced existing export");
    output.memory.unavailable = 0;
    output.memory.changingPointer = Player + pointerOffset;
    Check(exporter.Extract(Player, 7 | ExtractSettings_OVERWRITE_SAME_FILENAME, progress) == ExtractionErrcode_MovesetDataError &&
        ReadFile(target) == saved, "player pointer race replaced existing export");
    output.memory.changingPointer = 0;
    auto lock = CreateFileW(target.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(lock != INVALID_HANDLE_VALUE, "cannot lock export target");
    auto result = exporter.Extract(Player, 7 | ExtractSettings_OVERWRITE_SAME_FILENAME, progress);
    CloseHandle(lock);
    Check(result == ExtractionErrcode_FileCreationErr && ReadFile(target) == saved, "locked export was replaced");
    Check(std::distance(fs::directory_iterator(directory.path), fs::directory_iterator{}) == 1, "export left temporary artifacts");
    std::cout << "Pointer-driven T7 export regressions passed\n";
}
// Optional local-only evidence: an offline memory image made by the actual Python importer.
void CheckLegacyExportImage(const fs::path& manifest) {
    std::ifstream input(manifest); nlohmann::json image; input >> image;
    Memory memory;
    for (const auto& region : image.at("regions")) {
        auto bytes = ReadFile(manifest.parent_path() / region.at("file").get<std::string>());
        memory.regions.emplace(region.at("address").get<gameAddr>(), std::move(bytes));
    }
    auto snapshot = T7Export::ReadSnapshot(memory, image.at("moveset").get<gameAddr>(), image.value("mota_mask", uint64_t(0xfff)));
    TKMovesetHeader header; header.gameId = GameId_T7;
    VerifyFile(T7Export::Serialize(snapshot, header));
    std::cout << image.at("character").get<std::string>() << ": legacy Python import re-export passed ("
        << snapshot.payload.size() << " bytes, " << memory.reads.size() << " reads)\n";
}
