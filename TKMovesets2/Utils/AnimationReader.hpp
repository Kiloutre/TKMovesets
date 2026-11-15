#pragma once
#include <algorithm>
#include <cstring>
#include <span>
#include <stdexcept>
#include <type_traits>
#include "GameTypes.h"

namespace TAnimUtils {
class AnimationReader
{
    std::span<const Byte> m_bytes;
public:
    explicit AnimationReader(std::span<const Byte> bytes) : m_bytes(bytes) {}
    size_t Size() const { return m_bytes.size(); }
    void Require(size_t offset, size_t length) const {
        if (offset > m_bytes.size() || length > m_bytes.size() - offset)
            throw std::out_of_range("Animation data is truncated");
    }
    template<class T> T Read(size_t offset, bool swapped = false) const {
        Require(offset, sizeof(T));
        T value;
        memcpy(&value, m_bytes.data() + offset, sizeof(value));
        if (swapped) {
            auto bytes = reinterpret_cast<Byte*>(&value);
            std::reverse(bytes, bytes + sizeof(value));
        }
        return value;
    }
};

struct AnimationLayout
{
    bool swapped;
    uint16_t type;
    uint16_t bones;
    uint32_t frames;
    uint16_t floatBones = 0;
    size_t descriptors = 0;
    size_t postDescriptors = 0;
    size_t basePose = 0;
    size_t keyframes = 0;
    size_t size = 0;
};
AnimationLayout ReadAnimationLayout(std::span<const Byte> animation);
template<class Source>
AnimationLayout ReadAnimationLayoutFrom(Source& input, bool allChunks = true)
{
    auto tag = input.template Read<uint16_t>(0);
    bool swapped = tag == 0x6400 || tag == 0xc800;
    auto type = input.template Read<uint16_t>(0, swapped);
    auto bones = input.template Read<uint16_t>(2, swapped);
    if ((type != 0x64 && type != 0xc8) || bones == 0 || bones > 255)
        throw std::invalid_argument("Invalid animation type or bone count");
    AnimationLayout layout{swapped, type, bones, 0};
    if (type == 0xc8) {
        layout.frames = input.template Read<uint32_t>(4, swapped);
        if (!layout.frames) throw std::invalid_argument("Animation has no frames");
        uint64_t size = 8ULL + bones * 4ULL + static_cast<uint64_t>(bones) * 12 * layout.frames;
        if (size > input.Size()) throw std::out_of_range("Animation frames are truncated");
        layout.basePose = 8 + bones * 4;
        layout.size = static_cast<size_t>(size);
        return layout;
    }
    layout.descriptors = 4;
    layout.postDescriptors = 4 + bones * 2;
    layout.frames = input.template Read<uint16_t>(layout.postDescriptors, swapped);
    layout.floatBones = input.template Read<uint16_t>(layout.postDescriptors + 4, swapped);
    if (!layout.frames || layout.floatBones > bones)
        throw std::invalid_argument("Invalid animation frame or float-bone count");
    size_t floating = 0;
    for (size_t i = 0; i < bones; ++i) {
        auto descriptor = input.template Read<uint16_t>(4 + i * 2, swapped);
        if (descriptor < 4 || descriptor > 7) ++floating;
    }
    if (floating != layout.floatBones) throw std::invalid_argument("Float-bone descriptors disagree with header");
    layout.basePose = layout.postDescriptors + 6 + layout.floatBones * 4;
    layout.keyframes = layout.basePose + bones * 6 + layout.floatBones * 6;
    input.Require(layout.postDescriptors, layout.keyframes - layout.postDescriptors);
    layout.size = layout.keyframes;
    // The compressed tracks contain the deltas after the initial pose.
    const size_t chunks = (layout.frames + 14) / 16;
    input.Require(layout.keyframes, chunks * 4);
    for (size_t chunk = allChunks || !chunks ? 0 : chunks - 1; chunk < chunks; ++chunk) {
        size_t offset = input.template Read<uint32_t>(layout.keyframes + chunk * 4, swapped);
        if (offset < chunks * 4 || offset > input.Size() - layout.keyframes)
            throw std::out_of_range("Invalid animation keyframe offset");
        size_t cursor = layout.keyframes + offset;
        for (size_t channel = 0; channel < bones * 3; ++channel) {
            auto length = input.template Read<Byte>(cursor) >> 2;
            if (length == 0) throw std::invalid_argument("Animation channel has zero length");
            input.Require(cursor, length);
            cursor += length;
        }
        layout.size = (std::max)(layout.size, cursor);
    }
    return layout;

}

}
