#include "CompactZstd.hpp"
#include "CompactPanm.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>
#include <zlib.h>

namespace CompactZstd {
namespace {
using Bytes = std::vector<uint8_t>;
using View = std::span<const uint8_t>;
using Output = std::span<uint8_t>;
constexpr size_t FrameHeaderSize = 40, ArchiveHeaderSize = 16, MaximumRecords = 200001;
constexpr size_t MaximumStream = MaximumPayload * 2 + MaximumRecords * 33 + ArchiveHeaderSize;
void Check(bool ok, const char* message) { if (!ok) throw std::invalid_argument(message); }
template<class T> T Read(View bytes, size_t at) {
    Check(at <= bytes.size() && sizeof(T) <= bytes.size() - at, "Truncated compact Zstd frame");
    T value; std::memcpy(&value, bytes.data() + at, sizeof(value)); return value;
}
template<class T> void Write(Output bytes, size_t at, T value) { std::memcpy(bytes.data() + at, &value, sizeof(value)); }
uint32_t Crc(View bytes) { return uint32_t(crc32(0, bytes.data(), static_cast<uInt>(bytes.size()))); }
size_t Zcheck(size_t status) { Check(!ZSTD_isError(status), "Invalid compact Zstd stream"); return status; }
size_t Align(size_t value) { return (value + 7) & ~size_t(7); }
struct Range { size_t at, size; };

// Walk the versioned T8RS table directory; never scan opaque data for magic strings.
// This is only a bounds/layout recognizer, not a replacement for game semantic validation.
std::vector<Range> AnimationRanges(View bytes) {
    if (bytes.size() < 40 || std::memcmp(bytes.data(), "T8RS", 4)) return {};
    const auto version = Read<uint32_t>(bytes, 4);
    if (version < 2 || version > 4) return {};
    Check(Read<uint64_t>(bytes, 16) == 40 && Read<uint64_t>(bytes, 24) == 832 && Read<uint64_t>(bytes, 32) == bytes.size(), "Invalid T8RS header");
    constexpr std::array<uint32_t, 20> strides = {0x70,0x14,0x18,0xe0,0x10,2,0x28,0x28,4,0x28,0x20,0x20,0x448,0xc,0x10,8,4,0xc,0x10,0x18};
    size_t cursor = 832 + strides.size() * 32;
    for (size_t i = 0; i < strides.size(); ++i) {
        const size_t entry = 832 + i * 32;
        const auto count = Read<uint64_t>(bytes, entry + 8), at = Read<uint64_t>(bytes, entry + 16);
        Check(Read<uint32_t>(bytes, entry + 24) == strides[i] && !Read<uint32_t>(bytes, entry + 28), "Invalid T8RS table stride");
        cursor = Align(cursor);
        Check(at == cursor && cursor <= bytes.size() && count <= (bytes.size() - cursor) / strides[i], "Invalid T8RS table range");
        cursor += size_t(count) * strides[i];
    }
    cursor = Align(cursor);
    Check(cursor <= bytes.size() && bytes.size() - cursor >= 32 && !std::memcmp(bytes.data() + cursor, "T8AN", 4), "Missing T8RS animation directory");
    const auto count = Read<uint32_t>(bytes, cursor + 4);
    const auto entries = Read<uint64_t>(bytes, cursor + 8), data = Read<uint64_t>(bytes, cursor + 16);
    Check(count <= 100000 && !Read<uint64_t>(bytes, cursor + 24) && entries == cursor + 32 &&
        data == Align(size_t(entries) + size_t(count) * 32) && data <= bytes.size(), "Invalid T8RS animation directory");
    cursor = size_t(data); std::vector<Range> ranges;
    for (size_t i = 0; i < count; ++i) {
        const size_t entry = size_t(entries) + i * 32;
        const auto at = Read<uint64_t>(bytes, entry + 8), size = Read<uint64_t>(bytes, entry + 16);
        cursor = Align(cursor);
        Check(at == cursor && cursor <= bytes.size() && size <= bytes.size() - cursor && size <= CompactPanm::MaximumSize &&
            Read<uint32_t>(bytes, entry + 28) < 6, "Invalid T8RS animation range");
        if (!size) continue;
        Check(size >= 8 && !std::memcmp(bytes.data() + cursor + 4, "PANM", 4), "Invalid T8RS animation signature");
        ranges.push_back({cursor, size_t(size)}); cursor += size_t(size);
    }
    return ranges;
}

void Record(Bytes& stream, uint8_t mode, uint32_t original, View data) {
    if (!original) return;
    Check(data.size() <= size_t(original) + CompactPanm::MaximumOverhead && data.size() <= UINT32_MAX &&
        stream.size() <= MaximumStream && data.size() + 9 <= MaximumStream - stream.size(), "Compact stream exceeds budget");
    const auto at = stream.size(); stream.resize(at + 9); stream[at] = mode;
    Write<uint32_t>(stream, at + 1, original); Write<uint32_t>(stream, at + 5, uint32_t(data.size()));
    stream.insert(stream.end(), data.begin(), data.end());
}

class StreamReader {
    std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)> context{ZSTD_createDCtx(), ZSTD_freeDCtx};
    ZSTD_inBuffer input;
    size_t expected, total = 0;
    bool done = false;
public:
    StreamReader(View compressed, size_t length) : input{compressed.data(), compressed.size(), 0}, expected(length) {
        if (!context) throw std::bad_alloc();
        Zcheck(ZSTD_DCtx_setParameter(context.get(), ZSTD_d_windowLogMax, 25));
    }
    void ReadBytes(Output output) {
        Check(output.size() <= expected - total, "Compact stream output exceeds declared size");
        ZSTD_outBuffer out{output.data(), output.size(), 0};
        while (out.pos < out.size) {
            Check(!done, "Truncated compact records");
            const auto oldInput = input.pos, oldOutput = out.pos;
            const auto left = Zcheck(ZSTD_decompressStream(context.get(), &out, &input));
            done = left == 0;
            Check(input.pos != oldInput || out.pos != oldOutput || done, "Truncated compact Zstd stream");
            if (done) Check(input.pos == input.size && out.pos == out.size, "Trailing or short compact Zstd frame");
        }
        total += output.size();
    }
    void Finish() {
        Check(total == expected, "Trailing compact records");
        uint8_t byte; ZSTD_outBuffer out{&byte, 1, 0};
        while (!done) {
            const auto old = input.pos; const auto left = Zcheck(ZSTD_decompressStream(context.get(), &out, &input));
            Check(!out.pos, "Trailing compact stream bytes"); done = left == 0;
            Check(input.pos != old || done, "Truncated compact Zstd end");
        }
        Check(input.pos == input.size, "Trailing compact Zstd frame");
    }
};
}

