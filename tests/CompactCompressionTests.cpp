#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>
#include <nlohmann/json.hpp>
#include <xmmintrin.h>
#include <zstd.h>
#include <zlib.h>
#include "CompactPanm.hpp"
#include "CompactZstd.hpp"
#include "Compression.hpp"

namespace {
using Bytes = std::vector<uint8_t>;
using View = std::span<const uint8_t>;
using Json = nlohmann::json;
void Check(bool v, const char* s) { if (!v) throw std::runtime_error(s); }
template<class T> void Put(Bytes& b, size_t at, T value) { Check(at + sizeof(T) <= b.size(), "fixture write overflow"); std::memcpy(b.data() + at, &value, sizeof(value)); }
template<class T> T Get(View b, size_t at) { Check(at + sizeof(T) <= b.size(), "fixture read overflow"); T v; std::memcpy(&v, b.data() + at, sizeof(v)); return v; }
Bytes Load(const std::filesystem::path& p) { std::ifstream f(p, std::ios::binary); Check(bool(f), "cannot read fixture"); return Bytes(std::istreambuf_iterator<char>(f), {}); }
void Save(const std::filesystem::path& p, View b) { Check(!std::filesystem::exists(p), "test output exists"); std::ofstream f(p, std::ios::binary); Check(bool(f.write(reinterpret_cast<const char*>(b.data()), b.size())), "cannot write test output"); }
size_t Table(Bytes& b, unsigned fields) {
    const auto vt = b.size(); b.resize(vt + 4 + fields * 2);
    Put<uint16_t>(b, vt, uint16_t(4 + fields * 2)); Put<uint16_t>(b, vt + 2, uint16_t(4 + fields * 4));
    for (unsigned i = 0; i < fields; ++i) Put<uint16_t>(b, vt + 4 + i * 2, uint16_t(4 + i * 4));
    b.resize((b.size() + 3) & ~size_t(3)); const auto at = b.size(); b.resize(at + 4 + fields * 4); Put<int32_t>(b, at, int32_t(at - vt)); return at;
}
size_t Vector(Bytes& b, unsigned n) { const auto at = b.size(); b.resize(at + 4 + n * 4); Put<uint32_t>(b, at, n); return at; }
void Ref(Bytes& b, size_t field, size_t target) { Check(target > field, "backwards fixture reference"); Put<uint32_t>(b, field, uint32_t(target - field)); }
Bytes PanmFixture(bool overlapping = false) {
    Bytes b(8); std::memcpy(b.data() + 4, "PANM", 4);
    auto root = Table(b, 4); Put<uint32_t>(b, 0, uint32_t(root)); auto anim = Table(b, 6); Ref(b, root + 16, anim);
    auto groups = Vector(b, 1); Ref(b, anim + 20, groups); auto group = Table(b, 3); Ref(b, groups + 4, group);
    auto bones = Vector(b, 1); Ref(b, group + 12, bones); auto bone = Table(b, 3); Ref(b, bones + 4, bone);
    auto tracks = Vector(b, 2); Ref(b, bone + 12, tracks); auto raw = Table(b, 8); Ref(b, tracks + 4, raw); auto packed = Table(b, 8); Ref(b, tracks + 8, packed);
    constexpr unsigned frames = 17, rawSize = frames * 44, packedHeader = 60 + 3 * 16, packedBits = 38;
    constexpr unsigned packedSize = packedHeader + (frames * packedBits + 7) / 8 + 3;
    const auto base = b.size(); Put<uint32_t>(b, anim + 24, uint32_t(base)); b.resize(base + rawSize + packedSize + 7, 0xa7);
    for (auto at : {raw, packed}) { Put<uint32_t>(b, at + 12, 1); Put<uint32_t>(b, at + 20, frames); }
    Put<uint32_t>(b, raw + 16, 1); Put<uint32_t>(b, raw + 28, 0); Put<uint32_t>(b, raw + 32, rawSize);
    Put<uint32_t>(b, packed + 16, 3); Put<uint32_t>(b, packed + 28, rawSize); Put<uint32_t>(b, packed + 32, packedSize);
    std::mt19937 random(1701);
    for (size_t f = 0; f < frames; ++f) for (unsigned c = 0; c < 11; ++c) {
        const uint32_t v = c < 3 ? 0x3f800000u : c == 10 ? 0x80000000u : c == 6 ? 0x3f800000u : std::bit_cast<uint32_t>(float(f * f * (c + 1)) * 0.00001f);
        Put<uint32_t>(b, base + f * 44 + c * 4, v);
    }
    const size_t p = base + rawSize; Put<uint16_t>(b, p, 4); Put<uint16_t>(b, p + 6, packedBits); Put<uint32_t>(b, p + 8, packedHeader);
    for (unsigned c = 0; c < 3; ++c) { Put<float>(b, p + 16 + c * 16, c == 1 ? 1.f : 0.f); Put<float>(b, p + 20 + c * 16, 1.f); Put<uint32_t>(b, p + 24 + c * 16, c == 0 ? 5 : c == 1 ? 8 : 32); }
    for (size_t at = p + packedHeader; at < b.size(); ++at) b[at] = uint8_t(random());
    if (overlapping) {
        // Packed descriptor D at data offset 0 (1 frame, 76-byte header, channel width 33: rejected)
        // overlapped by the raw descriptor E at offset 12 (accepted). Once E's samples are zeroed,
        // D's channel table reads 0/0/0 and the implicit sign bit makes D acceptable, hiding E.
        Put<uint32_t>(b, raw + 28, 12);
        Put<uint32_t>(b, packed + 20, 1); Put<uint32_t>(b, packed + 28, 0); Put<uint32_t>(b, packed + 32, 80);
        Put<uint16_t>(b, base, 4); Put<uint16_t>(b, base + 6, 1); Put<uint32_t>(b, base + 8, 76);
        Put<float>(b, base + 16, 0.f); Put<float>(b, base + 20, 1.f); Put<uint32_t>(b, base + 24, 33);
    }
    return b;
}
Bytes MovesetFixture(View payload) {
    TKMovesetHeader h; h.header_size = sizeof(h); h.block_list = sizeof(h); h.block_list_size = 1; h.moveset_data_start = sizeof(h) + 8;
    Bytes b(h.moveset_data_start + payload.size()); std::memcpy(b.data(), &h, sizeof(h)); std::copy(payload.begin(), payload.end(), b.begin() + h.moveset_data_start); return b;
}
Bytes SnapshotFixture(View panm, unsigned version) {
    constexpr std::array<uint32_t, 20> strides = {0x70,0x14,0x18,0xe0,0x10,2,0x28,0x28,4,0x28,0x20,0x20,0x448,0xc,0x10,8,4,0xc,0x10,0x18};
    // Empty structural tables, two PANMs, an empty marker, and opaque padding/extension bytes.
    const size_t directory = 1472, entries = directory + 32, data = entries + 3 * 32;
    const size_t second = (data + panm.size() + 7) & ~size_t(7), end = (second + panm.size() + 7) & ~size_t(7);
    Bytes b(end + 23, 0); std::memcpy(b.data(), "T8RS", 4); Put<uint32_t>(b, 4, version);
    Put<uint64_t>(b, 16, 40); Put<uint64_t>(b, 24, 832); Put<uint64_t>(b, 32, b.size());
    for (size_t i = 0; i < strides.size(); ++i) { Put<uint64_t>(b, 832 + i * 32 + 16, directory); Put<uint32_t>(b, 832 + i * 32 + 24, strides[i]); }
    std::memcpy(b.data() + directory, "T8AN", 4); Put<uint32_t>(b, directory + 4, 3);
    Put<uint64_t>(b, directory + 8, entries); Put<uint64_t>(b, directory + 16, data);
    Put<uint64_t>(b, entries + 8, data); Put<uint64_t>(b, entries + 16, panm.size());
    Put<uint64_t>(b, entries + 40, second); Put<uint64_t>(b, entries + 48, panm.size());
    Put<uint64_t>(b, entries + 72, end);
    std::copy(panm.begin(), panm.end(), b.begin() + data); std::copy(panm.begin(), panm.end(), b.begin() + second);
    for (size_t i = end; i < b.size(); ++i) b[i] = uint8_t(i); return b;
}
void Roundtrip(View original) {
    const auto encoded = CompactPanm::Encode(original); Bytes decoded(original.size()); CompactPanm::Decode(encoded, decoded);
    Check(std::equal(decoded.begin(), decoded.end(), original.begin(), original.end()), "PANM roundtrip mismatch");
}
Bytes Frame(View archive, View original) {
    Bytes b(40 + ZSTD_compressBound(archive.size()));
    auto ctx = ZSTD_createCCtx(); Check(ctx != nullptr, "cannot allocate test compressor");
    ZSTD_CCtx_setParameter(ctx, ZSTD_c_checksumFlag, 1);
    auto n = ZSTD_compress2(ctx, b.data() + 40, b.size() - 40, archive.data(), archive.size()); ZSTD_freeCCtx(ctx); Check(!ZSTD_isError(n), "cannot frame test input"); b.resize(40 + n);
    std::memcpy(b.data(), "TCZ1", 4); Put<uint32_t>(b, 4, 40); Put<uint64_t>(b, 8, original.size()); Put<uint64_t>(b, 16, archive.size()); Put<uint64_t>(b, 24, n);
    Put<uint32_t>(b, 32, uint32_t(crc32(0, original.data(), uInt(original.size())))); Put<uint32_t>(b, 36, uint32_t(crc32(0, b.data(), 36))); return b;
}
Bytes Archive(View panm, View compact) {
    Bytes b(25); std::memcpy(b.data(), "T8AS", 4); Put<uint32_t>(b, 4, 1); Put<uint64_t>(b, 8, panm.size()); b[16] = 1;
    Put<uint32_t>(b, 17, uint32_t(panm.size())); Put<uint32_t>(b, 21, uint32_t(compact.size())); b.insert(b.end(), compact.begin(), compact.end()); return b;
}
void Tests() {
    const auto panm = PanmFixture(); Roundtrip(panm);
    const auto encoded = CompactPanm::Encode(panm); Check(encoded.size() < panm.size(), "fixture did not compact");
    // Encoding/decoding must neither depend on nor change the caller's rounding/FTZ/DAZ state.
    const auto normal = _mm_getcsr();
    for (unsigned rounding : {0u, 0x2000u, 0x4000u, 0x6000u}) {
        const auto changed = (normal & ~0x6000u) | rounding | 0x8040u; _mm_setcsr(changed);
        Check(CompactPanm::Encode(panm) == encoded, "caller FP mode changed encoded bits"); Roundtrip(panm);
        Check(_mm_getcsr() == changed, "codec leaked FP environment changes");
    }
    _mm_setcsr(normal);
    auto badFloats = panm;
    const auto base = Get<uint32_t>(panm, Get<uint32_t>(panm, 0) + 16) + Get<uint32_t>(panm, 0) + 16;
    const auto data = Get<uint32_t>(panm, base + 24);
    const std::array<uint32_t, 11> patterns = {0,0x80000000,0x7f800000,0xff800000,0x7fc01234,0x7f800001,1,0x7f7fffff,0x80000001,0xff7fffff,0x3f800000};
    for (size_t f = 0; f < 17; ++f) for (unsigned c = 0; c < 11; ++c) Put<uint32_t>(badFloats, data + f * 44 + c * 4, patterns[(f + c) % patterns.size()]);
    Roundtrip(badFloats);
    // A layout whose track discovery changes once emitted ranges are zeroed would be rejected by the
    // decoder: the encoder must refuse it, and the archive then carries the PANM opaquely.
    {
        const auto overlapping = PanmFixture(true); bool refused = false;
        try { CompactPanm::Encode(overlapping); } catch (const std::invalid_argument&) { refused = true; }
        Check(refused, "PANM with sample-dependent track discovery was compacted");
        try { CompactPanm::Encode(overlapping, CompactPanm::EncodeSettings{}); refused = false; } catch (const std::invalid_argument&) { refused = true; }
        Check(refused, "PANM with sample-dependent track discovery was compacted (reference settings)");
        const auto snapshot = SnapshotFixture(overlapping, 4), compact = CompactZstd::Compress(snapshot); Bytes restored(snapshot.size());
        Check(CompactZstd::Decompress(compact, restored) && restored == snapshot, "overlapping PANM archive roundtrip failed");
    }
    for (unsigned version : {2u, 3u, 4u}) {
        auto snapshot = SnapshotFixture(panm, version), compact = CompactZstd::Compress(snapshot); Bytes restored(snapshot.size());
        Check(CompactZstd::Decompress(compact, restored) && restored == snapshot, "T8RS structural transform roundtrip failed");
        Bytes stream(Get<uint64_t>(compact, 16));
        Check(ZSTD_decompress(stream.data(), stream.size(), compact.data() + 40, compact.size() - 40) == stream.size(), "test archive expand failed");
        size_t count = 0;
        for (size_t at = 16; at < stream.size(); at += 9 + Get<uint32_t>(stream, at + 5)) if (stream[at] == 1) ++count;
        Check(count == 2, "T8RS animation directory was not transformed");
        // Bad/unknown layouts must retain the whole original payload opaquely.
        Put<uint64_t>(snapshot, 832 + 16, UINT64_MAX); compact = CompactZstd::Compress(snapshot);
        Check(CompactZstd::Decompress(compact, restored) && restored == snapshot, "opaque fallback changed unsupported snapshot");
    }
    auto file = MovesetFixture(panm);
    for (auto codec : {TKMovesetCompressionType_None, TKMovesetCompressionType_LZ4, TKMovesetCompressionType_LZMA, TKMovesetCompressionType_CompactZstd}) {
        uint64_t size = 0; std::unique_ptr<Byte[]> compressed(CompressionUtils::RAW::Moveset::Compress(file.data(), file.size(), codec, size)); Check(bool(compressed), "file codec encode failed");
        auto copy = static_cast<Byte*>(malloc(size)); std::memcpy(copy, compressed.get(), size);
        Check(CompressionUtils::RAW::Moveset::ExpandFileBuffer(copy, size), "file codec expand failed"); Check(size == file.size() && !std::memcmp(copy, file.data(), size), "file prefix/payload changed"); free(copy);
    }
    {
        // A short header (208) with a tiny compressed payload: the stored file is larger than the header
        // struct but its expansion is not, and callers read the whole struct from the expanded buffer.
        const Bytes tiny(16, 0x5a); const auto stream = CompactZstd::Compress(tiny);
        TKMovesetHeader h; h.header_size = 208; h.block_list = 208; h.block_list_size = 1; h.moveset_data_start = 216;
        h.compressionType = TKMovesetCompressionType_CompactZstd; h.moveset_data_size = tiny.size();
        Bytes stored(216 + stream.size(), 0); std::memcpy(stored.data(), &h, 208); std::copy(stream.begin(), stream.end(), stored.begin() + 216);
        Check(stored.size() >= sizeof(TKMovesetHeader), "short-expansion fixture is not a full stored header");
        uint64_t size = stored.size(); auto copy = static_cast<Byte*>(malloc(size)); Check(copy != nullptr, "test allocation failed"); std::memcpy(copy, stored.data(), size);
        const bool expanded = CompressionUtils::RAW::Moveset::ExpandFileBuffer(copy, size); free(copy);
        Check(!expanded, "expansion shorter than the header struct accepted");
    }
    auto archive = Archive(panm, encoded), frame = Frame(archive, panm); Bytes output(panm.size());
    Check(CompactZstd::Decompress(frame, output) && output == panm, "streaming PANM roundtrip failed");
    Bytes guarded(panm.size() + 32, 0xa5); auto destination = std::span(guarded).subspan(16, panm.size());
    auto reject = [&](View bytes) { Check(!CompactZstd::Decompress(bytes, destination), "malformed frame accepted"); Check(std::all_of(guarded.begin(), guarded.begin() + 16, [](auto b){return b == 0xa5;}) && std::all_of(guarded.end() - 16, guarded.end(), [](auto b){return b == 0xa5;}), "decoder crossed output bounds"); };
    for (size_t length = 0; length < frame.size(); ++length) reject(View(frame).first(length));
    for (size_t at = 0; at < frame.size(); ++at) {
        auto bad = frame; bad[at] ^= 0x40;
        // Zstd's checksum protects decoded data, not unused bits in an equivalent compressed form.
        const bool accepted = CompactZstd::Decompress(bad, destination);
        Check(!accepted || std::equal(destination.begin(), destination.end(), panm.begin()), "corrupt frame changed accepted output");
    }
    auto extra = frame; extra.push_back(0); reject(extra);
    auto wrongOriginal = frame; Put<uint64_t>(wrongOriginal, 8, UINT64_MAX); Put<uint32_t>(wrongOriginal, 36, uint32_t(crc32(0, wrongOriginal.data(), 36))); reject(wrongOriginal);
    // Inner stream mutations are recompressed with valid Zstd checksums, exercising the parser.
    for (size_t at : {size_t(0), size_t(4), size_t(8), size_t(16), size_t(17), size_t(21), size_t(25), size_t(29), size_t(33), size_t(37), size_t(41), size_t(45), size_t(49), size_t(53)}) {
        auto bad = archive; bad[at] ^= 0x40; reject(Frame(bad, panm));
    }
    auto zero = archive; Put<uint32_t>(zero, 17, 0); reject(Frame(zero, panm));
    auto trailing = archive; trailing.push_back(0); reject(Frame(trailing, panm));
    std::mt19937 random(1901);
    for (unsigned test = 0; test < 300; ++test) {
        auto bad = archive; const size_t at = 25 + random() % encoded.size(); bad[at] ^= uint8_t(1u << (random() & 7));
        // Mutations in redundant/opaque metadata may be structurally valid; the output CRC still rejects changed output.
        const bool accepted = CompactZstd::Decompress(Frame(bad, panm), destination);
        Check(!accepted || std::equal(destination.begin(), destination.end(), panm.begin()), "mutated PANM changed accepted output");
    }
    std::cout << "Compact codecs: exact FP/padding roundtrips, all legacy codecs, streaming, truncation, corruption and malformed-input checks passed\n";
}
}

