#include "AddressScanner/AddressUpdate.hpp"
#include "GameAddressesFile.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {
namespace fs = std::filesystem;
using namespace ExeSignatures;
void Check(bool value, const std::string& message) { if (!value) throw std::runtime_error("Address scanner: " + message); }

template<class T> void Put(std::vector<uint8_t>& bytes, size_t at, T value) { memcpy(bytes.data() + at, &value, sizeof(T)); }
void PutBytes(std::vector<uint8_t>& bytes, size_t at, std::initializer_list<uint8_t> values) { std::copy(values.begin(), values.end(), bytes.begin() + at); }

constexpr uint32_t TextVa = 0x1000, TextRaw = 0x200, DataVa = 0x3000, DataRaw = 0x400, SectionSize = 0x200;

// Minimal PE: .text (code, raw 0x200) and .data (writable, VA 0x3000).
std::vector<uint8_t> Fixture() {
    std::vector<uint8_t> file(DataRaw + SectionSize, 0xCC);
    std::fill(file.begin(), file.begin() + TextRaw, 0);
    Put<uint16_t>(file, 0, 0x5A4D); Put<uint32_t>(file, 0x3C, 0x40);
    Put<uint32_t>(file, 0x40, 0x00004550); Put<uint16_t>(file, 0x46, 2); Put<uint32_t>(file, 0x48, 0x6ADB1AD5);
    Put<uint16_t>(file, 0x54, 0); // no optional header: sections follow at 0x58
    auto section = [&](size_t at, const char* name, uint32_t va, uint32_t raw, uint32_t characteristics) {
        memcpy(file.data() + at, name, strlen(name));
        Put<uint32_t>(file, at + 8, SectionSize); Put<uint32_t>(file, at + 12, va);
        Put<uint32_t>(file, at + 16, SectionSize); Put<uint32_t>(file, at + 20, raw); Put<uint32_t>(file, at + 36, characteristics);
    };
    section(0x58, ".text", TextVa, TextRaw, 0x60000020);
    section(0x80, ".data", DataVa, DataRaw, 0xC0000040);
    // mov rax, [rip + X] ; test rax, rax  -> X points to .data + 0x10
    PutBytes(file, TextRaw + 0x10, { 0x48, 0x8B, 0x05, 0, 0, 0, 0, 0x48, 0x85, 0xC0, 0x11, 0x22 });
    Put<int32_t>(file, TextRaw + 0x13, static_cast<int32_t>(DataVa + 0x10) - static_cast<int32_t>(TextVa + 0x10 + 7));
    // cmp qword [rip + X], 0 (one immediate byte after the displacement) -> .data + 0x20
    PutBytes(file, TextRaw + 0x40, { 0x48, 0x83, 0x3D, 0, 0, 0, 0, 0x00, 0x75, 0x05, 0xB9, 0x10 });
    Put<int32_t>(file, TextRaw + 0x43, static_cast<int32_t>(DataVa + 0x20) - static_cast<int32_t>(TextVa + 0x40 + 8));
    // mov rax, [rsi + 0x3B50] ; cmp [rsi + ...], rax
    PutBytes(file, TextRaw + 0x80, { 0x44, 0x38, 0xAE, 0x50, 0x3B, 0x00, 0x00, 0x48, 0x8B, 0x86, 0x50, 0x3B, 0x00, 0x00, 0x48, 0x39, 0x86 });
    // Twice: never unique
    PutBytes(file, TextRaw + 0x100, { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x10, 0x00, 0x00, 0x00 });
    PutBytes(file, TextRaw + 0x120, { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x10, 0x00, 0x00, 0x00 });
    // RIP-relative into .text itself (a call target / code address: accepted), one far outside the image (rejected),
    // and a value that is no struct offset
    PutBytes(file, TextRaw + 0x160, { 0x4C, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00, 0x90, 0x91, 0x92 });
    PutBytes(file, TextRaw + 0x1A0, { 0x4C, 0x8D, 0x0D, 0x00, 0x00, 0xFF, 0x7F, 0x93, 0x94, 0x95 });
    PutBytes(file, TextRaw + 0x180, { 0xAB, 0xCD, 0xEF, 0x01, 0x02, 0x03, 0xFF, 0xFF, 0xFF, 0xFF });
    return file;
}

