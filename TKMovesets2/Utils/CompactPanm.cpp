#include "CompactPanm.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <zstd.h>
#if defined(_M_X64) || defined(_M_IX86) || defined(__SSE2__)
#include <xmmintrin.h>
#define COMPACT_SSE 1
#endif

namespace CompactPanm {
namespace {
using View = std::span<const uint8_t>;
using Output = std::span<uint8_t>;
constexpr size_t MaximumTracks = 16384;
static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);

void Check(bool ok, const char* message) { if (!ok) throw std::invalid_argument(message); }
template<class T> T Read(View bytes, size_t at) {
    Check(at <= bytes.size() && sizeof(T) <= bytes.size() - at, "Truncated compact PANM");
    T value; std::memcpy(&value, bytes.data() + at, sizeof(value)); return value;
}
template<class T> void Write(Output bytes, size_t at, T value) {
    Check(at <= bytes.size() && sizeof(T) <= bytes.size() - at, "Invalid compact PANM output");
    std::memcpy(bytes.data() + at, &value, sizeof(value));
}
template<class T> void Append(Bytes& bytes, T value) {
    const auto at = bytes.size(); bytes.resize(at + sizeof(value)); Write<T>(bytes, at, value);
}

// Prediction is a file-format operation, independent of the caller's rounding mode, FTZ/DAZ,
// and exception masks. This translation unit must be compiled with strict FP and no FMA.
class FloatingPointScope {
    std::fenv_t saved{};
#ifdef COMPACT_SSE
    unsigned csr = _mm_getcsr();
#endif
public:
    FloatingPointScope() {
        Check(std::feholdexcept(&saved) == 0, "Cannot save floating-point state");
        if (std::fesetround(FE_TONEAREST) != 0) { std::fesetenv(&saved); throw std::runtime_error("Cannot set rounding mode"); }
#ifdef COMPACT_SSE
        _mm_setcsr((_mm_getcsr() | 0x1f80u) & ~0xe040u);
#endif
    }
    ~FloatingPointScope() {
        std::fesetenv(&saved);
#ifdef COMPACT_SSE
        _mm_setcsr(csr);
#endif
    }
};

struct Reader {
    View bytes;
    size_t Field(size_t table, size_t index) const {
        const int64_t vt = int64_t(table) - Read<int32_t>(bytes, table);
        Check(vt >= 0 && uint64_t(vt) < bytes.size(), "Invalid PANM vtable");
        const auto length = Read<uint16_t>(bytes, size_t(vt));
        const auto object = Read<uint16_t>(bytes, size_t(vt) + 2);
        Check(length >= 4 && !(length & 1) && length <= bytes.size() - size_t(vt) &&
            table <= bytes.size() && object >= 4 && object <= bytes.size() - table, "Invalid PANM table");
        if (4 + index * 2 >= length) return 0;
        const auto offset = Read<uint16_t>(bytes, size_t(vt) + 4 + index * 2);
        Check(!offset || (offset >= 4 && offset <= object - 4), "Invalid PANM field");
        return offset ? table + offset : 0;
    }
    uint32_t Value(size_t table, size_t index) const { const auto at = Field(table, index); return at ? Read<uint32_t>(bytes, at) : 0; }
    size_t Ref(size_t at) const { const auto offset = Read<uint32_t>(bytes, at); Check(offset && offset < bytes.size() - at, "Invalid PANM reference"); return at + offset; }
    size_t Object(size_t table, size_t index) const { const auto at = Field(table, index); Check(at != 0, "Missing PANM object"); return Ref(at); }
    uint32_t Count(size_t at) const { const auto n = Read<uint32_t>(bytes, at); Check(n <= 4096 && uint64_t(n) * 4 <= bytes.size() - at - 4, "Invalid PANM vector"); return n; }
};
struct Track {
    size_t at, size;
    uint32_t frames;
    bool raw = false;
    unsigned channels = 0, bitsPerFrame = 0;
    std::array<uint8_t, 12> widths{};
};
std::vector<Track> Tracks(View bytes) {
    Check(bytes.size() >= 8 && bytes.size() <= MaximumSize && !std::memcmp(bytes.data() + 4, "PANM", 4), "Invalid PANM extent or signature");
    Reader r{bytes}; const auto root = r.Ref(0), anim = r.Object(root, 3);
    const auto base = r.Value(anim, 5);
    Check(base >= 8 && base <= bytes.size(), "Invalid PANM data base");
    r.bytes = bytes.first(base);
    Check(r.Ref(0) == root && r.Object(root, 3) == anim, "PANM metadata in sample block");
    struct Descriptor { uint32_t kind, encoding; Track track; };
    std::vector<Descriptor> descriptors;
    const auto groups = r.Object(anim, 4); size_t visited = 0;
    for (uint32_t g = 0; g < r.Count(groups); ++g) {
        const auto bones = r.Object(r.Ref(groups + 4 + g * 4), 2);
        for (uint32_t j = 0; j < r.Count(bones); ++j) {
            Check(++visited <= 65536, "PANM traversal budget exceeded");
            const auto tracks = r.Object(r.Ref(bones + 4 + j * 4), 2);
            for (uint32_t k = 0; k < r.Count(tracks); ++k) {
                Check(descriptors.size() < MaximumTracks, "PANM track budget exceeded");
                const auto table = r.Ref(tracks + 4 + k * 4);
                const auto offset = r.Value(table, 6), size = r.Value(table, 7);
                Check(offset <= bytes.size() - base && size <= bytes.size() - base - offset, "PANM track outside data");
                descriptors.push_back({r.Value(table, 2), r.Value(table, 3), {base + size_t(offset), size, r.Value(table, 4)}});
            }
        }
    }
    std::stable_sort(descriptors.begin(), descriptors.end(), [](const auto& a, const auto& b) { return a.track.at < b.track.at; });
    std::vector<Track> result; size_t end = base;
    for (auto descriptor : descriptors) {
        auto t = descriptor.track; const auto fullEnd = t.at + t.size;
        if (t.at < end || descriptor.kind != 1 || !t.frames || t.frames > 1000000) continue;
        if (descriptor.encoding == 1 && t.frames > 1 && uint64_t(t.frames) * 44 == t.size) t.raw = true;
        else {
            if (descriptor.encoding != 3 || t.size < 60 || Read<uint16_t>(bytes, t.at) != 4) continue;
            const auto header = Read<uint32_t>(bytes, t.at + 8);
            if (header < 60 || header > t.size || (header - 60) % 16) continue;
            t.channels = (header - 60) / 16;
            if (!t.channels || t.channels > 11) continue;
            bool valid = true;
            for (unsigned c = 0; c < t.channels; ++c) {
                const auto at = t.at + 16 + c * 16;
                const auto low = Read<float>(bytes, at), high = Read<float>(bytes, at + 4);
                auto width = Read<uint32_t>(bytes, at + 8);
                if (width > 32 || !std::isfinite(low) || !std::isfinite(high)) { valid = false; break; }
                if (low == high) width = 0;
                t.widths[c] = uint8_t(width); t.bitsPerFrame += width;
            }
            if (!valid) continue;
            const auto bits = Read<uint16_t>(bytes, t.at + 6);
            if (bits == t.bitsPerFrame + 1) { t.widths[t.channels++] = 1; ++t.bitsPerFrame; }
            if (!bits || bits != t.bitsPerFrame || (uint64_t(t.frames) * bits + 7) / 8 > t.size - header) continue;
            t.at += header; t.size = (size_t(t.frames) * bits + 7) / 8;
        }
        result.push_back(t); end = fullEnd;
    }
    return result;
}
bool SameTrack(const Track& a, const Track& b) {
    return a.at == b.at && a.size == b.size && a.frames == b.frames && a.raw == b.raw &&
        a.channels == b.channels && a.bitsPerFrame == b.bitsPerFrame && a.widths == b.widths;
}

uint32_t Mask(unsigned width) { return width == 32 ? UINT32_MAX : (1u << width) - 1; }
uint32_t Zigzag(uint32_t value) { return (value << 1) ^ (0u - (value >> 31)); }
uint32_t Unzigzag(uint32_t value) { return (value >> 1) ^ (0u - (value & 1)); }
struct Predictor {
    std::array<uint32_t, 4> previous{};
    uint32_t Integer(uint32_t value, unsigned mode, unsigned width, bool inverse) {
        const auto mask = Mask(width);
        if (mode == 4) { const auto result = value ^ previous[0]; previous[0] = inverse ? result : value; return result; }
        if (inverse) {
            if (mode) value = Unzigzag(value);
            for (unsigned j = mode; j; --j) { value = (value + previous[j - 1]) & mask; previous[j - 1] = value; }
        } else {
            for (unsigned j = 0; j < mode; ++j) { const auto old = previous[j]; previous[j] = value; value = (value - old) & mask; }
            if (mode) value = ((value << 1) ^ (0u - (value >> (width - 1)))) & mask;
        }
        return value;
    }
};
uint32_t FloatBits(double value) {
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max()) value = 0;
    return std::bit_cast<uint32_t>(static_cast<float>(value));
}
uint32_t Numerical(Predictor& history, uint32_t value, unsigned mode, size_t frame, bool inverse) {
    constexpr int coefficients[3][4] = {{2,-1,0,0}, {3,-3,1,0}, {4,-6,4,-1}};
    const unsigned order = 2 + (mode - 8) / 2; double prediction = 0;
    for (unsigned j = 0; j < order && j < frame; ++j) prediction += coefficients[order - 2][j] * double(std::bit_cast<float>(history.previous[j]));
    const auto bits = FloatBits(prediction);
    const auto result = mode & 1 ? value ^ bits : inverse ? Unzigzag(value) + bits : Zigzag(value - bits);
    for (unsigned j = 3; j; --j) history.previous[j] = history.previous[j - 1];
    history.previous[0] = inverse ? result : value;
    return result;
}
uint32_t Quaternion(View sample, unsigned target) {
    double f[3]; unsigned j = 0;
    for (unsigned c = 3; c < 7; ++c) if (c != target) f[j++] = Read<float>(sample, c * 4);
    const double sum = (f[0] * f[0] + f[1] * f[1]) + f[2] * f[2];
    return FloatBits(std::isfinite(sum) ? std::sqrt((std::max)(0.0, 1.0 - sum)) : 0);
}
uint32_t GetBits(View bytes, size_t pos, unsigned width) {
    uint64_t value = 0; const unsigned shift = unsigned(pos & 7), n = (shift + width + 7) / 8;
    for (unsigned j = 0; j < n; ++j) value |= uint64_t(bytes[pos / 8 + j]) << (8 * j);
    return uint32_t(value >> shift) & Mask(width);
}
void SetBits(Output bytes, size_t pos, unsigned width, uint32_t value) {
    const unsigned shift = unsigned(pos & 7), n = (shift + width + 7) / 8;
    const uint64_t mask = uint64_t(Mask(width)) << shift, bits = uint64_t(value) << shift;
    for (unsigned j = 0; j < n; ++j) bytes[pos / 8 + j] = uint8_t((bytes[pos / 8 + j] & ~uint8_t(mask >> (8 * j))) | uint8_t(bits >> (8 * j)));
}
class Score {
    std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)> context{nullptr, ZSTD_freeCCtx};
    Bytes scratch;
    std::unordered_map<std::string, size_t> cache;
    int level;