std::vector<uint8_t> Compress(View payload) {
    EncodeSettings settings;
    settings.panm = CompactPanm::FastSettings();
    settings.level = 3;
    return Compress(payload, settings);
}

std::vector<uint8_t> Compress(View payload, const EncodeSettings& settings) {
    Check(!payload.empty() && payload.size() <= MaximumPayload, "Invalid compact payload size");
    std::vector<Range> ranges;
    try { if (settings.transform) ranges = AnimationRanges(payload); }
    catch (const std::invalid_argument&) { /* Unsupported/malformed content is carried opaquely; semantic validation is separate. */ }
    Bytes stream(ArchiveHeaderSize); std::memcpy(stream.data(), "T8AS", 4);
    Write<uint32_t>(stream, 4, 1); Write<uint64_t>(stream, 8, payload.size());
    size_t cursor = 0;
    for (const auto& range : ranges) {
        Record(stream, 0, uint32_t(range.at - cursor), payload.subspan(cursor, range.at - cursor));
        const auto panm = payload.subspan(range.at, range.size); Bytes compact;
        try { compact = CompactPanm::Encode(panm, settings.panm); }
        catch (const std::invalid_argument&) { /* Preserve an unrecognized PANM byte-for-byte. */ }
        // Predictor/range bytes can enlarge the intermediate stream while greatly reducing its
        // compressed size, especially for native packed tracks. Do not compare uncompressed sizes.
        if (!compact.empty() && Read<uint32_t>(compact, 12) != 0) Record(stream, 1, uint32_t(panm.size()), compact);
        else Record(stream, 0, uint32_t(panm.size()), panm);
        cursor = range.at + range.size;
    }
    Record(stream, 0, uint32_t(payload.size() - cursor), payload.subspan(cursor));
    std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)> context(ZSTD_createCCtx(), ZSTD_freeCCtx);
    if (!context) throw std::bad_alloc();
    Zcheck(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_compressionLevel, settings.level));
    Zcheck(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_checksumFlag, 1));
    // Bounded window, also documented as part of version 1's decoder budget.
    Zcheck(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_windowLog, 23));
    Bytes output(FrameHeaderSize + ZSTD_compressBound(stream.size()));
    const auto compressed = Zcheck(ZSTD_compress2(context.get(), output.data() + FrameHeaderSize, output.size() - FrameHeaderSize, stream.data(), stream.size()));
    output.resize(FrameHeaderSize + compressed); std::memcpy(output.data(), "TCZ1", 4);
    Write<uint32_t>(output, 4, FrameHeaderSize); Write<uint64_t>(output, 8, payload.size());
    Write<uint64_t>(output, 16, stream.size()); Write<uint64_t>(output, 24, compressed);
    Write<uint32_t>(output, 32, Crc(payload)); Write<uint32_t>(output, 36, Crc(View(output).first(36)));
    return output;
}

