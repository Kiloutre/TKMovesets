#include "Importer_t7/Importer_t7_Utils.hpp"
#include "Animations.hpp"
#include "Editor_t7/Editor_t7.hpp"
#include "MovesetLoader_t7.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace StructsT7;
namespace G = StructsT7_gameAddr;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Fixture {
    TKMovesetHeaderBlocks blocks{};
    std::vector<Byte> payload;
    G::MovesetInfo& Info() { return *reinterpret_cast<G::MovesetInfo*>(payload.data()); }
    template<class T> T* Records(uint64_t offset) { return reinterpret_cast<T*>(payload.data() + blocks.movesetBlock + offset); }
    G::Move& Move() { return *Records<G::Move>(Info().table.move); }
    G::Cancel* Cancels() { return Records<G::Cancel>(Info().table.cancel); }
    MvlHead& Training() { return *reinterpret_cast<MvlHead*>(payload.data() + blocks.movelistBlock); }
    std::vector<Byte> File() {
        TKMovesetHeader header;
        header.gameId = 0; header.minorVersion = 0;
        header.header_size = header.block_list = sizeof(header);
        header.block_list_size = 8;
        header.moveset_data_start = sizeof(header) + sizeof(blocks);
        std::vector<Byte> file(header.moveset_data_start + payload.size());
        memcpy(file.data(), &header, sizeof(header));
        memcpy(file.data() + header.block_list, &blocks, sizeof(blocks));
        memcpy(file.data() + header.moveset_data_start, payload.data(), payload.size());
        return file;
    }
};
// animationBones = 23: a T7 body animation (rest pose) that the T7 -> retail T8 converter accepts.
// cameraMota: a camera MOTA (slot 9) holding one 2-frame 0xC8 camera clip (position, Euler rotation, zoom, roll).
Fixture Valid(bool sharedMota = false, size_t propertyRows = 1, uint16_t animationBones = 1, bool cameraMota = false) {
    Fixture fixture;
    auto& b = fixture.blocks;
    b.tableBlock = offsetof(MovesetInfo, table);
    b.motalistsBlock = offsetof(MovesetInfo, motas);
    b.nameBlock = sizeof(MovesetInfo); b.movesetBlock = b.nameBlock + 8;
    G::MovesetTable table{};
    const size_t strides[] = {sizeof(Reactions), sizeof(Requirement), sizeof(HitCondition), sizeof(Projectile),
        sizeof(Pushback), sizeof(PushbackExtradata), sizeof(Cancel), sizeof(Cancel), sizeof(CancelExtradata),
        sizeof(ExtraMoveProperty), sizeof(OtherMoveProperty), sizeof(OtherMoveProperty), sizeof(Move),
        sizeof(Voiceclip), sizeof(InputSequence), sizeof(Input), sizeof(UnknownParryRelated), sizeof(CameraData), sizeof(ThrowCamera)};
    size_t size = 0;
    for (size_t i = 0; i < std::size(strides); ++i) {
        size = (size + 7) & ~size_t(7);
        table.entries[i].listAddr = size;
        table.entries[i].listCount = i == 1 || i == 2 || i == 6 ? 2 : i == 9 ? propertyRows : 1;
        size += strides[i] * table.entries[i].listCount;
    }
    b.animationBlock = b.movesetBlock + size;
    const uint32_t animationFrames = 2;
    b.motaBlock = b.animationBlock + (std::max)(size_t(40), (8 + animationBones * 4 + size_t(animationFrames) * animationBones * 12 + 7) & ~size_t(7)); // C8 extent
    const size_t animationOffset = sharedMota ? 28 : 24;
    const size_t cameraAt = (animationOffset + 36 + 7) & ~size_t(7), cameraClip = 8 + 4 * 4 + 2 * 4 * 12;
    b.movelistBlock = b.motaBlock + (cameraMota ? cameraAt + 24 + cameraClip : animationOffset + 36);
    const size_t displayables = 40, playables = displayables + sizeof(MvlDisplayable), inputs = playables + sizeof(MvlPlayable);
    fixture.payload.resize(b.movelistBlock + inputs + sizeof(MvlInput));
    auto& info = fixture.Info();
    info.table = table;
    memset(&info.motas, 0xff, sizeof(info.motas));
    info.motas.motas[0] = 0;
    info.character_name_addr = reinterpret_cast<char*>(b.nameBlock);
    memcpy(fixture.payload.data() + b.nameBlock, "m\0a\0", 4);
    fixture.Records<Requirement>(table.requirement)[1].condition = 881;
    fixture.Cancels()[1].command = 0x8000;
    fixture.Records<G::Cancel>(table.groupCancel)->command = 0x800c;
    fixture.Records<Voiceclip>(table.voiceclip)->id = UINT32_MAX;
    fixture.Records<G::OtherMoveProperty>(table.moveBeginningProp)->extraprop = 881;
    fixture.Records<G::OtherMoveProperty>(table.moveEndingProp)->extraprop = 881;
    fixture.Records<G::HitCondition>(table.hitCondition)[1].requirements_addr = 1;
    fixture.Records<G::InputSequence>(table.inputSequence)->input_amount = 1;
    // Extra properties: one list of `propertyRows` rows, the last one its id-0 terminator. The other rows carry
    // property 0x8020 (no T7 handler, left unmapped by the T7 -> T8 converter) so they are walked, not ends.
    for (size_t row = 0; row + 1 < propertyRows; ++row) {
        auto& property = fixture.Records<ExtraMoveProperty>(table.extraMoveProperty)[row];
        property.starting_frame = 1; property.id = 0x8020;
    }
    auto& move = fixture.Move();
    move.name_addr = 0; move.anim_name_addr = 2;
    auto animation = fixture.payload.data() + b.animationBlock;
    const uint16_t type = 0xc8, bones = animationBones; const uint32_t frames = animationFrames;
    memcpy(animation, &type, 2); memcpy(animation + 2, &bones, 2); memcpy(animation + 4, &frames, 4);
    auto mota = fixture.payload.data() + b.motaBlock;
    memcpy(mota, "MOTA", 4); mota[4] = 1;
    const uint32_t count = sharedMota ? 2 : 1, offset = static_cast<uint32_t>(animationOffset);
    memcpy(mota + 12, &count, 4); memcpy(mota + 20, &offset, 4);
    if (sharedMota) memcpy(mota + 24, &offset, 4);
    memcpy(mota + offset, animation, 36);
    const uint16_t motaBones = 1; memcpy(mota + offset + 2, &motaBones, 2); // the MOTA keeps a 1-bone, 36-byte C8 animation
    if (cameraMota) {
        info.motas.motas[9] = static_cast<gameAddr>(cameraAt);
        auto camera = mota + cameraAt;
        memcpy(camera, "MOTA", 4); camera[4] = 1; camera[0x11] = 1; // little endian
        const uint32_t clips = 1, clipOffset = 24; memcpy(camera + 12, &clips, 4); memcpy(camera + 20, &clipOffset, 4);
        auto clip = camera + clipOffset;
        const uint16_t c8 = 0xc8, channels = 4; const uint32_t clipFrames = 2, descriptors[4] = {0x0B, 0x07, 0x0F, 0x15};
        memcpy(clip, &c8, 2); memcpy(clip + 2, &channels, 2); memcpy(clip + 4, &clipFrames, 4); memcpy(clip + 8, descriptors, 16);
        const float values[2][12] = {{0, 1500, 3000, 0, 0, 0, 1, 0, 0, 0, 0, 0}, {100, 1500, 3000, 0, 0, 0, 1, 0, 0, 0, 0, 0}};
        memcpy(clip + 24, values, sizeof(values));
    }
    auto& training = fixture.Training();
    memcpy(training.mvlString, "MVLT", 4);
    training.displayables_count = training.playables_count = 1;
    training.displayables_offset = displayables; training.playables_offset = playables; training.inputs_offset = inputs;
    auto base = reinterpret_cast<Byte*>(&training);
    memcpy(base + 32, "text", 5);
    auto row = reinterpret_cast<MvlDisplayable*>(base + displayables);
    for (auto& text : row->all_translation_offsets) text = 32;
    reinterpret_cast<MvlPlayable*>(base + playables)->input_count = 1;
    return fixture;
}
bool Validate(Fixture& fixture, std::string* error = nullptr) {
    return T7ImportUtils::ValidateT7Semantics(fixture.blocks, fixture.payload.data(), fixture.payload.size(), error);
}
template<class Change> void Malformed(Change change, const char* expected, bool sharedMota = false) {
    auto fixture = Valid(sharedMota); change(fixture);
    auto before = fixture.payload;
    std::string error;
    Check(!Validate(fixture, &error), "malformed moveset accepted");
    Check(error.find(expected) != std::string::npos, error.c_str());
    Check(fixture.payload == before, "validation changed source bytes");
}
// Not broken, only unusual: animation contents this tool cannot read, overlapping animations, large files.
template<class Change> void Accepted(Change change, const char* what, bool sharedMota = false) {
    auto fixture = Valid(sharedMota); change(fixture);
    std::string error;
    Check(Validate(fixture, &error), (std::string(what) + " was refused: " + error).c_str());
}
}

