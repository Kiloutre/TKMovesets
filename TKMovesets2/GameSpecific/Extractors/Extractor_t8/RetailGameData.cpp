#include "RetailGameData.hpp"
#include "RetailNames.hpp"
#include "RetailSnapshot.hpp"
#include <Windows.h>
#include <bcrypt.h>
#include <zstd.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <mutex>
#include <fstream>
#include <stdexcept>
#include <set>
#include <Windows.h>
#include <TlHelp32.h>

namespace RetailT8GameData {
namespace {
constexpr char Magic[] = "__TEKKEN8FILES__";
constexpr std::array<uint32_t, 16> XorKeys = {
    0xADFB76C8, 0x7DEF1F1C, 0xA84B71E0, 0xD88448A2, 0x964F5B9E, 0x7EE2BC2C, 0xA27D5221, 0xBB9FE67D,
    0xAD15269F, 0xEC1A9785, 0x9BAE2F45, 0xA4296896, 0x275AA004, 0x37E22F31, 0x3803D4A7, 0x9B81329F};
constexpr std::string_view KeyPool =
    "U%ZPP0S2y4WoIofdsjM@2a%LJr@#*Ir!u8?FTSD2T%YcYsYl3I0W8=tSiI9.EOTQJ4?"
    "09hmYC3L5FL0uzRHN2?JF9IX9WI7t?UAQ4FujsC3XH+RkqIGJ%k19$OJS&.AKh$@LFT1"
    "C83nXrjJ0MMcZVWF0Ln!SfhwzSKoWDjQj7c!PXVEYOKD@+BUZGhY5@#RgW+Gw0sKw63Q"
    "BjAp$$QOEFB%$V5?jcS@Q+hfg!=DrtmmALG0G@UXXEaVr17bnLrDnovHG.%PfIyB!8W9n"
    "&#xAywcKo6te3DAYi$KSCCz8bXQiN331x77uw5IJVq.BUfckF4sEBp%XV+e6rFg*V.ZS40"
    "#supW8T3ZB4e892MpxUMPMxr8d4LCltyVlJHBWKjK2W4SlynsF1CK5CxAkwLd6?ta.W6c"
    "W4IrQ1f@x5tUvpfPxOOyao6kEjfaPKGsBYB=Y6qQwQW3l3oGtV5zEH=&EgBL8h7Zz5sm3"
    "L&HsrFLS8CKoh3lR.XBAWXnB%S*O5&q?H9zjOqqr#JqwX3TU%2hYHTJ&2dY*fWIrPdc!P"
    "$hPlzFZUQPkgy6+Pg5C8f#bYYXua9gDn484!XO=5BuHI2FX$RR8%&rX%c@6u8EFBYXLhi"
    "0BNW!w7d+Z=iOabHqZZXSQ6Db*sncGALQFLgm%lvHfKhMP5.78Gm0Dd?nFE9zsTBIF=f6"
    "VBtD=*1PWxmnthUSjGQ&tf@9l$CAJR87UM4I$2oa@8ncr48dDMyMuM7KZX4@B.Ny6dObQe"
    "SdJ2=8jkV61GvIBOHAbbUiWfDCu52&MRs=@Bz#HHUMoQ9WFm5TxKWdoHlDEOW$!4$z==V"
    "G9PkfxPIT4.y*737zPZ9V4?JG.rl7kHK$k44$hdf.L*QX.=wp&JbpULUm#GI2vJv*lPJ$"
    "2B7M9Gg.+.O6tgUZvU4HMV2&7gvcBHDA.FG9jzC@s5*6XW8mUHB6j$*tz$OVPYDjW@07a"
    "pt&poihk.I69&*d%y=m%@XP1ERk+bTP0v5iHd?xQ9C%1O2t=Tj7.xpgABlCyee1i1hPVz"
    "egGXSQD1Fp1lQd*IxwpA%HtBmuG6c$#43S?HK*Q*!Tj0kAU&RTbDoRfW=!MwGiHM2KMMN"
    "NBK=31f#83gt%NXAzix3D!6EQ5O3NSZKvQYvSKuvUSu1gGAATF1MqWG7USVjG9NAjz1&?K"
    "W0dTMaCJfD!Luym?WWLN3nf9#UcDUvL02m7L2$zJA=dbNQ2N5kVybBg7JE3ByS3AaZO=u"
    "@ty2g*dPrYW7UN&pqA1g*rQ+7%PM3XTsLcH=B+BFRIDPTsLj!+=68T+%vIMaRzFoiC4rJ"
    "A#uevTsalLrpO.WfY2SG%%qbKihqRXg2=IJwXKZ+xoYUydgLLh9eRt+Df0&5&C3T7i9sj"
    ".Z2TEUCxaNl2T8d@=Muw$S955+iHs=KCt?LZNFy*#%UU8gOHl*I9lQUXH4#cVYy9JY@gi"
    "lIQ?v.TdkUBN9gHob*OVicLITzNqQpGL6wvupDkVqTlAORJTgL5RMKqQYpSD8LiYUp#aW"
    "KgFCBs1hCYTzFwa$*cllNQW.M2GSSI+x#qa#JcK7Cwh.v78hI1&T3c#DR&VDazL.XL2KG"
    "USSBKmcc31R%JCa9736gp=QV5#I7%BGEdJkOs7QhKxt0n3sS4#sAl1BGNNXPx&9PoPx8.G"
    "dv=eDotAfJmKuSUAirWB0XUe4FG9PYTgR??$t2IKh4+LjNCZ8TkXGqh=7rN4B86TZ3gjJ"
    "6QY#sT?FIc?Z*IstFG43YZqw%BwC&Ml2p7LB@JH!YW!zvNJYKjgaNm1!%sf$ZQbms4Akc"
    "Ojs2Bh";
static_assert(KeyPool.size() == 1800); // the key offset wraps at size - 32
constexpr Byte ZstdMagic[] = {0x28, 0xB5, 0x2F, 0xFD};

void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class T> T At(std::span<const Byte> bytes, size_t offset) {
    Check(offset <= bytes.size() && sizeof(T) <= bytes.size() - offset, "Truncated tkdata/ANMBIN field");
    T value; memcpy(&value, bytes.data() + offset, sizeof(value)); return value;
}
// AES-256-ECB with the pool key of `seed`, then PKCS7 padding removed when present (format of tkdata.py).
void Decrypt(std::vector<Byte>& data, uint8_t seed) {
    if (data.size() % 16) data.resize(data.size() + 16 - data.size() % 16, 0);
    // Windows CNG (bcrypt), no padding: ECB blocks are independent, so the buffer is decrypted in place in chunks.
    const auto key = reinterpret_cast<PUCHAR>(const_cast<char*>(KeyPool.data() + (44983u * seed) % (KeyPool.size() - 32)));
    BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_KEY_HANDLE handle = nullptr;
    Check(BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0)), "Cannot start tkdata decryption");
    std::unique_ptr<void, void(*)(void*)> closeAlgorithm(algorithm, [](void* a) { BCryptCloseAlgorithmProvider(a, 0); });
    Check(BCRYPT_SUCCESS(BCryptSetProperty(algorithm, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_ECB)),
        sizeof(BCRYPT_CHAIN_MODE_ECB), 0)) && BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(algorithm, &handle, nullptr, 0, key, 32, 0)),
        "Cannot start tkdata decryption");
    std::unique_ptr<void, void(*)(void*)> closeKey(handle, [](void* k) { BCryptDestroyKey(k); });
    for (size_t at = 0; at < data.size();) {
        const auto length = static_cast<ULONG>((std::min)(data.size() - at, size_t(1) << 28));
        ULONG written = 0;
        Check(BCRYPT_SUCCESS(BCryptDecrypt(handle, data.data() + at, length, nullptr, nullptr, 0, data.data() + at, length, &written, 0)) &&
            written == length, "tkdata decryption failed");
        at += length;
    }
    const auto pad = data.empty() ? 0 : data.back();
    if (pad >= 1 && pad <= 16 && pad <= data.size()) {
        bool padded = true;
        for (size_t i = data.size() - pad; i < data.size(); ++i) padded &= data[i] == pad;
        if (padded) data.resize(data.size() - pad);
    }
}
std::vector<Byte> Unzstd(std::span<const Byte> frame) {
    std::unique_ptr<ZSTD_DStream, decltype(&ZSTD_freeDStream)> stream(ZSTD_createDStream(), ZSTD_freeDStream);
    Check(stream && !ZSTD_isError(ZSTD_initDStream(stream.get())), "Cannot start tkdata decompression");
    std::vector<Byte> out;
    const auto known = ZSTD_getFrameContentSize(frame.data(), frame.size());
    if (known != ZSTD_CONTENTSIZE_ERROR && known != ZSTD_CONTENTSIZE_UNKNOWN) {
        Check(known <= (4ULL << 30), "tkdata file is too large");
        out.reserve(static_cast<size_t>(known));
    }
    ZSTD_inBuffer in{frame.data(), frame.size(), 0};
    std::vector<Byte> chunk(ZSTD_DStreamOutSize());
    size_t result = 1;
    while (in.pos < in.size || result) {
        ZSTD_outBuffer o{chunk.data(), chunk.size(), 0};
        result = ZSTD_decompressStream(stream.get(), &o, &in);
        Check(!ZSTD_isError(result), "Invalid tkdata zstd data");
        out.insert(out.end(), chunk.begin(), chunk.begin() + o.pos);
        if (!o.pos && in.pos >= in.size) break;
    }
    Check(!result, "Truncated tkdata zstd data");
    return out;
}
std::vector<Byte> ReadAt(std::ifstream& file, uint64_t size, uint64_t at, uint64_t length) {
    Check(at <= size && length <= size - at && length <= (4ULL << 30), "tkdata extent is outside the archive");
    std::vector<Byte> bytes(static_cast<size_t>(length));
    file.clear(); file.seekg(static_cast<std::streamoff>(at));
    Check(bool(file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(length))), "Cannot read tkdata.bin");
    return bytes;
}
}

