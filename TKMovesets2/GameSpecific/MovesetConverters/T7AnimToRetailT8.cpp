#include "T7AnimToRetailT8.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <numbers>
#include <stdexcept>
#include <string>

namespace T7AnimToRetailT8 {
namespace {
// Closed-form retargeting: per-bone constant frame changes on ZYX Euler input.
using Quat = std::array<double, 4>; // x y z w
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
using Euler = std::array<double, 3>;
// ZYX Euler (radians) -> quaternion, q = Rz * Ry * Rx.
Quat FromEuler(const Euler& e) {
    const Quat x{std::sin(e[0]/2), 0, 0, std::cos(e[0]/2)}, y{0, std::sin(e[1]/2), 0, std::cos(e[1]/2)}, z{0, 0, std::sin(e[2]/2), std::cos(e[2]/2)};
    return Mul(Mul(z, y), x);
}
Euler ToEuler(const Quat& q) {
    return {std::atan2(2*(q[3]*q[0] + q[1]*q[2]), 1 - 2*(q[0]*q[0] + q[1]*q[1])),
            std::asin(std::fmax(-1.0, std::fmin(1.0, 2*(q[3]*q[1] - q[2]*q[0])))),
            std::atan2(2*(q[3]*q[2] + q[0]*q[1]), 1 - 2*(q[1]*q[1] + q[2]*q[2]))};
}
Quat Slerp(const Quat& a, Quat b, double t) {
    double dot = a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
    if (dot < 0) { for (auto& c : b) c = -c; dot = -dot; }
    dot = std::fmin(1.0, dot);
    if (dot > 0.9995) return Norm({a[0] + t*(b[0]-a[0]), a[1] + t*(b[1]-a[1]), a[2] + t*(b[2]-a[2]), a[3] + t*(b[3]-a[3])});
    const double th0 = std::acos(dot), th = th0 * t;
    const double s0 = std::cos(th) - dot * std::sin(th) / std::sin(th0), s1 = std::sin(th) / std::sin(th0);
    return Norm({s0*a[0] + s1*b[0], s0*a[1] + s1*b[1], s0*a[2] + s1*b[2], s0*a[3] + s1*b[3]});
}
Quat AxisZ(double angle) { return {0, 0, std::sin(angle/2), std::cos(angle/2)}; }

// The 23 standard T7 body channels.
enum Channel { GlobalPos, LocalPos, DirY, FullBody, UpperBody, LowerBody, SpineFlex, Neck, Head,
               R_Shoulder, R_UpperArm, R_LowerArm, R_Hand, L_Shoulder, L_UpperArm, L_LowerArm, L_Hand,
               R_UpperLeg, R_LowerLeg, R_Foot, L_UpperLeg, L_LowerLeg, L_Foot, ChannelCount };
static_assert(ChannelCount == BodyChannels);
bool IsPosition(int channel) { return channel == GlobalPos || channel == LocalPos || channel == SpineFlex; }

const Quat CHead = FromEuler({Pi/2, 0, Pi/2}), CArm = FromEuler({-Pi/2, 0, 0}), CHand = FromEuler({Pi, 0, 0}),
           CLeg = FromEuler({0, 0, Pi}), BLower = FromEuler({-Pi/2, 0, -Pi/2});
constexpr Quat HipRotation{0.5, 0.5, 0.5, 0.5}; // T8 Hip is static at this rotation; Spine1 is expressed under it
constexpr double Spine1Ratio = 0.45;            // share of T7 UpperBody given to Spine1, the rest goes to Spine2

// One frame of T7 channels in 0xc8 form: positions in source units, rotations as ZYX Euler.
struct Frame { std::array<Euler, ChannelCount> c; };
Frame ReadFrame(const RetailPanm::T7Animation& source, uint32_t frame) {
    Frame out{};
    for (int ch = 0; ch < ChannelCount; ++ch) {
        const float* v = &source.values[(size_t(frame) * source.channels + ch) * 3];
        if (!source.keyframed || IsPosition(ch)) { out.c[ch] = {v[0], v[1], v[2]}; continue; }
        // 0x64 rotations: axis (cos a sin b, -sin a, cos a cos b), angle c. 0xc8 stores the inverse rotation as Euler.
        const double a = v[0], b = v[1], c = v[2], s = std::sin(c/2);
        const Quat q{std::cos(a)*std::sin(b)*s, -std::sin(a)*s, std::cos(a)*std::cos(b)*s, std::cos(c/2)};
        out.c[ch] = ToEuler(Conj(q));
    }
    return out;
}

struct Pose { Quat rotation = Identity; std::array<double, 3> translation{}; bool hasTranslation = false; };
using Bones = std::map<std::string, Pose>;
// Per T8 bone, the converted local transform of one frame.
Bones ConvertFrame(const Frame& f, bool native) {
    Bones out;
    auto q = [&](int ch) { return FromEuler(f.c[ch]); };
    out["Top"] = {Identity, {f.c[GlobalPos][0]/10, f.c[GlobalPos][1]/10, f.c[GlobalPos][2]/10}, true};
    out["Trans"] = {Identity, {f.c[LocalPos][0]/10, f.c[LocalPos][1]/10, f.c[LocalPos][2]/10}, true};
    // Native T7 data carries the pelvis orientation in LowerBody (relative to its constant rest BLower);
    // older files converted from T8 pin LowerBody to BLower and carry it in FullBody (current ones use the native layout).
    const Quat orient = native ? Norm(Mul(q(LowerBody), Conj(BLower))) : Identity;
    out["Rot"].rotation = Norm(Mul(q(FullBody), orient));
    // Native T8 layout: MUKI is the facing (T7 DirY: identity, or a 180 degree yaw for back-turned moves) and
    // HARA_ROT1 is the T7 FullBody rotation, the body state the game reads during juggles and downs (upright,
    // turned, face up/down). Putting FullBody into MUKI instead flipped juggled opponents (issue #178).
    // This holds when LowerBody stays at its rest too: T7 down loops (Mco_dwkam0, DOWN_AO_TES...) keep the pelvis at
    // rest and lie through FullBody, and native T8 has that lying rotation in HARA_ROT1. An upright HARA_ROT1 there
    // made every move ending on the ground flip the body when it handed over to them.
    out["MUKI"].rotation = Norm(q(DirY));
    out["HARA_ROT1"].rotation = Norm(q(FullBody));
    const Quat upper = Norm(Mul(Conj(HipRotation), Mul(Conj(orient), q(UpperBody))));
    const Quat spine1 = Slerp(Identity, upper, Spine1Ratio);
    out["Spine1"].rotation = spine1;
    out["Spine2"].rotation = Norm(Mul(Conj(spine1), upper));
    out["Neck"].rotation = q(Neck);
    const Quat head = Mul(Conj(CHead), q(Head));
    out["Head"].rotation = Norm({head[2], head[0], head[1], head[3]});
    for (const auto& [name, ch] : {std::pair{"R_Shoulder", R_Shoulder}, std::pair{"L_Shoulder", L_Shoulder}}) {
        const Quat r = Mul(Conj(CArm), q(ch));
        out[name].rotation = Norm({r[0], r[2], -r[1], r[3]});
    }
    for (const auto& [name, ch] : {std::pair{"R_UpperArm", R_UpperArm}, std::pair{"L_UpperArm", L_UpperArm}}) {
        const Quat r = Mul(Conj(CArm), q(ch));
        out[name].rotation = Norm({r[0], -r[1], -r[2], r[3]});
    }
    out["R_LowerArm"].rotation = AxisZ(-f.c[R_LowerArm][2]);
    out["L_LowerArm"].rotation = AxisZ(-f.c[L_LowerArm][2]);
    out["R_Hand"].rotation = Norm(Mul(Conj(CHand), q(R_Hand)));
    const Quat lHand = q(L_Hand);
    out["L_Hand"].rotation = Norm({lHand[0], -lHand[1], -lHand[2], lHand[3]});
    out["R_UpperLeg"].rotation = Norm(Mul(Conj(CLeg), q(R_UpperLeg)));
    out["L_UpperLeg"].rotation = Norm(Mul(Conj(CLeg), q(L_UpperLeg)));
    out["R_LowerLeg"].rotation = AxisZ(f.c[R_LowerLeg][2]);
    out["L_LowerLeg"].rotation = AxisZ(f.c[L_LowerLeg][2]);
    out["R_Foot"].rotation = q(R_Foot);
    out["L_Foot"].rotation = q(L_Foot);
    return out;
}

// The template's skeleton: bone set and order, per-track options and each bone's frame-0 pose (front sample).
RetailPanm::Animation Skeleton(std::span<const Byte> templatePanm) {
    if (!UsableTemplate(templatePanm)) throw std::invalid_argument("Template is not a native raw body PANM with Top/Trans");
    return RetailPanm::DecodeRaw(templatePanm);
}
}

bool UsableTemplate(std::span<const Byte> panm) {
    try {
        const auto decoded = RetailPanm::DecodeRaw(panm);
        bool top = false, trans = false;
        for (const auto& t : decoded.tracks) { top |= t.name == "Top"; trans |= t.name == "Trans"; }
        return top && trans && decoded.tracks.size() >= 20;
    } catch (const std::exception&) { return false; }
}

namespace {
// Built-in skeleton (T7AnimToRetailT8Skeleton.inc): the 43 retail body bones in native order with their track options
// and the frame-0 pose of a native idle. Every conversion uses the same skeleton, whatever the import target.
struct SkeletonBone {
    const char* name; RetailPanm::TrackOption options[4];
    std::array<float, 3> scale; std::array<float, 4> rotation; std::array<float, 3> translation;
};
constexpr SkeletonBone SkeletonBones[] = {
#include "T7AnimToRetailT8Skeleton.inc"
};
std::vector<Byte> ConvertOnto(const RetailPanm::T7Animation& source, RetailPanm::Animation output);
}

const RetailPanm::Animation& BuiltInSkeleton() {
    static const RetailPanm::Animation skeleton = [] {
        RetailPanm::Animation a; a.frames = 1;
        for (const auto& bone : SkeletonBones) {
            RetailPanm::Track track; track.name = bone.name; track.options.clear();
            for (const auto& option : bone.options) if (option.field) track.options.push_back(option);
            RetailPanm::Sample sample; sample.scale = bone.scale; sample.rotation = bone.rotation; sample.translation = bone.translation;
            track.samples = {sample};
            a.tracks.push_back(std::move(track));
        }
        return a;
    }();
    return skeleton;
}

std::vector<Byte> Convert(const RetailPanm::T7Animation& source) { return ConvertOnto(source, BuiltInSkeleton()); }

uint32_t MaxConvertibleFrames() {
    static const uint32_t frames = [] {
        // The bones ConvertFrame animates (Hip is pinned to one sample by ConvertOnto).
        const Bones converted = ConvertFrame(Frame{}, true);
        size_t animated = 0;
        for (const auto& track : BuiltInSkeleton().tracks)
            if (converted.contains(track.name) && track.name != "Hip") ++animated;
        if (!animated) throw std::logic_error("Built-in skeleton has no converted bones");
        return static_cast<uint32_t>((32ULL << 20) / (animated * sizeof(RetailPanm::Sample)));
    }();
    return frames;
}

std::vector<Byte> Convert(const RetailPanm::T7Animation& source, std::span<const Byte> templatePanm) {
    return ConvertOnto(source, Skeleton(templatePanm));
}

namespace {
std::vector<Byte> ConvertOnto(const RetailPanm::T7Animation& source, RetailPanm::Animation output) {
    if (!source.frames || source.channels < ChannelCount || source.values.size() != size_t(source.frames) * source.channels * 3)
        throw std::invalid_argument("T7 animation layout is not a 23-bone body animation");
    std::vector<Frame> frames(source.frames);
    for (uint32_t f = 0; f < source.frames; ++f) frames[f] = ReadFrame(source, f);
    // Native T7 convention unless every LowerBody frame sits at the constant rest (-pi/2, 0, -pi/2).
    bool native = false;
    for (const auto& f : frames) {
        const auto& lower = f.c[LowerBody];
        native |= !(std::abs(lower[0] + Pi/2) < 0.001 && std::abs(lower[1]) < 0.001 && std::abs(lower[2] + Pi/2) < 0.001);
    }
    std::vector<Bones> converted(source.frames);
    for (uint32_t f = 0; f < source.frames; ++f) converted[f] = ConvertFrame(frames[f], native);
    for (auto& track : output.tracks) {
        const auto base = track.samples.front();
        // Hip is static at the pose the spine formula expects; other unconverted bones keep the template's frame 0.
        if (track.name == "Hip") {
            auto sample = base;
            for (int i = 0; i < 4; ++i) sample.rotation[i] = static_cast<float>(HipRotation[i]);
            track.samples = {sample};
            continue;
        }
        if (!converted.front().contains(track.name)) { track.samples = {base}; continue; }
        // A body state that never changes stays one static sample, like native HARA_ROT1 in most animations.
        if (track.name == "HARA_ROT1" && std::all_of(converted.begin(), converted.end(), [&](const Bones& b) {
                return b.at(track.name).rotation == converted.front().at(track.name).rotation; })) {
            auto sample = base;
            const Quat r = converted.front().at(track.name).rotation;
            for (int i = 0; i < 4; ++i) sample.rotation[i] = static_cast<float>(r[i]);
            track.samples = {sample};
            continue;
        }
        track.samples.assign(source.frames, base);
        Quat previous = Identity;
        for (uint32_t f = 0; f < source.frames; ++f) {
            const auto& pose = converted[f].at(track.name);
            Quat r = pose.rotation;
            if (f && r[0]*previous[0] + r[1]*previous[1] + r[2]*previous[2] + r[3]*previous[3] < 0) for (auto& c : r) c = -c;
            previous = r;
            auto& sample = track.samples[f];
            for (int i = 0; i < 4; ++i) sample.rotation[i] = static_cast<float>(r[i]);
            // Rotation bones keep the template's local translation (bone length); Top/Trans carry root motion.
            if (pose.hasTranslation) for (int i = 0; i < 3; ++i) sample.translation[i] = static_cast<float>(pose.translation[i]);
        }
    }
    output.frames = source.frames;
    return RetailPanm::Encode(output);
}
}

std::vector<Byte> Rewrite(const RetailPanm::Animation& source, std::span<const Byte> templatePanm, size_t* matchedBones) {
    if (!source.frames || source.tracks.empty()) throw std::invalid_argument("Empty source animation");
    std::map<std::string, const RetailPanm::Track*> tracks;
    for (const auto& track : source.tracks) {
        if (track.samples.size() != 1 && track.samples.size() != source.frames) throw std::invalid_argument("Source track length differs from animation");
        if (!tracks.emplace(track.name, &track).second) throw std::invalid_argument("Duplicate source bone");
    }
    auto output = Skeleton(templatePanm);
    // Template bones absent from the source keep the template's frame-0 pose; extra source bones are dropped.
    size_t matched = 0;
    for (auto& track : output.tracks)
        if (auto found = tracks.find(track.name); found != tracks.end()) { track.samples = found->second->samples; ++matched; }
        else track.samples = {track.samples.front()};
    if (!matched) throw std::invalid_argument("Source animation shares no bone with the template skeleton");
    if (matchedBones) *matchedBones = matched;
    output.frames = source.frames;
    return RetailPanm::Encode(output);
}
}