void RunSemanticValidationTests() {
    auto valid = Valid();
    std::string error;
    Check(Validate(valid, &error), error.c_str());
    auto shared = Valid(true);
    shared.Info().motas.motas[1] = 0;
    Check(Validate(shared, &error), error.c_str());
    Check(TAnimUtils::FromMemory::ByteswapMota(shared.payload.data() + shared.blocks.motaBlock,
        shared.blocks.movelistBlock - shared.blocks.motaBlock), "semantic MOTA fixture byte swap failed");
    Check(Validate(shared, &error), "big-endian MOTA was rejected");
    auto before = valid.payload;
    Check(Validate(valid) && valid.payload == before, "successful validation changed bytes");
    Malformed([](auto& f) { f.Info().current_aliases[0] = 1; }, "alias");
    Malformed([](auto& f) { f.Info().orig_aliases[0] = UINT16_MAX; }, "alias");
    Malformed([](auto& f) { f.Info().character_name_addr = reinterpret_cast<char*>(1); }, "metadata");
    Malformed([](auto& f) { f.Move().transition = 0x8038; }, "transition");
    Malformed([](auto& f) { f.Move().cancel_addr = f.Info().table.cancelCount; }, "reference");
    Malformed([](auto& f) { f.Cancels()[1].command = 0; }, "cancel list");
    Malformed([](auto& f) { f.template Records<Requirement>(f.Info().table.requirement)[1].condition = 0; }, "Unterminated");
    Malformed([](auto& f) { f.Cancels()[0].command = 0xabcd0000800dULL; f.Info().table.inputSequenceCount = 0; }, "sequence");
    Malformed([](auto& f) { f.Cancels()[0].command = 0x800b; f.Cancels()[0].move_id = 1; }, "cancel target");
    Malformed([](auto& f) { f.Cancels()[0].move_id = 500; }, "cancel target");
    // Property id 0 ends an extra-property list, whatever the frame: a frame-0 row with an id is not an end...
    Malformed([](auto& f) { f.template Records<ExtraMoveProperty>(f.Info().table.extraMoveProperty)->id = 0x8123; }, "property");
    // ...and an id-0 row with a nonzero frame is one.
    {
        auto framed = Valid();
        framed.template Records<ExtraMoveProperty>(framed.Info().table.extraMoveProperty)->starting_frame = 5;
        Check(Validate(framed, &error), ("id-0 terminator with a frame rejected: " + error).c_str());
    }
    Malformed([](auto& f) { f.template Records<Voiceclip>(f.Info().table.voiceclip)->id = 3; }, "voice");
    Malformed([](auto& f) { f.template Records<G::OtherMoveProperty>(f.Info().table.moveBeginningProp)->extraprop = 0; }, "property");
    Malformed([](auto& f) { f.template Records<G::HitCondition>(f.Info().table.hitCondition)[1].requirements_addr = 0; }, "hit-condition list");
    Malformed([](auto& f) { f.template Records<G::HitCondition>(f.Info().table.hitCondition)[0].requirements_addr = UINT64_MAX; }, "hit-condition list");
    Malformed([](auto& f) { f.template Records<G::Reactions>(f.Info().table.reactions)->standing_moveid = 500; }, "reaction");
    Malformed([](auto& f) { f.template Records<G::InputSequence>(f.Info().table.inputSequence)->input_amount = 2; }, "Input sequence");
    Malformed([](auto& f) { f.template Records<G::InputSequence>(f.Info().table.inputSequence)->input_addr = UINT64_MAX; }, "Input sequence");
    Accepted([](auto& f) { f.payload[f.blocks.animationBlock] = 0; }, "unreadable move animation");
    Accepted([](auto& f) { uint32_t frames = UINT32_MAX; memcpy(f.payload.data() + f.blocks.animationBlock + 4, &frames, 4); }, "unreadable move animation");
    Malformed([](auto& f) { f.payload[f.blocks.motaBlock] = 0; }, "MOTA header");
    Malformed([](auto& f) { uint32_t count = UINT32_MAX; memcpy(f.payload.data() + f.blocks.motaBlock + 12, &count, 4); }, "MOTA offset");
    Malformed([](auto& f) { uint32_t offset = 20; memcpy(f.payload.data() + f.blocks.motaBlock + 20, &offset, 4); }, "MOTA animation offset");
    Accepted([](auto& f) { f.payload[f.blocks.motaBlock + 24] = 0; }, "unreadable MOTA animation");
    Accepted([](auto& f) { uint32_t offset = 29; memcpy(f.payload.data() + f.blocks.motaBlock + 24, &offset, 4); }, "overlapping MOTA animation", true);
    Malformed([](auto& f) { auto base = reinterpret_cast<Byte*>(&f.Training()); reinterpret_cast<MvlDisplayable*>(base + f.Training().displayables_offset)->playable_id = 1; }, "playable");
    Malformed([](auto& f) {
        auto& head = f.Training(); auto base = reinterpret_cast<Byte*>(&head);
        auto row = reinterpret_cast<MvlDisplayable*>(base + head.displayables_offset);
        for (auto& offset : row->all_translation_offsets) offset = 0;
        head.playables_offset = head.displayables_offset;
    }, "Overlapping movelist tables");
    Malformed([](auto& f) {
        auto& head = f.Training(); auto base = reinterpret_cast<Byte*>(&head);
        reinterpret_cast<MvlDisplayable*>(base + head.displayables_offset)->all_translation_offsets[0] = head.displayables_offset;
    }, "string overlaps records");
    Malformed([](auto& f) {
        auto& head = f.Training(); auto base = reinterpret_cast<Byte*>(&head);
        reinterpret_cast<MvlDisplayable*>(base + head.displayables_offset)->all_translation_offsets[0] = 4;
    }, "string overlaps header");
    valid.Records<G::Reactions>(valid.Info().table.reactions)->vertical_pushback = UINT16_MAX;
    Check(Validate(valid, &error), "vertical pushback was incorrectly treated as a move ID");
    // The real editor and online installer must reject invalid data before installing it.
    auto bad = Valid(); bad.Info().current_aliases[0] = 10;
    auto file = bad.File();
    SharedMemT7_Player player{};
    auto packet = new Byte[file.size()]; memcpy(packet, file.data(), file.size());
    Check(!MovesetLoaderT7::ImportForOnline(player, packet, file.size()), "online installer accepted invalid semantics");
    GameAddressesFile addresses;
    GameProcess process; GameData game; game.SetProcess(&process, &addresses);
    GameInfo info{}; info.dataString = info.minorDataString = "t7";
    EditorT7 editor(process, game, &info);
    auto goodFile = Valid().File();
    auto owned = static_cast<Byte*>(malloc(goodFile.size()));
    Check(owned != nullptr, "editor fixture allocation failed");
    memcpy(owned, goodFile.data(), goodFile.size());
    Check(editor.LoadMoveset(owned, goodFile.size()), "editor rejected valid semantic fixture");
    editor.live_loadedMoveset = 0x1234;
    uint64_t originalSize;
    const auto original = editor.GetMoveset(originalSize);
    const std::vector<Byte> originalBytes(original, original + originalSize);
    auto invalid = static_cast<Byte*>(malloc(file.size()));
    Check(invalid != nullptr, "invalid editor fixture allocation failed");
    memcpy(invalid, file.data(), file.size());
    Check(!editor.LoadMoveset(invalid, file.size()), "editor installed malformed moveset");
    uint64_t preservedSize;
    Check(editor.GetMoveset(preservedSize) == original && preservedSize == originalSize &&
        !memcmp(original, originalBytes.data(), originalSize) && editor.live_loadedMoveset == 0x1234,
        "rejected editor load changed the installed moveset");
    // Every truncated prefix abuts an inaccessible guard page.
    SYSTEM_INFO system; GetSystemInfo(&system);
    const auto pages = (before.size() + system.dwPageSize - 1) / system.dwPageSize;
    auto allocation = static_cast<Byte*>(VirtualAlloc(nullptr, (pages + 1) * system.dwPageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Check(allocation != nullptr, "semantic guard allocation failed");
    DWORD protection;
    auto guard = allocation + pages * system.dwPageSize;
    Check(VirtualProtect(guard, system.dwPageSize, PAGE_NOACCESS, &protection), "semantic guard protection failed");
    for (size_t size = 0; size < before.size(); ++size) {
        auto data = guard - size; memcpy(data, before.data(), size);
        const auto accepted = T7ImportUtils::ValidateT7Semantics(valid.blocks, data, size);
        // A complete file may omit the optional training-movelist block entirely.
        Check(accepted == (size == valid.blocks.movelistBlock), "truncated moveset acceptance mismatch");
    }
    VirtualFree(allocation, 0, MEM_RELEASE);
}

std::vector<Byte> SemanticFixture() { return Valid().File(); }
std::vector<Byte> SemanticBodyFixture() { return Valid(false, 1, 23).File(); }
// A 23-bone body move whose extra properties play the camera MOTA's clip 0 on either side (T7 33845, character MOTA bit
// 0x4000) next to a drama camera row (33443), then a list end.
std::vector<Byte> SemanticCameraFixture() {
    auto fixture = Valid(false, 4, 23, true);
    auto rows = fixture.Records<ExtraMoveProperty>(fixture.Info().table.extraMoveProperty);
    rows[0].starting_frame = 0x8001; rows[0].id = 33845; rows[0].value_unsigned = 0x4000;
    rows[1].starting_frame = 0x8001; rows[1].id = 33443; rows[1].value_unsigned = 1;
    rows[2].starting_frame = 0x8001; rows[2].id = 0x8020; rows[2].value_unsigned = 0;
    std::string error;
    Check(Validate(fixture, &error), ("Camera fixture is invalid: " + error).c_str());
    return fixture.File();
}
std::vector<Byte> SemanticFixtureWithPropertyList(size_t rows) {
    auto fixture = Valid(false, rows);
    std::string error;
    Check(Validate(fixture, &error), ("Long property list fixture is invalid: " + error).c_str());
    return fixture.File();
}