std::filesystem::path TkdataPath(const std::filesystem::path& gameExecutable) {
    // .../Polaris/Binaries/Win64/Polaris-Win64-Shipping.exe
    return gameExecutable.parent_path().parent_path().parent_path() / "Content" / "Binary" / "pak" / "tkdata.bin";
}

namespace {
struct Toc { std::vector<Byte> bytes; int32_t count = 0, first = 0; };
Toc ReadToc(std::ifstream& file, uint64_t size) {
    auto magic = ReadAt(file, size, 0, 16);
    Check(!memcmp(magic.data(), Magic, 16), "tkdata.bin has an unknown signature");
    auto footer = ReadAt(file, size, size - 128, 128);
    for (size_t i = 0; i < XorKeys.size(); ++i)
        for (size_t j = 0; j < 4; ++j) footer[i * 4 + j] ^= static_cast<Byte>(XorKeys[i] >> (8 * j));
    Check(!memcmp(footer.data(), "kaPniBNB", 8), "tkdata.bin footer is invalid");
    const auto tocOffset = At<int32_t>(footer, 0x18), tocSize = At<int32_t>(footer, 0x20);
    Check(tocOffset > 0 && tocSize > 0, "tkdata.bin table of contents is invalid");
    auto toc = ReadAt(file, size, static_cast<uint64_t>(tocOffset), static_cast<uint64_t>(tocSize));
    if (footer[0x11]) Decrypt(toc, footer[0x11]);
    Check(toc.size() >= 4 && !memcmp(toc.data(), ZstdMagic, 4), "tkdata.bin table of contents is not compressed as expected");
    Toc result{Unzstd(toc)};
    result.count = At<int32_t>(result.bytes, 8); result.first = At<int32_t>(result.bytes, 0x10);
    Check(result.count > 0 && result.count <= 100000 && result.first >= 0 &&
        static_cast<uint64_t>(result.first) + static_cast<uint64_t>(result.count) * 64 <= result.bytes.size(), "tkdata.bin table of contents is invalid");
    return result;
}
uint64_t OpenArchive(const std::filesystem::path& tkdata, std::ifstream& file) {
    std::error_code error;
    const auto size = std::filesystem::file_size(tkdata, error);
    if (error) throw std::runtime_error("The game's tkdata.bin was not found at " + tkdata.string());
    file.open(tkdata, std::ios::binary);
    Check(file && size >= 16 + 128, "Cannot open tkdata.bin");
    return size;
}
}