public:
    explicit Score(int scoreLevel) : level(scoreLevel) { if (level) { context.reset(ZSTD_createCCtx()); if (!context) throw std::bad_alloc(); } }
    size_t operator()(View bytes) {
        if (!level) {
            static const auto costs = [] {
                std::array<double, 4097> values{};
                for (size_t i = 1; i < values.size(); ++i) values[i] = double(i) * std::log2(double(i));
                return values;
            }();
            const auto cost = [&](size_t n) { return n < costs.size() ? costs[n] : double(n) * std::log2(double(n)); };
            std::array<uint32_t, 256> counts{};
            for (auto b : bytes) ++counts[b];
            double bits = cost(bytes.size());
            for (auto n : counts) bits -= cost(n);
            return size_t((std::max)(0.0, bits) * 16);
        }
        std::string key;
        if (bytes.size() <= 4096) { key.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size()); const auto it = cache.find(key); if (it != cache.end()) return it->second; }
        scratch.resize(ZSTD_compressBound(bytes.size()));
        const auto size = ZSTD_compressCCtx(context.get(), scratch.data(), scratch.size(), bytes.data(), bytes.size(), level);
        Check(!ZSTD_isError(size), "PANM predictor scoring failed");
        if (!key.empty() && cache.size() < 2000) cache.emplace(std::move(key), size);
        return size;
    }
};
struct Choice { size_t score = SIZE_MAX; uint8_t mode = 0; Bytes data; };
Choice Choose(std::span<const uint32_t> words, Score& score, uint16_t modes, uint32_t sampleFrames, bool needScore = true) {
    Choice best; Bytes lane(words.size() * 4);
    const size_t samples = sampleFrames ? (std::min)(words.size(), size_t(sampleFrames)) : words.size();
    auto fill = [&](unsigned mode, size_t n) {
        Predictor history;
        for (size_t f = 0; f < n; ++f) {
            const auto value = mode < 5 ? history.Integer(words[f], mode, 32, false) : Numerical(history, words[f], mode, f, false);
            for (unsigned b = 0; b < 4; ++b) lane[b * n + f] = uint8_t(value >> (8 * b));
        }
    };
    if (std::has_single_bit(modes) && !needScore) {
        best.mode = uint8_t(std::countr_zero(modes)); fill(best.mode, words.size()); best.data = std::move(lane); return best;
    }
    for (unsigned mode = 0; mode < 14; ++mode) if (modes & (1u << mode)) {
        fill(mode, samples);
        const auto cost = score(View(lane).first(samples * 4));
        if (cost < best.score) { best.score = cost; best.mode = uint8_t(mode); }
    }
    fill(best.mode, words.size()); best.data = std::move(lane);
    return best;
}
}

