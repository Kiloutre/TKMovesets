#include "Animations.hpp"
#include "Structs_t7.h"
#include "MovesetStructs.h"
#include <set>
#include <iostream>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace {
void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
template<class T> void Put(std::vector<Byte>& bytes, size_t offset, T value) { memcpy(bytes.data() + offset, &value, sizeof(value)); }
std::vector<Byte> ShortAnimation()
{
    std::vector<Byte> bytes(28, 0);
    Put<uint16_t>(bytes, 0, 0x64);
    Put<uint16_t>(bytes, 2, 1);
    Put<uint16_t>(bytes, 4, 4);
    Put<uint16_t>(bytes, 6, 2);
    Put<int16_t>(bytes, 12, 1000);
    Put<int16_t>(bytes, 14, -2000);
    Put<uint32_t>(bytes, 18, 4);
    bytes[22] = bytes[24] = bytes[26] = 8;
    return bytes;
}
std::vector<Byte> FloatAnimation()
{
    std::vector<Byte> bytes(44, 0);
    Put<uint16_t>(bytes, 0, 0x64);
    Put<uint16_t>(bytes, 2, 1);
    Put<uint16_t>(bytes, 4, 0xb);
    Put<uint16_t>(bytes, 6, 2);
    Put<uint16_t>(bytes, 10, 1);
    Put<float>(bytes, 12, 0.01f);
    Put<float>(bytes, 16, 1.0f);
    Put<float>(bytes, 20, 2.0f);
    Put<float>(bytes, 24, 3.0f);
    Put<uint32_t>(bytes, 28, 4);
    for (int i = 0; i < 3; ++i) { bytes[32 + i * 4] = 19; bytes[33 + i * 4] = 0x40; }
    return bytes;
}
std::vector<Byte> C8Animation()
{
    std::vector<Byte> bytes(36, 0);
    Put<uint16_t>(bytes, 0, 0xc8);
    Put<uint16_t>(bytes, 2, 1);
    Put<uint32_t>(bytes, 4, 2);
    return bytes;
}
void Truncations(const std::vector<Byte>& valid)
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    auto page = static_cast<Byte*>(VirtualAlloc(nullptr, info.dwPageSize * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Check(page != nullptr, "guard-page allocation failed");
    DWORD old;
    Check(VirtualProtect(page + info.dwPageSize, info.dwPageSize, PAGE_NOACCESS, &old), "guard-page protection failed");
    for (size_t size = 0; size < valid.size(); ++size) {
        auto ptr = page + info.dwPageSize - size;
        memcpy(ptr, valid.data(), size);
        Check(TAnimUtils::FromMemory::Safe::GetAnimSize(ptr, size) == 0, "truncated animation accepted");
        Check(TAnimUtils::FromMemory::Safe::GetAnimDuration(ptr, size) == 0, "truncated duration accepted");
        std::array<float,3> out{9,9,9};
        Check(!TAnimUtils::FromMemory::ParseAnimation0x64({ptr, size}, 2, out), "truncated decode accepted");
        Check(out == std::array<float,3>{9,9,9}, "failed decode changed its output");
    }
    VirtualFree(page, 0, MEM_RELEASE);
}
}