std::vector<std::string> TkdataContains(const std::filesystem::path& tkdata, const std::vector<std::string>& candidates) {
    std::ifstream file;
    const auto size = OpenArchive(tkdata, file);
    const auto toc = ReadToc(file, size);
    std::set<uint64_t> hashes;
    for (int32_t i = 0; i < toc.count; ++i) hashes.insert(At<uint64_t>(toc.bytes, static_cast<size_t>(toc.first) + i * 64ULL + 16));
    std::vector<std::string> found;
    for (const auto& path : candidates) if (hashes.count(RetailNames::Hash(path))) found.push_back(path);
    return found;
}

const std::vector<TkdataFighter>& TkdataRoster(const std::filesystem::path& tkdata) {
    struct Entry { uintmax_t size = 0; std::filesystem::file_time_type time{}; std::vector<TkdataFighter> roster; };
    static std::mutex mutex;
    static std::map<std::wstring, Entry> cache;
    static const std::vector<TkdataFighter> none;
    std::error_code error;
    const auto size = std::filesystem::file_size(tkdata, error);
    if (error) return none;
    const auto time = std::filesystem::last_write_time(tkdata, error);
    std::lock_guard lock(mutex);
    auto& entry = cache[tkdata.wstring()];
    if (entry.size == size && entry.time == time && !entry.roster.empty()) return entry.roster;
    entry = Entry{size, time, {}};
    try {
        static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
        std::vector<std::string> candidates;
        candidates.reserve(36 * 36 * 36);
        for (char a : std::string_view(alphabet)) for (char b : std::string_view(alphabet)) for (char c : std::string_view(alphabet))
            candidates.push_back(std::string("mothead/bin/") + a + b + c + ".motbin");
        for (const auto& path : TkdataContains(tkdata, candidates)) {
            try {
                const auto motbin = ReadTkdataFile(tkdata, path);
                if (motbin.size() < 0x168) continue;
                const auto identity = At<uint32_t>(motbin, 0x164);
                // id << 16 | (0x10000 - id): Paul (0) comes out as 0x00010000; an all-zero identity is no fighter
                if (identity == 0) continue;
                const uint32_t id = identity == 0x10000 ? 0 : identity >> 16, check = identity & 0xFFFF;
                if (identity != 0x10000 && ((id + check) & 0xFFFF) != 0) continue;
                entry.roster.push_back({path.substr(12, 3), id});
            } catch (const std::exception&) {}
        }
        std::sort(entry.roster.begin(), entry.roster.end(), [](const TkdataFighter& x, const TkdataFighter& y) { return x.id < y.id; });
    } catch (const std::exception&) { entry.roster.clear(); }
    return entry.roster;
}