int main(int argc, char** argv) try {
    if (argc == 1) { Tests(); return 0; }
    const std::string command = argv[1];
    if (argc == 4 && command == "panm-encode") { Save(argv[3], CompactPanm::Encode(Load(argv[2]))); return 0; }
    if (argc == 4 && command == "panm-decode") { const auto input = Load(argv[2]); const auto size = Get<uint32_t>(input, 8); Check(size <= CompactPanm::MaximumSize, "input budget"); Bytes out(size); CompactPanm::Decode(input, out); Save(argv[3], out); return 0; }
    if (argc == 4 && (command == "corpus" || command == "corpus-reference")) {
        std::ifstream mf(argv[2]); Json manifest; mf >> manifest; Json result = {{"files", 0}, {"bytes", 0}, {"encoded", 0}, {"reference_matches", 0}, {"reference_decodes", 0}};
        double encodeSeconds = 0, decodeSeconds = 0; size_t count = 0;
        for (const auto& row : manifest["manifest"]) {
            auto raw = Load(std::filesystem::u8path(row["path"].get<std::string>()));
            auto start = std::chrono::steady_clock::now(); auto compact = command == "corpus-reference" ? CompactPanm::Encode(raw, CompactPanm::EncodeSettings{}) : CompactPanm::Encode(raw); encodeSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            Bytes output(raw.size()); start = std::chrono::steady_clock::now(); CompactPanm::Decode(compact, output); decodeSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(); Check(output == raw, "corpus reconstruction mismatch");
            if (row.contains("filtered")) {
                auto reference = Load(std::filesystem::u8path(row["filtered"].get<std::string>())); std::memcpy(reference.data(), "PCF1", 4);
                CompactPanm::Decode(reference, output); Check(output == raw, "Historical reference decode mismatch");
                result["reference_decodes"] = result["reference_decodes"].get<size_t>() + 1;
                if (command == "corpus-reference") { Check(reference == compact, "Python/native encoder mismatch"); result["reference_matches"] = result["reference_matches"].get<size_t>() + 1; }
            }
            result["bytes"] = result["bytes"].get<size_t>() + raw.size(); result["encoded"] = result["encoded"].get<size_t>() + compact.size();
            if (++count % 200 == 0) std::cout << count << " corpus files checked\n" << std::flush;
        }
        result["files"] = count; result["encode_seconds"] = encodeSeconds; result["decode_seconds"] = decodeSeconds;
        std::ofstream report(argv[3]); report << result.dump(2) << '\n'; std::cout << result.dump(2) << '\n'; return 0;
    }
    throw std::runtime_error("Usage: CompactCompressionTests [panm-encode IN OUT | panm-decode IN OUT | corpus MANIFEST REPORT]");
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