void RunAnimationTests()
{
    namespace memory = TAnimUtils::FromMemory;
    auto shortAnim = ShortAnimation();
    auto floating = FloatAnimation();
    auto c8 = C8Animation();
    for (auto bytes : {shortAnim, floating, c8}) {
        Check(memory::Safe::GetAnimSize(bytes.data(), bytes.size()) == bytes.size(), "valid animation size rejected");
        Check(memory::Safe::GetAnimDuration(bytes.data(), bytes.size()) == 2, "valid duration rejected");
        Truncations(bytes);
        auto swapped = bytes;
        Check(memory::ByteswapAnimation(swapped.data(), swapped.size()), "bounded endian conversion failed");
        Check(memory::Safe::GetAnimSize(swapped.data(), swapped.size()) == bytes.size(), "big-endian animation rejected");
        Check(memory::ByteswapAnimation(swapped.data(), swapped.size()) && swapped == bytes, "endian round trip changed animation");
    }
    std::array<float,3> out{};
    Check(memory::ParseAnimation0x64(shortAnim, 1, out) == 1, "base pose rejected");
    Check(std::abs(out[0] - 1000 * 0.000095873802f) < 0.000001f, "short base pose changed");
    auto base = out;
    Check(memory::ParseAnimation0x64(shortAnim, 2, out) == 1 && out == base, "short delta pose changed");
    Check(memory::ParseAnimation0x64(floating, 2, out) == 1 && out == std::array<float,3>{1,2,3}, "float delta pose changed");
    auto swapped = floating;
    memory::ByteswapAnimation(swapped.data(), swapped.size());
    Check(memory::ParseAnimation0x64(swapped, 2, out) == 1 && out == std::array<float,3>{1,2,3}, "big-endian frame decode failed");
    Check(!memory::ParseAnimation0x64(floating, 0, out), "frame zero accepted");
    Check(!memory::ParseAnimation0x64(floating, 2, std::span(out).first(2)), "short output buffer accepted");

    for (int test = 0; test < 5; ++test) {
        auto bad = shortAnim;
        if (test == 0) Put<uint16_t>(bad, 2, 0);
        if (test == 1) Put<uint16_t>(bad, 2, 256);
        if (test == 2) Put<uint16_t>(bad, 10, 256);
        if (test == 3) Put<uint32_t>(bad, 18, UINT32_MAX);
        if (test == 4) bad[22] = 0;
        Check(!memory::Safe::GetAnimSize(bad.data(), bad.size()), "invalid animation header accepted");
        auto before = bad;
        Check(!memory::ByteswapAnimation(bad.data(), bad.size()) && bad == before, "failed byte swap modified data");
    }
    auto shortChannel = shortAnim;
    shortChannel[22] = 11; // Declares two bytes, but requests more bits than it contains.
    shortChannel[23] = 0x40;
    out = {9,9,9};
    Check(!memory::ParseAnimation0x64(shortChannel, 2, out) && out == std::array<float,3>{9,9,9}, "bitstream overrun was accepted");

    auto deltas = shortAnim;
    deltas.resize(31);
    Put<uint16_t>(deltas, 6, 3);
    for (int axis = 0; axis < 3; ++axis) {
        deltas[22 + axis * 3] = 12;
        deltas[23 + axis * 3] = 0x41;
        deltas[24 + axis * 3] = 0;
    }
    Check(memory::ParseAnimation0x64(deltas, 2, out) == 1, "first delta frame failed");
    Check(std::abs(out[0] - 1001 * 0.000095873802f) < 0.000001f, "first delta frame applied twice");
    Check(memory::ParseAnimation0x64(deltas, 3, out) == 1, "second delta frame failed");
    Check(std::abs(out[0] - 1002 * 0.000095873802f) < 0.000001f, "second delta frame was skipped");

    std::vector<Byte> mota(24 + floating.size(), 0);
    memcpy(mota.data(), "MOTA", 4);
    mota[4] = 1;
    Put<uint32_t>(mota, 12, 1);
    Put<uint32_t>(mota, 20, 24);
    memcpy(mota.data() + 24, floating.data(), floating.size());
    auto originalMota = mota;
    Check(memory::ByteswapMota(mota.data(), mota.size()), "bounded MOTA swap failed");
    Check(memory::ByteswapMota(mota.data(), mota.size()) && mota == originalMota, "MOTA endian round trip failed");
    Put<uint32_t>(mota, 20, UINT32_MAX);
    auto corruptMota = mota;
    Check(!memory::ByteswapMota(mota.data(), mota.size()) && mota == corruptMota, "bad MOTA was partially swapped");

    auto path = std::filesystem::temp_directory_path() / ("tkmovesets-animation-" + std::to_string(GetCurrentProcessId()) + ".bin");
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(floating.data()), floating.size());
    }
    s_animInfo info{};
    Check(TAnimUtils::FromFile::GetAnimationInfos(path.c_str(), info) && info.duration == 2, "bounded file metadata failed");
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << "bad";
    }
    Check(!TAnimUtils::FromFile::GetAnimationInfos(path.c_str(), info) && info.size == 0, "truncated file metadata accepted");
    std::filesystem::remove(path);

    GameProcess process;
    Check(process.Attach("MovesetRegressionTests.exe", 0), "cannot attach remote animation test");
    Check(TAnimUtils::FromProcess::get64AnimSize(process, reinterpret_cast<gameAddr>(shortAnim.data())) == shortAnim.size(), "checked remote size failed");
    Check(!TAnimUtils::FromProcess::get64AnimSize(process, 1), "failed remote read accepted");
    process.Detach();
}

void RunHistoricalAnimationTests(const std::vector<Byte>& file)
{
    auto header = reinterpret_cast<const TKMovesetHeader*>(file.data());
    auto blocks = reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(file.data() + header->block_list);
    const auto payload = file.data() + header->moveset_data_start;
    auto info = reinterpret_cast<const StructsT7_gameAddr::MovesetInfo*>(payload);
    auto moves = reinterpret_cast<const StructsT7_gameAddr::Move*>(payload + blocks->movesetBlock + info->table.move);
    std::set<uint64_t> offsets;
    for (size_t i = 0; i < info->table.moveCount; ++i) offsets.insert(moves[i].anim_addr);
    const uint64_t animationSize = blocks->motaBlock - blocks->animationBlock;
    size_t invalid = 0, decodeFailures = 0;
    std::array<float, 255 * 3> output{};
    for (auto it = offsets.begin(); it != offsets.end(); ++it) {
        auto next = std::next(it);
        auto end = next == offsets.end() ? animationSize : *next;
        auto bytes = std::span(payload + blocks->animationBlock + *it, static_cast<size_t>(end - *it));
        auto size = TAnimUtils::FromMemory::Safe::GetAnimSize(bytes.data(), bytes.size());
        if (!size) {
            if (invalid < 5) std::cerr << "invalid historical animation at " << *it << " of " << bytes.size() << " bytes\n";
            ++invalid;
            continue;
        }
        if (bytes[0] == 0x64 || bytes[1] == 0x64) {
            if (!TAnimUtils::FromMemory::ParseAnimation0x64(bytes, 1, output) ||
                !TAnimUtils::FromMemory::ParseAnimation0x64(bytes, 2, output) ||
                !TAnimUtils::FromMemory::ParseAnimation0x64(bytes,
                    TAnimUtils::FromMemory::Safe::GetAnimDuration(bytes.data(), bytes.size()), output)) {
                if (decodeFailures < 5) std::cerr << "historical frame decode failed at " << *it << '\n';
                ++decodeFailures;
            }
        }
    }
    std::cout << "Historical animations " << offsets.size() << ", invalid " << invalid
        << ", frame failures " << decodeFailures << '\n';
    Check(!invalid && !decodeFailures, "historical bounded animation checks failed");
}