std::string TkdataCodeFor(const std::filesystem::path& tkdata, uint32_t fighterId) {
    for (const auto& fighter : TkdataRoster(tkdata)) if (fighter.id == fighterId) return fighter.code;
    return {};
}

std::optional<std::filesystem::path> RunningGameExecutable() {
    std::unique_ptr<void, decltype(&CloseHandle)> snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0), CloseHandle);
    if (snapshot.get() == INVALID_HANDLE_VALUE) { snapshot.release(); return std::nullopt; }
    PROCESSENTRY32W entry{sizeof(entry)};
    for (bool more = Process32FirstW(snapshot.get(), &entry); more; more = Process32NextW(snapshot.get(), &entry)) {
        if (_wcsicmp(entry.szExeFile, L"Polaris-Win64-Shipping.exe")) continue;
        std::unique_ptr<void, decltype(&CloseHandle)> process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID), CloseHandle);
        if (!process) continue;
        wchar_t path[MAX_PATH * 4]; DWORD length = static_cast<DWORD>(std::size(path));
        if (QueryFullProcessImageNameW(process.get(), 0, path, &length)) return std::filesystem::path(std::wstring(path, length));
    }
    return std::nullopt;
}

std::vector<Byte> ReadTkdataFile(const std::filesystem::path& tkdata, std::string_view vfsPath) {
    std::error_code error;
    const auto size = std::filesystem::file_size(tkdata, error);
    if (error) throw std::runtime_error("The game's tkdata.bin was not found at " + tkdata.string());
    std::ifstream file(tkdata, std::ios::binary);
    Check(file && size >= 16 + 128, "Cannot open tkdata.bin");
    auto magic = ReadAt(file, size, 0, 16);
    Check(!memcmp(magic.data(), Magic, 16), "tkdata.bin has an unknown signature");
    auto footer = ReadAt(file, size, size - 128, 128);
    for (size_t i = 0; i < XorKeys.size(); ++i)
        for (size_t j = 0; j < 4; ++j) footer[i * 4 + j] ^= static_cast<Byte>(XorKeys[i] >> (8 * j));
    Check(!memcmp(footer.data(), "kaPniBNB", 8), "tkdata.bin footer is invalid");
    const auto tocOffset = At<int32_t>(footer, 0x18), tocSize = At<int32_t>(footer, 0x20);
    Check(tocOffset > 0 && tocSize > 0, "tkdata.bin table of contents is invalid");
    auto toc = ReadAt(file, size, static_cast<uint64_t>(tocOffset), static_cast<uint64_t>(tocSize));
    if (footer[0x11]) Decrypt(toc, footer[0x11]);
    Check(toc.size() >= 4 && !memcmp(toc.data(), ZstdMagic, 4), "tkdata.bin table of contents is not compressed as expected");
    toc = Unzstd(toc);
    const auto count = At<int32_t>(toc, 8), first = At<int32_t>(toc, 0x10);
    Check(count > 0 && count <= 100000 && first >= 0, "tkdata.bin table of contents is invalid");
    const uint64_t hash = RetailNames::Hash(vfsPath);
    for (int32_t i = 0; i < count; ++i) {
        const size_t entry = static_cast<size_t>(first) + static_cast<size_t>(i) * 64;
        if (At<uint64_t>(toc, entry + 16) != hash) continue;
        const auto flag = At<uint8_t>(toc, entry + 8), seed = At<uint8_t>(toc, entry + 9);
        const auto offset = At<uint64_t>(toc, entry + 24), stored = At<uint64_t>(toc, entry + 32), size2 = At<uint64_t>(toc, entry + 40);
        Check(offset <= UINT64_MAX - 16, "tkdata.bin entry is invalid");
        auto data = ReadAt(file, size, 16 + offset, stored);
        Decrypt(data, seed);
        if (data.size() >= 4 && !memcmp(data.data(), ZstdMagic, 4)) {
            const auto frame = flag == 0 && size2 && size2 < data.size() ? std::span<const Byte>(data).first(size2) : std::span<const Byte>(data);
            return Unzstd(frame);
        }
        if (size2 && size2 < data.size()) data.resize(static_cast<size_t>(size2));
        return data;
    }
    throw std::runtime_error("tkdata.bin has no " + std::string(vfsPath));
}