const KeyResult& Key(const ScanReport& report, const std::string& name) {
    for (const auto& key : report.keys) if (key.key == name) return key;
    throw std::runtime_error("Address scanner: no result for " + name);
}

void ScanTests() {
    const auto image = ParseImage(Fixture());
    Check(image.peTimestamp == 0x6ADB1AD5 && image.sections.size() == 2, "PE headers");
    Check(image.code[0].size() == SectionSize && image.code[1].empty(), "only executable sections are kept");
    const std::map<std::string, std::string> patterns{
        { "sig_registry_rva_1", "48 8B 05 [r4] 48 85 C0" },
        { "sig_registry_rva_2", "48 8B 05 [r4] 48 85 C0 11 22" },       // same instruction, longer: agrees
        { "sig_registry_rva_3", "48 8B 0D [r4] 4C 8B F0" },             // absent: ignored, key still found
        { "sig_manager_1", "48 83 3D [r4+1] 00 75 ?? B9 10" },
        { "sig_motbin_offset_1", "44 38 AE ?? ?? ?? ?? 48 8B 86 [v4] 48 39 86" },
        { "sig_twice_1", "DE AD BE EF 01 02 [v4]" },
        { "sig_missing_1", "01 02 03 04 05 06 [v4]" },
        { "sig_conflict_1", "48 8B 05 [r4] 48 85 C0" },
        { "sig_conflict_2", "48 83 3D [r4+1] 00 75 ?? B9" },
        { "sig_code_1", "4C 8D 05 [r4] 90 91 92" },
        { "sig_outside_1", "4C 8D 0D [r4] 93 94 95" },
        { "sig_value_1", "AB CD EF 01 02 03 [v4]" },
        { "not_a_pattern", "zz" },                                      // ignored: not sig_<key>_<n>
    };
    const auto report = Scan(image, patterns);
    Check(report.peTimestamp == 0x6ADB1AD5, "report timestamp");
    const auto& registry = Key(report, "registry_rva");
    Check(registry.status == KeyStatus::Found && registry.value == DataVa + 0x10, "RIP-relative capture");
    Check(registry.patterns.size() == 3 && registry.patterns[2].status == PatternStatus::NotFound, "absent pattern is reported");
    Check(Key(report, "manager").value == DataVa + 0x20 && Key(report, "manager").status == KeyStatus::Found, "[r4+1] counts the trailing immediate");
    Check(Key(report, "motbin_offset").status == KeyStatus::Found && Key(report, "motbin_offset").value == 0x3B50, "value capture");
    Check(Key(report, "twice").status == KeyStatus::NotFound && Key(report, "twice").patterns[0].status == PatternStatus::Ambiguous,
        "a pattern matching twice yields nothing");
    Check(Key(report, "missing").status == KeyStatus::NotFound, "no match");
    Check(Key(report, "conflict").status == KeyStatus::Conflict, "disagreeing patterns are a conflict, not a value");
    // code and read-only data targets are captured too (call targets, vtables: Tag's shared-animation keys)
    Check(Key(report, "code").status == KeyStatus::Found && Key(report, "code").value == TextVa + 0x160 + 7, "RIP target in code is accepted");
    Check(Key(report, "outside").patterns[0].status == PatternStatus::Invalid, "RIP target outside the image is rejected");
    Check(Key(report, "value").patterns[0].status == PatternStatus::Invalid, "implausible struct offset is rejected");
    for (const auto& key : report.keys) Check(key.key != "not_a_pattern" && key.key != "a_pattern", "non-pattern keys are skipped");

    auto throws = [&](const char* text) {
        try { Scan(image, { { "sig_bad_1", text } }); } catch (const std::invalid_argument&) { return true; }
        return false;
    };
    Check(throws("48 8B 05 ?? ?? ?? ??"), "pattern without capture");
    Check(throws("48 [r4] [v4] 8B 05"), "pattern with two captures");
    Check(throws("48 ?? 8B ?? 05 ?? 11 ?? 22 ?? 33 ?? [r4]"), "pattern without 2 consecutive fixed bytes");
    Check(throws("48 8B ?? [r4] ?? 8B"), "pattern with fewer than 6 fixed bytes");
    Check(throws("48 8B 05 [r4] XY"), "bad hex byte");
    Check(throws("48 8B 05 [v4+1]"), "[v4] with +N");

    Check(TargetKey("sig_registry_rva_12") == "registry_rva" && TargetKey("sig_x") == "" && TargetKey("registry_rva_1") == "" &&
        TargetKey("sig_a_b") == "", "pattern key names");

    // On disk: same result as from memory.
    const auto path = fs::temp_directory_path() / ("tkmovesets-exe-" + std::to_string(GetCurrentProcessId()) + ".exe");
    { const auto bytes = Fixture(); std::ofstream out(path, std::ios::binary); out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size()); }
    const auto fromDisk = ReadImage(path);
    std::error_code error; fs::remove(path, error);
    Check(fromDisk.code == image.code && fromDisk.peTimestamp == image.peTimestamp, "ReadImage reads the same code as ParseImage");
}