bool Decompress(View compressed, Output output) noexcept {
    try {
        Check(!output.empty() && output.size() <= MaximumPayload && compressed.size() >= FrameHeaderSize &&
            !std::memcmp(compressed.data(), "TCZ1", 4) && Read<uint32_t>(compressed, 4) == FrameHeaderSize, "Invalid compact Zstd header");
        Check(Read<uint32_t>(compressed, 36) == Crc(compressed.first(36)), "Compact header checksum mismatch");
        const auto original = Read<uint64_t>(compressed, 8), filtered = Read<uint64_t>(compressed, 16), stored = Read<uint64_t>(compressed, 24);
        Check(original == output.size() && filtered >= ArchiveHeaderSize + 9 && filtered <= original * 2 + MaximumRecords * 33 + ArchiveHeaderSize &&
            filtered <= MaximumStream && stored == compressed.size() - FrameHeaderSize, "Invalid compact frame extent");
        const auto frame = compressed.subspan(FrameHeaderSize);
        ZSTD_frameHeader header{};
        Check(Zcheck(ZSTD_getFrameHeader(&header, frame.data(), frame.size())) == 0 && header.frameType == ZSTD_frame &&
            header.checksumFlag && !header.dictID && header.frameContentSize == filtered && header.windowSize <= (1u << 25), "Invalid compact Zstd parameters");
        Check(Zcheck(ZSTD_findFrameCompressedSize(frame.data(), frame.size())) == frame.size(), "Trailing compact frames");
        StreamReader reader(frame, size_t(filtered)); std::array<uint8_t, ArchiveHeaderSize> archive{}; reader.ReadBytes(archive);
        Check(!std::memcmp(archive.data(), "T8AS", 4) && Read<uint32_t>(archive, 4) == 1 && Read<uint64_t>(archive, 8) == original, "Invalid compact archive version");
        size_t cursor = 0, records = 0;
        while (cursor < output.size()) {
            Check(++records <= MaximumRecords, "Compact record budget exceeded");
            std::array<uint8_t, 9> record{}; reader.ReadBytes(record);
            const size_t size = Read<uint32_t>(record, 1), packed = Read<uint32_t>(record, 5);
            Check(size && size <= output.size() - cursor && packed <= size + CompactPanm::MaximumOverhead, "Invalid compact record extent");
            auto destination = output.subspan(cursor, size);
            if (record[0] == 0) { Check(size == packed, "Invalid opaque compact record"); reader.ReadBytes(destination); }
            else {
                Check(record[0] == 1 && size <= CompactPanm::MaximumSize && packed >= 24, "Invalid compact PANM record");
                Bytes bytes(packed); reader.ReadBytes(bytes); CompactPanm::Decode(bytes, destination);
            }
            cursor += size;
        }
        reader.Finish(); Check(Crc(output) == Read<uint32_t>(compressed, 32), "Compact reconstruction checksum mismatch");
        return true;
    } catch (const std::exception&) { return false; }
}
}