Anmbin::Anmbin(std::vector<Byte> bytes) : m_bytes(std::move(bytes)) {
    const std::span<const Byte> all(m_bytes);
    Check(all.size() >= 0x98, "Invalid ANMBIN size");
    for (uint32_t type = 0; type < 6; ++type) {
        const auto count = At<uint32_t>(all, 4 + 4 * type);
        const auto pool = At<uint64_t>(all, 0x38 + 8 * type);
        Check(count <= 100000 && pool <= all.size() && count * 0x38ULL <= all.size() - pool, "Invalid ANMBIN category bounds");
        for (uint32_t i = 0; i < count; ++i) {
            const auto instance = static_cast<uint32_t>(At<uint64_t>(all, pool + i * 0x38ULL));
            const auto offset = At<uint64_t>(all, pool + i * 0x38ULL + 8);
            if (m_declared.insert({type, instance}).second) m_order.push_back({type, instance});
            if (!offset) continue; // a shared reference (the blob lives in com.anmbin), never data at offset zero
            Check(offset < all.size() && all.size() - offset >= 8, "Invalid ANMBIN PANM offset");
            m_entries[{type, instance}].push_back(offset);
        }
    }
}

std::optional<std::span<const Byte>> Anmbin::Panm(uint32_t type, uint32_t instance) const {
    const auto found = m_entries.find({type, instance});
    if (found == m_entries.end()) return std::nullopt;
    std::optional<std::span<const Byte>> result;
    for (const auto offset : found->second) {
        const auto rest = std::span<const Byte>(m_bytes).subspan(static_cast<size_t>(offset));
        const auto size = RetailT8Snapshot::PanmSize(rest.first((std::min)(rest.size(), size_t(1) << 20)));
        Check(size <= rest.size(), "ANMBIN PANM exceeds the archive");
        const auto panm = rest.first(static_cast<size_t>(size));
        if (result && !std::equal(result->begin(), result->end(), panm.begin(), panm.end()))
            throw std::runtime_error("ANMBIN holds different animations under one identity (category " + std::to_string(type) +
                ", instance " + std::to_string(instance) + ")");
        result = panm;
    }
    return result;
}