void IniTests() {
    const std::string text =
        "## -- T8 Retail -- ##\r\n"
        "val:t8-retail_registry_rva = 0x959A320     # player lookup\r\n"
        "val:t8-retail_motbin_offset = 0x3B50       # moveset root\r\n"
        "# val:t8-retail_motbin_offset = 0x1        commented out\r\n"
        "t8-retail_anim_manager = +0x98D0C40, 0x8, 0\r\n"
        "val:t8cnt_motbin_offset = 0x2BF8\r\n"
        "str:t8-retail_sig_motbin_offset_1 = 48 8B 86 [v4]\r\n"
        "val:t8-retail_currmove = 0x3E0";
    std::vector<std::string> missing;
    const auto out = RewriteIni(text, "t8-retail", { { "registry_rva", 0xA0B0C0D }, { "motbin_offset", 0x3B58 }, { "anim_manager", 0x98D1000 },
        { "currmove", 0x3F0 }, { "nowhere", 1 } }, missing);
    const std::string expected =
        "## -- T8 Retail -- ##\r\n"
        "val:t8-retail_registry_rva = 0xA0B0C0D     # player lookup\r\n"
        "val:t8-retail_motbin_offset = 0x3B58       # moveset root\r\n"
        "# val:t8-retail_motbin_offset = 0x1        commented out\r\n"
        "t8-retail_anim_manager = +0x98D1000, 0x8, 0\r\n"
        "val:t8cnt_motbin_offset = 0x2BF8\r\n"
        "str:t8-retail_sig_motbin_offset_1 = 48 8B 86 [v4]\r\n"
        "val:t8-retail_currmove = 0x3F0";
    Check(out == expected, "ini rewrite keeps everything but the values:\n" + out);
    Check(missing == std::vector<std::string>{ "nowhere" }, "keys absent from the ini are reported, not inserted");
    // Longer and shorter values keep the comment column when there is room.
    std::vector<std::string> unused;
    Check(RewriteIni("val:g_k = 0x10   # c\n", "g", { { "k", 0x1000 } }, unused) == "val:g_k = 0x1000 # c\n", "longer value");
    Check(RewriteIni("val:g_k = 0x1000 # c\n", "g", { { "k", 0x10 } }, unused) == "val:g_k = 0x10   # c\n", "shorter value");
    Check(RewriteIni("val:g_k = 0x10 # c\n", "g", { { "k", 0x123456 } }, unused) == "val:g_k = 0x123456 # c\n", "no room: one space");
}