EncodeSettings FastSettings() {
    EncodeSettings settings;
    settings.rawModes = 4; // fixed second difference: no per-channel predictor search
    settings.correctionModes = 0x11;
    settings.packedModes = 4; // second difference; no native packed-track trial compression
    settings.quaternionChannels = 0;
    settings.scoreLevel = 0;
    settings.sampleFrames = 0;
    return settings;
}

Bytes Encode(View input) { return Encode(input, FastSettings()); }

Bytes Encode(View input, const EncodeSettings& settings) {
    FloatingPointScope floatingPoint;
    Check(settings.rawModes && !(settings.rawModes & ~0x3f1f) && settings.correctionModes && !(settings.correctionModes & ~0x1f) &&
        settings.packedModes && !(settings.packedModes & ~0x1f) && !(settings.quaternionChannels & ~0x78) && settings.scoreLevel >= 0 && settings.scoreLevel <= 9,
        "Invalid compact encoder settings");
    const auto tracks = Tracks(input); Score score(settings.scoreLevel);
    {
        // Decode rediscovers tracks after zeroing every emitted range. A descriptor rejected here
        // because of bytes inside a later track's samples (e.g. an overlapping packed header) can be
        // accepted there, so refuse any layout whose discovery depends on those bytes. The caller
        // then stores this PANM byte-for-byte.
        Bytes zeroed(input.begin(), input.end());
        for (const auto& t : tracks) std::memset(zeroed.data() + t.at, 0, t.size);
        const auto rediscovered = Tracks(zeroed);
        Check(std::equal(tracks.begin(), tracks.end(), rediscovered.begin(), rediscovered.end(), SameTrack),
            "PANM track layout depends on sample bytes");
    }
    Bytes metadata, models; std::vector<Bytes> raw, packed; size_t cursor = 0, rawSize = 0, packedSize = 0;
    for (const auto& t : tracks) {
        metadata.insert(metadata.end(), input.begin() + cursor, input.begin() + t.at); cursor = t.at + t.size;
        const auto src = input.subspan(t.at, t.size);
        if (t.raw) {
            std::array<std::vector<uint32_t>, 11> channels; std::array<bool, 11> constant{}; std::array<Choice, 11> choices;
            for (unsigned c = 0; c < 11; ++c) {
                auto& lane = channels[c]; lane.resize(t.frames);
                for (size_t f = 0; f < t.frames; ++f) lane[f] = Read<uint32_t>(src, f * 44 + c * 4);
                constant[c] = std::all_of(lane.begin(), lane.end(), [&](uint32_t v) { return v == lane.front(); });
                if (!constant[c]) choices[c] = Choose(lane, score, settings.rawModes, settings.sampleFrames, (settings.quaternionChannels & (1u << c)) != 0);
            }
            int64_t bestGain = 0; unsigned target = 0; Choice correction;
            for (unsigned c = 3; c < 7; ++c) if (!constant[c] && (settings.quaternionChannels & (1u << c))) {
                auto lane = channels[c];
                for (size_t f = 0; f < t.frames; ++f) lane[f] ^= Quaternion(src.subspan(f * 44, 44), c);
                auto choice = Choose(lane, score, settings.correctionModes, settings.sampleFrames); const int64_t gain = int64_t(choice.score) - int64_t(choices[c].score);
                if (gain < bestGain) { bestGain = gain; target = c; correction = std::move(choice); }
            }
            if (bestGain < 0) { correction.mode |= 16; choices[target] = std::move(correction); }
            for (unsigned c = 0; c < 11; ++c) {
                if (constant[c]) { models.push_back(128); Append(models, channels[c].front()); }
                else { models.push_back(choices[c].mode); rawSize += choices[c].data.size(); raw.push_back(std::move(choices[c].data)); }
            }
        } else {
            const unsigned tailBits = unsigned((size_t(t.frames) * t.bitsPerFrame) & 7);
            models.push_back(tailBits ? uint8_t(src.back() & uint8_t(255u << tailBits)) : 0);
            unsigned bestMode = std::countr_zero(settings.packedModes); size_t bestScore = SIZE_MAX;
            const size_t samples = settings.sampleFrames ? (std::min)(size_t(t.frames), size_t(settings.sampleFrames)) : t.frames;
            for (unsigned mode = 0; mode < 5; ++mode) if (!std::has_single_bit(settings.packedModes) && (settings.packedModes & (1u << mode))) {
                Bytes combined(src.begin(), src.begin() + (samples * t.bitsPerFrame + 7) / 8); unsigned shift = 0;
                for (unsigned c = 0; c < t.channels; ++c) if (const unsigned width = t.widths[c]) {
                    Predictor history;
                    for (size_t f = 0; f < samples; ++f) {
                        const auto value = history.Integer(GetBits(src, f * t.bitsPerFrame + shift, width), mode, width, false);
                        for (unsigned bit = 0; bit < width; ++bit) {
                            const size_t pos = size_t(shift + bit) * samples + f; const auto mask = uint8_t(1u << (pos & 7));
                            combined[pos / 8] = uint8_t((combined[pos / 8] & ~mask) | (((value >> bit) & 1) << (pos & 7)));
                        }
                    }
                    shift += width;
                }
                const auto cost = score(combined); if (cost < bestScore) { bestScore = cost; bestMode = mode; }
            }
            unsigned shift = 0;
            for (unsigned c = 0; c < t.channels; ++c) if (const unsigned width = t.widths[c]) {
                Bytes lane((size_t(t.frames) * width + 7) / 8); Predictor history;
                for (size_t f = 0; f < t.frames; ++f) {
                    const auto value = history.Integer(GetBits(src, f * t.bitsPerFrame + shift, width), bestMode, width, false);
                    for (unsigned bit = 0; bit < width; ++bit) { const size_t pos = size_t(bit) * t.frames + f; lane[pos / 8] |= uint8_t(((value >> bit) & 1) << (pos & 7)); }
                }
                models.push_back(uint8_t(bestMode)); packedSize += lane.size(); packed.push_back(std::move(lane)); shift += width;
            }
        }
    }
    metadata.insert(metadata.end(), input.begin() + cursor, input.end());
    Bytes output(24 + tracks.size() * 8); std::memcpy(output.data(), "PCF1", 4); output[4] = 1;
    Write<uint32_t>(output, 8, uint32_t(input.size())); Write<uint32_t>(output, 12, uint32_t(tracks.size()));
    Write<uint32_t>(output, 16, uint32_t(metadata.size())); Write<uint32_t>(output, 20, uint32_t(models.size()));
    for (size_t i = 0; i < tracks.size(); ++i) { Write<uint32_t>(output, 24 + i * 8, uint32_t(tracks[i].at)); Write<uint32_t>(output, 28 + i * 8, uint32_t(tracks[i].size)); }
    output.reserve(output.size() + metadata.size() + models.size() + rawSize + packedSize);
    output.insert(output.end(), metadata.begin(), metadata.end()); output.insert(output.end(), models.begin(), models.end());
    for (unsigned b = 0; b < 4; ++b) for (const auto& lane : raw) { const auto n = lane.size() / 4; output.insert(output.end(), lane.begin() + b * n, lane.begin() + (b + 1) * n); }
    for (const auto& lane : packed) output.insert(output.end(), lane.begin(), lane.end());
    return output;
}