std::vector<uint32_t> Anmbin::List(uint32_t type) const {
    const std::span<const Byte> all(m_bytes);
    Check(type < 6, "Invalid ANMBIN list");
    const auto count = At<uint32_t>(all, 0x1c + 4 * type);
    const auto at = At<uint64_t>(all, 0x68 + 8 * type);
    Check(count <= 1000000 && at <= all.size() && count * 4ULL <= all.size() - at, "Invalid ANMBIN list bounds");
    std::vector<uint32_t> list(count);
    if (count) memcpy(list.data(), all.data() + at, count * 4ULL);
    return list;
}

std::vector<std::pair<uint32_t, uint32_t>> Anmbin::Identities() const {
    std::vector<std::pair<uint32_t, uint32_t>> result;
    for (const auto& [identity, offsets] : m_entries) result.push_back(identity);
    return result;
}

std::shared_ptr<Anmbin> Source::Open(const std::string& code) {
    if (const auto found = m_open.find(code); found != m_open.end()) return found->second;
    auto archive = std::make_shared<Anmbin>(ReadTkdataFile(m_tkdata, "mothead/bin/" + code + ".anmbin"));
    m_open[code] = archive;
    return archive;
}

std::optional<std::vector<Byte>> Source::Panm(uint32_t domain, uint32_t type, uint32_t instance) {
    const auto code = m_code(domain & 0xff);
    std::vector<std::string> codes{"com"};
    if ((domain & 0xff) != 0x81) {
        if (code.empty()) throw std::runtime_error("No animation archive is known for animation character " + std::to_string(domain & 0xff) +
            " (character codes come from TKMInterface/t8-retail_names.json)");
        codes.push_back(code);
    }
    bool declared = false;
    for (const auto& name : codes) {
        const auto archive = Open(name);
        if (const auto panm = archive->Panm(type, instance)) return std::vector<Byte>(panm->begin(), panm->end());
        declared |= archive->Declared(type, instance);
    }
    if (declared) return std::nullopt; // listed without data in every archive: nothing exists to read
    throw std::runtime_error("Animation " + std::to_string(instance) + " (category " + std::to_string(type) +
        ") is in neither com.anmbin nor " + (codes.size() > 1 ? codes[1] : std::string("com")) + ".anmbin");
}
}