void AddressFileTests() {
    // The shipped address file: every pattern parses, and names a value that exists to be updated.
    GameAddressesFile addresses;
    addresses.Reload();
    const auto patterns = addresses.GetStringsWithPrefix("t8-retail", "sig_");
    Check(patterns.size() >= 20, "shipped retail patterns are loaded");
    Scan(ParseImage(Fixture()), patterns); // throws on a malformed pattern
    for (const auto& [name, text] : patterns) {
        const auto key = TargetKey(name);
        Check(!key.empty(), "pattern key " + name);
        bool relative = false; const std::vector<gameAddr>* path = nullptr;
        Check(addresses.HasKey("t8-retail", key.c_str()) &&
            (addresses.GetPtrPathEx("t8-retail", key.c_str(), relative, path) ? relative : true), "pattern target " + key + " exists");
    }
    // In-place update: values and the module offset of a pointer path, nothing else.
    const auto registry = addresses.GetValue("t8-retail", "registry_rva");
    Check(addresses.UpdateInPlace("t8-retail", "registry_rva", registry + 0x100) && addresses.GetValue("t8-retail", "registry_rva") == registry + 0x100,
        "value updated in place");
    bool relative = false;
    const auto& manager = addresses.GetPtrPath("t8-retail", "anim_manager", relative);
    const auto before = manager;
    Check(addresses.UpdateInPlace("t8-retail", "anim_manager", 0x1234) && &addresses.GetPtrPath("t8-retail", "anim_manager", relative) == &manager &&
        manager[0] == 0x1234 && manager.size() == before.size() && manager[1] == before[1], "pointer path module offset updated in place");
    Check(!addresses.UpdateInPlace("t8-retail", "no_such_key", 1) && !addresses.UpdateInPlace("no-such-game", "registry_rva", 1), "unknown keys are not added");
}

std::string ReadText(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
}
void WriteText(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary); out << text;
}

