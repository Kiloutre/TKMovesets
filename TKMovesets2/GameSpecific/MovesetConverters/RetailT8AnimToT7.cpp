#include "RetailT8AnimToT7.hpp"
#include "T7AnimToRetailT8.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <numbers>
#include <stdexcept>
#include <string>

namespace RetailT8AnimToT7 {
namespace {
// Same conventions as T7AnimToRetailT8.cpp: quaternions x y z w, Hamilton product, ZYX Euler = Rz * Ry * Rx.
using Quat = std::array<double, 4>;
using Euler = std::array<double, 3>;
constexpr double Pi = std::numbers::pi;
constexpr Quat Identity{0, 0, 0, 1};
Quat Mul(const Quat& a, const Quat& b) {
    return {a[3]*b[0] + a[0]*b[3] + a[1]*b[2] - a[2]*b[1],
            a[3]*b[1] - a[0]*b[2] + a[1]*b[3] + a[2]*b[0],
            a[3]*b[2] + a[0]*b[1] - a[1]*b[0] + a[2]*b[3],
            a[3]*b[3] - a[0]*b[0] - a[1]*b[1] - a[2]*b[2]};
}
Quat Conj(const Quat& q) { return {-q[0], -q[1], -q[2], q[3]}; }
Quat Norm(const Quat& q) {
    const double n = std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    return n > 1e-10 ? Quat{q[0]/n, q[1]/n, q[2]/n, q[3]/n} : Identity;
}
Quat FromEuler(const Euler& e) {
    const Quat x{std::sin(e[0]/2), 0, 0, std::cos(e[0]/2)}, y{0, std::sin(e[1]/2), 0, std::cos(e[1]/2)}, z{0, 0, std::sin(e[2]/2), std::cos(e[2]/2)};
    return Mul(Mul(z, y), x);
}
Euler ToEuler(const Quat& q) {
    return {std::atan2(2*(q[3]*q[0] + q[1]*q[2]), 1 - 2*(q[0]*q[0] + q[1]*q[1])),
            std::asin(std::fmax(-1.0, std::fmin(1.0, 2*(q[3]*q[1] - q[2]*q[0])))),
            std::atan2(2*(q[3]*q[2] + q[0]*q[1]), 1 - 2*(q[1]*q[1] + q[2]*q[2]))};
}
double Wrap(double angle) { return std::remainder(angle, 2*Pi); } // [-pi, pi]
// Twist about Z (the only axis the forward conversion reads for lower arms and lower legs).
double TwistZ(const Quat& q) { return Wrap(2*std::atan2(q[2], q[3])); }

enum Channel { GlobalPos, LocalPos, DirY, FullBody, UpperBody, LowerBody, SpineFlex, Neck, Head,
               R_Shoulder, R_UpperArm, R_LowerArm, R_Hand, L_Shoulder, L_UpperArm, L_LowerArm, L_Hand,
               R_UpperLeg, R_LowerLeg, R_Foot, L_UpperLeg, L_LowerLeg, L_Foot, ChannelCount };
static_assert(ChannelCount == std::size(ChannelTypes));
bool IsPosition(int channel) { return ChannelTypes[channel] == 11; }

const Quat CHead = FromEuler({Pi/2, 0, Pi/2}), CArm = FromEuler({-Pi/2, 0, 0}), CHand = FromEuler({Pi, 0, 0}),
           CLeg = FromEuler({0, 0, Pi});
constexpr Euler LowerBodyRest{-Pi/2, 0, -Pi/2};
constexpr Quat HipRotation{0.5, 0.5, 0.5, 0.5};

void Require(bool ok, const char* what) { if (!ok) throw std::invalid_argument(what); }

void CheckChannels(const RetailPanm::T7Animation& a) {
    Require(!a.keyframed, "T7 writer input must be in 0xC8 form (positions and ZYX Euler angles)");
    Require(a.frames && a.channels == ChannelCount && a.values.size() == size_t(a.frames) * ChannelCount * 3,
        "T7 writer input is not a 23-channel body animation");
    for (float v : a.values) Require(std::isfinite(v), "Nonfinite T7 channel value");
}

template<class T> void Append(std::vector<Byte>& out, T value) {
    const auto at = out.size(); out.resize(at + sizeof(T)); memcpy(out.data() + at, &value, sizeof(T));
}
template<class T> void Put(std::vector<Byte>& out, size_t at, T value) { memcpy(out.data() + at, &value, sizeof(T)); }

// ---- 0x64 bitstream (inverse of TAnimUtils::FromMemory::ParseAnimation0x64) ----
constexpr double ShortUnit = 0.000095873802; // pi / 32769, the decoder's mult_const
int16_t Wrap16(int64_t value) { return static_cast<int16_t>(static_cast<uint16_t>(value & 0xffff)); }

// Variable-length codes: opcode (4 bits), then `width` bits holding value - bias.
struct Code { int op, width, low; };
Code Classify(int value) {
    static constexpr Code codes[] = {{1,2,1},{2,2,-4},{3,4,5},{4,4,-20},{5,6,21},{6,6,-84},{7,8,85},{8,8,-340},
                                     {9,10,341},{10,10,-1364},{11,12,1365},{12,12,-5460}};
    if (!value) return {0, 0, 0};
    for (const auto& c : codes) if (value >= c.low && value < c.low + (1 << c.width)) return c;
    return {13, 16, 0};
}

class BitWriter {
    std::vector<bool> m_bits;
public:
    void Put(uint32_t value, int width) { for (int i = width - 1; i >= 0; --i) m_bits.push_back((value >> i) & 1); }
    void Value(int value, const Code& c) {
        if (c.op == 13) Put(static_cast<uint16_t>(value), 16);
        else if (c.width) Put(static_cast<uint32_t>(value - c.low), c.width);
    }
    // Channel stream: a prefix byte (total length << 2 | first two bits), then the remaining bits MSB first.
    void Flush(std::vector<Byte>& out) const {
        auto bit = [&](size_t i) -> unsigned { return i < m_bits.size() && m_bits[i] ? 1u : 0u; };
        const size_t data = m_bits.size() > 2 ? (m_bits.size() - 2 + 7) / 8 : 0;
        if (data + 1 > 63) throw std::logic_error("0x64 channel block exceeds 63 bytes");
        out.push_back(static_cast<Byte>(((data + 1) << 2) | (bit(0) << 1) | bit(1)));
        for (size_t b = 0; b < data; ++b) {
            Byte v = 0;
            for (size_t i = 0; i < 8; ++i) v = static_cast<Byte>((v << 1) | bit(2 + b * 8 + i));
            out.push_back(v);
        }
    }
};

// One keyframe block of one channel. `start` is the decoder's value before the block, `targets` the wanted
// int16 values of the block's frames (1 to 16). Symbol 0 moves to the first target, later symbols are second
// differences (the decoder accumulates them into a velocity that starts at zero).
void EncodeBlock(std::vector<Byte>& out, int start, std::span<const int16_t> targets) {
    std::vector<int> symbols{Wrap16(int(targets[0]) - start)};
    int velocity = 0;
    for (size_t j = 1; j < targets.size(); ++j) {
        const int wanted = Wrap16(int(targets[j]) - int(targets[j - 1]));
        const int delta = Wrap16(int64_t(wanted) - velocity);
        velocity += delta;
        symbols.push_back(delta);
    }
    // A trailing hold (every later frame equal) is one end marker: the decoder stops accumulating there.
    size_t hold = targets.size();
    while (hold > 1 && targets[hold - 1] == targets[hold - 2]) --hold;
    const bool end = hold < targets.size();
    symbols.resize(hold);

    BitWriter bits;
    for (size_t i = 0; i < symbols.size();) {
        const auto code = Classify(symbols[i]);
        bits.Put(code.op, 4); bits.Value(symbols[i], code);
        size_t run = 0;
        while (run < 15 && i + 1 + run < symbols.size() && Classify(symbols[i + 1 + run]).op == code.op) ++run;
        // Repeat (opcode 14, count): `count` more values in the mode just used; only ever after a symbol of this
        // channel, since the mode carried in from the previous channel depends on the decoded frame.
        if (run >= 3) {
            bits.Put(14, 4); bits.Put(static_cast<uint32_t>(run), 4);
            for (size_t k = 0; k < run; ++k) bits.Value(symbols[i + 1 + k], code);
            i += 1 + run;
        } else ++i;
    }
    if (end) bits.Put(15, 4);
    bits.Flush(out);
}

// Rotation channel -> KEF triple (a, b, c): axis (cos a sin b, -sin a, cos a cos b), angle c, of the inverse of
// the 0xC8 rotation. Among the equivalent triples, the one closest to the previous frame keeps deltas small.
Euler KefTriple(const Euler& c8, const Euler* previous) {
    const Quat q = Norm(Conj(FromEuler(c8)));
    std::array<Euler, 4> candidates;
    for (int s = 0; s < 2; ++s) {
        const double sign = s ? -1 : 1;
        const double x = q[0]*sign, y = q[1]*sign, z = q[2]*sign, w = q[3]*sign;
        const double n = std::sqrt(x*x + y*y + z*z);
        double a = previous ? (*previous)[0] : 0, b = previous ? (*previous)[1] : 0;
        if (n > 1e-9) { a = std::asin(std::fmax(-1.0, std::fmin(1.0, -y / n))); b = std::atan2(x / n, z / n); }
        const double c = Wrap(2 * std::atan2(n, w));
        candidates[s * 2] = {a, Wrap(b), c};
        candidates[s * 2 + 1] = {Wrap(-a), Wrap(b + Pi), Wrap(-c)}; // opposite axis, opposite angle
    }
    if (!previous) return candidates[0];
    auto distance = [&](const Euler& e) {
        double d = 0; for (int i = 0; i < 3; ++i) { const double v = Wrap(e[i] - (*previous)[i]); d += v * v; } return d;
    };
    Euler best = candidates[0];
    for (const auto& e : candidates) if (distance(e) < distance(best)) best = e;
    return best;
}
int16_t QuantizeAngle(double angle) { return Wrap16(std::llround(Wrap(angle) / ShortUnit)); }
}

RetailPanm::T7Animation ToChannels(const RetailPanm::Animation& panm) {
    Require(panm.frames && !panm.tracks.empty(), "Empty PANM animation");
    std::map<std::string, const RetailPanm::Track*> tracks;
    for (const auto& track : panm.tracks) {
        Require(track.samples.size() == 1 || track.samples.size() == panm.frames, "PANM track length differs from animation");
        tracks.emplace(track.name, &track);
    }
    Require(tracks.contains("Top") && tracks.contains("Trans"), "PANM is not a body animation (needs Top and Trans)");
    // Positions are the only channels that can leave the float range (translation x 10; rotations come from normalized
    // quaternions). Checked on the samples before the per-frame walk: a static track holds one sample for any frame count.
    for (const char* name : {"Top", "Trans"})
        for (const auto& s : tracks.at(name)->samples)
            for (float t : s.translation)
                Require(std::fabs(double(t) * 10.0) <= std::numeric_limits<float>::max(), "PANM translation exceeds the T7 position range");
    std::map<std::string, Quat> rest;
    for (const auto& track : T7AnimToRetailT8::BuiltInSkeleton().tracks) {
        const auto& r = track.samples.front().rotation;
        rest[track.name] = {r[0], r[1], r[2], r[3]};
    }
    auto sample = [&](const char* name, uint32_t f) -> const RetailPanm::Sample* {
        const auto found = tracks.find(name);
        if (found == tracks.end()) return nullptr;
        const auto& samples = found->second->samples;
        return &samples[samples.size() == 1 ? 0 : f];
    };
    auto rot = [&](const char* name, uint32_t f) {
        if (const auto* s = sample(name, f)) return Norm({s->rotation[0], s->rotation[1], s->rotation[2], s->rotation[3]});
        const auto found = rest.find(name);
        return found != rest.end() ? found->second : Identity;
    };

    RetailPanm::T7Animation out{panm.frames, ChannelCount, std::vector<float>(size_t(panm.frames) * ChannelCount * 3), false};
    std::array<Euler, ChannelCount> previous{};
    for (uint32_t f = 0; f < panm.frames; ++f) {
        std::array<Quat, ChannelCount> q; q.fill(Identity);
        std::array<Euler, ChannelCount> e{};
        for (int i = 0; i < 3; ++i) {
            e[GlobalPos][i] = sample("Top", f)->translation[i] * 10.0;
            e[LocalPos][i] = sample("Trans", f)->translation[i] * 10.0;
        }
        q[DirY] = rot("MUKI", f);
        // Native T7 layout: FullBody is the body state (T8 HARA_ROT1: upright, face up/down, turned; still in most
        // animations, median range 0 over T7 Jin) and LowerBody the pelvis relative to it and to its rest. T7 blends
        // move transitions on LowerBody; pelvis motion carried in FullBody snapped at every move end (T8 Jin ws2 / dash
        // on T7, 2026-11-14). T7AnimToRetailT8 reads Rot back as FullBody * LowerBody and HARA_ROT1 as FullBody.
        const Quat pelvis = rot("Rot", f), body = rot("HARA_ROT1", f), orient = Norm(Mul(Conj(body), pelvis));
        q[FullBody] = body;
        q[LowerBody] = Norm(Mul(orient, FromEuler(LowerBodyRest)));
        q[UpperBody] = Norm(Mul(orient, Mul(HipRotation, Mul(rot("Spine1", f), rot("Spine2", f)))));
        q[Neck] = rot("Neck", f);
        const Quat head = rot("Head", f);
        q[Head] = Mul(CHead, {head[1], head[2], head[0], head[3]});
        for (const auto& [name, ch] : {std::pair{"R_Shoulder", R_Shoulder}, std::pair{"L_Shoulder", L_Shoulder}}) {
            const Quat r = rot(name, f);
            q[ch] = Mul(CArm, {r[0], -r[2], r[1], r[3]});
        }
        for (const auto& [name, ch] : {std::pair{"R_UpperArm", R_UpperArm}, std::pair{"L_UpperArm", L_UpperArm}}) {
            const Quat r = rot(name, f);
            q[ch] = Mul(CArm, {r[0], -r[1], -r[2], r[3]});
        }
        q[R_Hand] = Mul(CHand, rot("R_Hand", f));
        const Quat lHand = rot("L_Hand", f);
        q[L_Hand] = {lHand[0], -lHand[1], -lHand[2], lHand[3]};
        q[R_UpperLeg] = Mul(CLeg, rot("R_UpperLeg", f));
        q[L_UpperLeg] = Mul(CLeg, rot("L_UpperLeg", f));
        q[R_Foot] = rot("R_Foot", f);
        q[L_Foot] = rot("L_Foot", f);
        for (int ch = 0; ch < ChannelCount; ++ch)
            if (!IsPosition(ch)) e[ch] = ToEuler(Norm(q[ch]));
        e[R_LowerArm] = {0, 0, -TwistZ(rot("R_LowerArm", f))};
        e[L_LowerArm] = {0, 0, -TwistZ(rot("L_LowerArm", f))};
        e[R_LowerLeg] = {0, 0, TwistZ(rot("R_LowerLeg", f))};
        e[L_LowerLeg] = {0, 0, TwistZ(rot("L_LowerLeg", f))};
        for (int ch = 0; ch < ChannelCount; ++ch) {
            // Keep Euler angles continuous across frames (no 2 pi jumps between neighbouring frames).
            if (f && !IsPosition(ch)) for (int i = 0; i < 3; ++i) e[ch][i] = previous[ch][i] + Wrap(e[ch][i] - previous[ch][i]);
            previous[ch] = e[ch];
            for (int i = 0; i < 3; ++i) out.values[(size_t(f) * ChannelCount + ch) * 3 + i] = static_cast<float>(e[ch][i]);
        }
    }
    for (float v : out.values) Require(std::isfinite(v), "Nonfinite converted T7 channel");
    return out;
}

std::vector<Byte> WriteC8(const RetailPanm::T7Animation& a) {
    CheckChannels(a);
    std::vector<Byte> out;
    out.reserve(8 + ChannelCount * 4 + a.values.size() * 4);
    Append<uint16_t>(out, 0xC8); Append<uint16_t>(out, ChannelCount); Append<uint32_t>(out, a.frames);
    for (auto type : ChannelTypes) Append<uint32_t>(out, type);
    for (float v : a.values) Append<float>(out, v);
    return out;
}

std::vector<Byte> Write64(const RetailPanm::T7Animation& a) {
    CheckChannels(a);
    Require(a.frames <= 0xFFFF, "0x64 animations hold at most 65535 frames");
    auto value = [&](uint32_t f, int ch, int axis) { return a.values[(size_t(f) * ChannelCount + ch) * 3 + axis]; };

    // Per channel axis, the decoder value of every frame: int16 angles, or float offsets from frame 0 in `scale` steps.
    const size_t count = size_t(a.frames) * ChannelCount * 3;
    std::vector<int16_t> targets(count);
    std::array<float, ChannelCount> scale{};
    std::vector<Euler> previous(ChannelCount);
    for (uint32_t f = 0; f < a.frames; ++f)
        for (int ch = 0; ch < ChannelCount; ++ch) {
            if (IsPosition(ch)) continue;
            const Euler kef = KefTriple({value(f, ch, 0), value(f, ch, 1), value(f, ch, 2)}, f ? &previous[ch] : nullptr);
            previous[ch] = kef;
            for (int i = 0; i < 3; ++i) targets[(size_t(f) * ChannelCount + ch) * 3 + i] = QuantizeAngle(kef[i]);
        }
    for (int ch = 0; ch < ChannelCount; ++ch) {
        if (!IsPosition(ch)) continue;
        double range = 0;
        for (uint32_t f = 1; f < a.frames; ++f)
            for (int i = 0; i < 3; ++i) range = std::fmax(range, std::fabs(double(value(f, ch, i)) - value(0, ch, i)));
        scale[ch] = range > 0 ? static_cast<float>(range / 32767) : 1.0f;
        if (range > 0 && !(scale[ch] > 0)) scale[ch] = std::numeric_limits<float>::min();
        for (uint32_t f = 0; f < a.frames; ++f)
            for (int i = 0; i < 3; ++i) {
                const double steps = (double(value(f, ch, i)) - value(0, ch, i)) / scale[ch];
                targets[(size_t(f) * ChannelCount + ch) * 3 + i] = static_cast<int16_t>(std::llround(std::fmax(-32767.0, std::fmin(32767.0, steps))));
            }
    }

    std::vector<Byte> out;
    Append<uint16_t>(out, 0x64); Append<uint16_t>(out, ChannelCount);
    for (auto type : ChannelTypes) Append<uint16_t>(out, type);
    uint16_t floatChannels = 0;
    for (int ch = 0; ch < ChannelCount; ++ch) floatChannels += IsPosition(ch);
    Append<uint16_t>(out, static_cast<uint16_t>(a.frames));
    Append<uint8_t>(out, 0); // int16 shift: full pi/32769 precision
    Append<uint8_t>(out, 0); // float shift
    Append<uint16_t>(out, floatChannels);
    for (int ch = 0; ch < ChannelCount; ++ch) if (IsPosition(ch)) Append<float>(out, scale[ch]);
    // Base pose (frame 1): int16 angles, frame-0 floats.
    for (int ch = 0; ch < ChannelCount; ++ch)
        for (int i = 0; i < 3; ++i)
            if (IsPosition(ch)) Append<float>(out, value(0, ch, i));
            else Append<int16_t>(out, targets[size_t(ch) * 3 + i]);
    // Keyframe blocks of 16 frames after the base pose; every block restarts from the base pose.
    const size_t blocks = (size_t(a.frames) + 14) / 16, table = out.size();
    out.resize(table + blocks * 4);
    std::vector<int16_t> block;
    for (size_t k = 0; k < blocks; ++k) {
        Put<uint32_t>(out, table + k * 4, static_cast<uint32_t>(out.size() - table));
        const size_t first = 1 + k * 16, frames = std::min<size_t>(16, a.frames - first);
        for (int ch = 0; ch < ChannelCount; ++ch)
            for (int i = 0; i < 3; ++i) {
                const size_t axis = size_t(ch) * 3 + i;
                block.resize(frames);
                for (size_t j = 0; j < frames; ++j) block[j] = targets[(first + j) * ChannelCount * 3 + axis];
                EncodeBlock(out, IsPosition(ch) ? 0 : targets[axis], block);
            }
    }
    return out;
}

std::vector<Byte> Convert(std::span<const Byte> panm, Format format) {
    const auto channels = ToChannels(RetailPanm::Decode(panm));
    return format == Format::Keyframed ? Write64(channels) : WriteC8(channels);
}
}
