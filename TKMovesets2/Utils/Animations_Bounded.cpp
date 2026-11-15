#include "Animations.hpp"
#include "AnimationReader.hpp"
#include <algorithm>
#include <limits>
#include <set>

TAnimUtils::AnimationLayout TAnimUtils::ReadAnimationLayout(std::span<const Byte> bytes)
{
    AnimationReader input(bytes);
    return ReadAnimationLayoutFrom(input);
}

namespace TAnimUtils::FromMemory::Safe {
    uint64_t GetAnimSize(const Byte* animation, uint64_t size)
    {
        if (!animation || size > SIZE_MAX) return 0;
        try { return ReadAnimationLayout({animation, static_cast<size_t>(size)}).size; }
        catch (const std::exception&) { return 0; }
    }
    uint64_t getC8AnimSize(const Byte* animation, uint64_t size)
    {
        if (!animation) return 0;
        try {
            auto layout = ReadAnimationLayout({animation, static_cast<size_t>(size)});
            return layout.type == 0xc8 ? layout.size : 0;
        } catch (const std::exception&) { return 0; }
    }
    uint64_t get64AnimSize(const Byte* animation, uint64_t size)
    {
        if (!animation) return 0;
        try {
            auto layout = ReadAnimationLayout({animation, static_cast<size_t>(size)});
            return layout.type == 0x64 ? layout.size : 0;
        } catch (const std::exception&) { return 0; }
    }
    uint32_t GetAnimDuration(const Byte* animation, uint64_t size)
    {
        if (!animation) return 0;
        try { return ReadAnimationLayout({animation, static_cast<size_t>(size)}).frames; }
        catch (const std::exception&) { return 0; }
    }
}

bool TAnimUtils::FromMemory::ByteswapAnimation(Byte* animation, uint64_t size)
{
    if (!animation) return false;
    try {
        auto layout = ReadAnimationLayout({animation, static_cast<size_t>(size)});
        AnimationReader input({animation, static_cast<size_t>(size)});
        auto swap = [&](size_t offset, size_t width) {
            std::reverse(animation + offset, animation + offset + width);
        };
        if (layout.type == 0xc8) {
            for (size_t offset = 4; offset < layout.size; offset += 4) swap(offset, 4);
        } else {
            size_t pose = layout.basePose;
            for (size_t i = 0; i < layout.bones; ++i) {
                auto descriptor = input.Read<uint16_t>(4 + i * 2, layout.swapped);
                const size_t width = descriptor >= 4 && descriptor <= 7 ? 2 : 4;
                for (int axis = 0; axis < 3; ++axis) { swap(pose, width); pose += width; }
            }
            for (size_t i = 0; i < layout.floatBones; ++i) swap(layout.postDescriptors + 6 + i * 4, 4);
            for (size_t i = 0; i < (layout.frames + 14) / 16; ++i) swap(layout.keyframes + i * 4, 4);
            for (size_t i = 0; i < layout.bones; ++i) swap(4 + i * 2, 2);
            swap(layout.postDescriptors, 2);
            swap(layout.postDescriptors + 4, 2);
        }
        swap(0, 2);
        swap(2, 2);
        return true;
    } catch (const std::exception&) { return false; }
}

bool TAnimUtils::FromMemory::ByteswapMota(Byte* mota, uint64_t size)
{
    try {
        if (!mota || size < 20 || size > (256ULL << 20) || memcmp(mota, "MOTA", 4)) return false;
        std::vector<Byte> copy(mota, mota + size);
        AnimationReader input(copy);
        bool swapped = copy[5] == 1;
        auto count = input.Read<uint32_t>(12, swapped);
        if (count > (size - 20) / 4) return false;
        std::set<uint32_t> animations;
        for (uint32_t i = 0; i < count; ++i) {
            auto offset = input.Read<uint32_t>(20 + i * 4, swapped);
            if (offset < 20ULL + count * 4ULL || offset >= size) return false;
            auto layout = ReadAnimationLayout(std::span<const Byte>(copy).subspan(offset));
            if (animations.insert(offset).second && layout.swapped == swapped)
                if (!ByteswapAnimation(copy.data() + offset, size - offset)) return false;
            std::reverse(copy.begin() + 20 + i * 4, copy.begin() + 24 + i * 4);
        }
        std::reverse(copy.begin() + 4, copy.begin() + 6);
        std::reverse(copy.begin() + 6, copy.begin() + 8);
        std::reverse(copy.begin() + 8, copy.begin() + 12);
        std::reverse(copy.begin() + 12, copy.begin() + 16);
        copy[17] = copy[17] == 0;
        memcpy(mota, copy.data(), copy.size());
        return true;
    } catch (const std::exception&) { return false; }
}