// The update step shared by Tools > Addresses and --headless update-addresses-t8.
void UpdateTests() {
    const auto directory = fs::temp_directory_path() / ("tkmovesets-addresses-" + std::to_string(GetCurrentProcessId()));
    fs::create_directories(directory);
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code e; fs::remove_all(path, e); } } cleanup{ directory };
    const auto ini = directory / "game_addresses.ini", exe = directory / "game.exe";
    const std::string text =
        "val:g-test_registry_rva = 0x10     # player lookup\r\n"
        "val:g-test_motbin_offset = 0x3B50\r\n"
        "g-test_manager = +0x1, 0x8, 0\r\n"
        "val:g-test_twice = 0x5\r\n"
        "str:g-test_sig_registry_rva_1 = 48 8B 05 [r4] 48 85 C0\r\n"
        "str:g-test_sig_motbin_offset_1 = 44 38 AE ?? ?? ?? ?? 48 8B 86 [v4] 48 39 86\r\n"
        "str:g-test_sig_manager_1 = 48 83 3D [r4+1] 00 75 ?? B9 10\r\n"
        "str:g-test_sig_twice_1 = DE AD BE EF 01 02 [v4]\r\n"
        "str:g-test_sig_absent_1 = 48 8B 05 [r4] 48 85 C0 11 22\r\n";
    WriteText(ini, text);
    { const auto bytes = Fixture(); std::ofstream out(exe, std::ios::binary); out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size()); }

    GameAddressesFile addresses; addresses.ReloadFrom(ini);
    const auto report = ScanGame(addresses, "g-test", exe);
    auto outcome = [](const UpdateResult& r, const std::string& key) {
        for (const auto& row : r.rows) if (row.scan.key == key) return row.outcome;
        throw std::runtime_error("Address scanner: no row for " + key);
    };

    // Report only: nothing changes, on disk or in memory.
    const auto dry = ApplyScan(addresses, "g-test", report, ini, false);
    Check(dry.changes == 2 && dry.updated == 0 && dry.failed == 1 && !dry.writeFailed, "dry run counts");
    Check(outcome(dry, "registry_rva") == Outcome::Updated && outcome(dry, "manager") == Outcome::Updated &&
        outcome(dry, "motbin_offset") == Outcome::Unchanged && outcome(dry, "twice") == Outcome::NotFound &&
        outcome(dry, "absent") == Outcome::NotInIni, "dry run outcomes");
    Check(ReadText(ini) == text && addresses.GetValue("g-test", "registry_rva") == 0x10, "dry run wrote nothing");

    // A write that cannot happen (read-only file) leaves memory alone too.
    const auto readOnly = directory / "read-only.ini";
    WriteText(readOnly, text);
    SetFileAttributesW(readOnly.c_str(), FILE_ATTRIBUTE_READONLY);
    const auto failed = ApplyScan(addresses, "g-test", report, readOnly, true);
    SetFileAttributesW(readOnly.c_str(), FILE_ATTRIBUTE_NORMAL);
    Check(ReadText(readOnly) == text, "read-only ini untouched");
    Check(failed.writeFailed && failed.updated == 0 && outcome(failed, "registry_rva") == Outcome::Unchanged &&
        addresses.GetValue("g-test", "registry_rva") == 0x10, "failed write changes nothing");

    // The real update: file and memory.
    const auto done = ApplyScan(addresses, "g-test", report, ini, true);
    Check(done.updated == 2 && done.changes == 2 && done.failed == 1 && !done.writeFailed, "update counts");
    std::string expected = text;
    expected.replace(expected.find("0x10     #"), 10, "0x3010   #");
    expected.replace(expected.find("+0x1,"), 5, "+0x3020,");
    Check(ReadText(ini) == expected, "updated ini:\n" + ReadText(ini));
    bool relative = false;
    Check(addresses.GetValue("g-test", "registry_rva") == 0x3010 && addresses.GetPtrPath("g-test", "manager", relative)[0] == 0x3020,
        "loaded addresses updated");
    // Running again finds nothing to change.
    GameAddressesFile reloaded; reloaded.ReloadFrom(ini);
    const auto again = ApplyScan(reloaded, "g-test", ScanGame(reloaded, "g-test", exe), ini, true);
    Check(again.changes == 0 && again.updated == 0 && ReadText(ini) == expected, "second update is a no-op");

    bool threw = false;
    try { ScanGame(addresses, "no-such-game", exe); } catch (const std::runtime_error&) { threw = true; }
    Check(threw, "a game without patterns is an error");
}

// Lines with nothing after '=' (a placeholder whose value is only a comment) are skipped; parsing never throws.
void EmptyValueTests() {
    const auto directory = fs::temp_directory_path() / ("tkmovesets-addresses-empty-" + std::to_string(GetCurrentProcessId()));
    fs::create_directories(directory);
    struct Cleanup { fs::path path; ~Cleanup() { std::error_code e; fs::remove_all(path, e); } } cleanup{ directory };
    const auto ini = directory / "game_addresses.ini";
    WriteText(ini,
        "val:g-test_todo = # todo\r\n"
        "g-test_empty =\r\n"
        "str:g-test_blank =    \r\n"
        "  val:g-test_indented = 0x20\r\n"
        "val:nounderscore = 3\r\n"
        "val:g-test_kept = 0x7\r\n");
    GameAddressesFile addresses;
    try { addresses.ReloadFrom(ini); }
    catch (const std::exception& error) { throw std::runtime_error(std::string("address file with empty values threw: ") + error.what()); }
    Check(!addresses.HasKey("g-test", "todo") && !addresses.HasKey("g-test", "empty") && !addresses.HasKey("g-test", "blank"),
        "empty values are skipped");
    Check(addresses.GetValue("g-test", "kept") == 0x7 && addresses.GetValue("g-test", "indented") == 0x20, "other lines still load");
}
}

void RunAddressScannerTests() {
    ScanTests();
    IniTests();
    AddressFileTests();
    UpdateTests();
    EmptyValueTests();
}