void Decode(View input, Output output) {
    FloatingPointScope floatingPoint;
    Check(input.size() >= 24 && !std::memcmp(input.data(), "PCF1", 4) && input[4] == 1 && !input[5] && !input[6] && !input[7], "Invalid compact PANM version/flags");
    const size_t original = Read<uint32_t>(input, 8), count = Read<uint32_t>(input, 12), metaSize = Read<uint32_t>(input, 16), modelSize = Read<uint32_t>(input, 20);
    Check(original >= 8 && original <= MaximumSize && original == output.size() && count <= MaximumTracks && count * 8 <= input.size() - 24, "Invalid compact PANM budget");
    const size_t metaAt = 24 + count * 8;
    Check(metaSize <= input.size() - metaAt && modelSize <= input.size() - metaAt - metaSize, "Invalid compact PANM sections");
    const auto models = input.subspan(metaAt + metaSize, modelSize), payload = input.subspan(metaAt + metaSize + modelSize);
    size_t cursor = 0, mp = metaAt;
    for (size_t i = 0; i < count; ++i) {
        const size_t at = Read<uint32_t>(input, 24 + i * 8), size = Read<uint32_t>(input, 28 + i * 8);
        Check(size && at >= cursor && at <= original && size <= original - at && at - cursor <= metaAt + metaSize - mp, "Invalid compact PANM range");
        std::memcpy(output.data() + cursor, input.data() + mp, at - cursor); mp += at - cursor;
        std::memset(output.data() + at, 0, size); cursor = at + size;
    }
    Check(original - cursor == metaAt + metaSize - mp, "Invalid compact PANM metadata length");
    std::memcpy(output.data() + cursor, input.data() + mp, original - cursor);
    const auto tracks = Tracks(output); Check(tracks.size() == count, "Compact PANM schema/range disagreement");
    struct Lane { uint8_t mode; uint32_t constant = 0; size_t offset = 0; unsigned width = 0; };
    struct Entry { std::vector<Lane> lanes; uint8_t tail = 0; };
    std::vector<Entry> entries(count); size_t modelAt = 0, rawTotal = 0, packedTotal = 0;
    for (size_t i = 0; i < count; ++i) {
        const auto& t = tracks[i]; Check(t.at == Read<uint32_t>(input, 24 + i * 8) && t.size == Read<uint32_t>(input, 28 + i * 8), "Compact PANM schema/range disagreement");
        auto& e = entries[i]; unsigned qcount = 0;
        if (t.raw) for (unsigned c = 0; c < 11; ++c) {
            Lane lane{Read<uint8_t>(models, modelAt++)};
            if (lane.mode == 128) { lane.constant = Read<uint32_t>(models, modelAt); modelAt += 4; }
            else {
                const unsigned mode = lane.mode & 15;
                Check(lane.mode < 32 && (mode <= 4 || (mode >= 8 && mode <= 13)), "Invalid compact PANM raw predictor");
                if (lane.mode & 16) { Check(c >= 3 && c <= 6 && mode <= 4, "Invalid quaternion predictor"); ++qcount; }
                lane.offset = rawTotal; rawTotal += t.frames;
            }
            e.lanes.push_back(lane);
        } else {
            e.tail = Read<uint8_t>(models, modelAt++); const unsigned tailBits = unsigned((size_t(t.frames) * t.bitsPerFrame) & 7);
            Check(tailBits ? !(e.tail & ((1u << tailBits) - 1)) : e.tail == 0, "Invalid compact PANM tail bits");
            for (unsigned c = 0; c < t.channels; ++c) if (t.widths[c]) {
                Lane lane{Read<uint8_t>(models, modelAt++)}; Check(lane.mode <= 4, "Invalid compact PANM packed predictor");
                lane.width = t.widths[c]; lane.offset = packedTotal; packedTotal += (size_t(t.frames) * lane.width + 7) / 8; e.lanes.push_back(lane);
            }
        }
        Check(qcount <= 1, "Cyclic quaternion prediction");
    }
    Check(modelAt == models.size() && rawTotal * 4 + packedTotal == payload.size(), "Invalid compact PANM payload length");
    for (size_t i = 0; i < count; ++i) {
        const auto& t = tracks[i]; const auto& e = entries[i];
        if (t.raw) {
            int target = -1;
            for (unsigned c = 0; c < 11; ++c) {
                const auto& lane = e.lanes[c]; const unsigned mode = lane.mode & 15; Predictor history;
                if (lane.mode != 128 && (lane.mode & 16)) target = int(c);
                for (size_t f = 0; f < t.frames; ++f) {
                    uint32_t value = lane.constant;
                    if (lane.mode != 128) {
                        value = 0; for (unsigned b = 0; b < 4; ++b) value |= uint32_t(payload[b * rawTotal + lane.offset + f]) << (8 * b);
                        value = mode <= 4 ? history.Integer(value, mode, 32, true) : Numerical(history, value, mode, f, true);
                    }
                    // The track extent and all channel/sample indices were checked above.
                    std::memcpy(output.data() + t.at + f * 44 + c * 4, &value, 4);
                }
            }
            if (target >= 0) for (size_t f = 0; f < t.frames; ++f) {
                const size_t at = t.at + f * 44 + size_t(target) * 4;
                Write<uint32_t>(output, at, Read<uint32_t>(output, at) ^ Quaternion(output.subspan(t.at + f * 44, 44), unsigned(target)));
            }
        } else {
            unsigned shift = 0;
            for (const auto& lane : e.lanes) {
                Predictor history; const auto offset = rawTotal * 4 + lane.offset;
                const size_t bits = size_t(t.frames) * lane.width;
                if (bits & 7) Check(!(payload[offset + bits / 8] & uint8_t(255u << (bits & 7))), "Nonzero compact PANM lane padding");
                for (size_t f = 0; f < t.frames; ++f) {
                    uint32_t value = 0;
                    for (unsigned bit = 0; bit < lane.width; ++bit) { const size_t pos = size_t(bit) * t.frames + f; value |= uint32_t((payload[offset + pos / 8] >> (pos & 7)) & 1) << bit; }
                    value = history.Integer(value, lane.mode, lane.width, true);
                    SetBits(output, t.at * 8 + f * t.bitsPerFrame + shift, lane.width, value);
                }
                shift += lane.width;
            }
            output[t.at + t.size - 1] |= e.tail;
        }
    }
}
}
