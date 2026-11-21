#include "AppTheme.hpp"
#include "headless.hpp"
#include "Importer_t8/RetailImport.hpp"
#include "Importer_t8/RetailPreview.hpp"
#include "Importer_t8/RetailLiveEdit.hpp"
#include "Editor_t8Retail/Editor_t8Retail.hpp"
#include "Importer_t8/RetailResourceLoader.hpp"
#include "Importer_t8/Importer_t8.hpp"
#include "Importer_t8/RetailImportShadow.hpp"
#include "Importer_t8/RetailAnimationArchive.hpp"
#include "GameStaticData.hpp"
#include "Extractor_t8/Extractor_t8.hpp"
#include "Extractor_t8/RetailRuntime.hpp"
#include "Extractor_t8/RetailGameData.hpp"
#include "AddressScanner/AddressUpdate.hpp"
#include "Extractor_t8/RetailDiskMoveset.hpp"
#include "Extractor_t7/T7DiskMoveset.hpp"
#include "LegacyJsonMoveset.hpp"
#include "AnimExtractors/AnimExtractor_t8.hpp"
#include <atomic>
#include "Animations.hpp"
#include "MovesetFile.hpp"
#include "Compression.hpp"
#include "FileIO.hpp"
#include "MainWindow.hpp"
#include "Localization.hpp"
#include "Settings.hpp"
#include "Games.hpp"
#include "EditorLabel.hpp"
#include "RetailT8Records.hpp"
#include "T7ToRetailT8.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include "imgui_extras.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>
#include <fstream>
#include <set>
#include <sstream>
#include <functional>
#include "RetailPanm.hpp"
#include "AnimationReader.hpp"
#include "T7AnimToRetailT8.hpp"
#include "T7HandToRetailT8.hpp"
#include "T7CameraToRetailT8.hpp"
#include "T7WingToRetailT8.hpp"
#include "T7FaceToRetailT8.hpp"
#include "RetailT8AnimToT7.hpp"
#include "T8ToT7.hpp"
#include "ConversionRules.hpp"
#include "Online_t8.hpp"
#include "SharedMemory_t8.h"

#ifdef TKM_EDITOR_FORM_TESTS
void RunEditorFormTests();
void RunRetailEditorTests();
int VerifyRetailEditorFile(const std::wstring& input, const std::wstring& output);
int RenderRetailEditorFile(const std::wstring& input, const std::wstring& output,bool references,bool lists);
int RunRetailEditorUiTests();
int VerifyRetailEditorStructure(const std::wstring& input,const std::wstring& output);
int AuditRetailEditorValidation(std::span<const std::wstring> inputs);
int WriteRetailPanmFixture(const std::wstring& output);
int CheckAnimationPreview(const std::wstring& reference);
int ShootAnimationPreview(std::span<const std::wstring> arguments);
int DumpAnimationPreviewJoints(const std::wstring& animation, const std::wstring& output);
int ExportAnimationPreviewSource(std::span<const std::wstring> arguments);
int ScanAnimationPreview(const std::wstring& file);
#endif

static TKMovesetCompressionType_ Codec(const std::wstring& name)
{
    if (name == L"none") return TKMovesetCompressionType_None;
    if (name == L"lz4") return TKMovesetCompressionType_LZ4;
    if (name == L"lzma") return TKMovesetCompressionType_LZMA;
    if (name == L"compact-zstd") return TKMovesetCompressionType_CompactZstd;
    throw std::invalid_argument("Codec must be none, lz4, lzma or compact-zstd");
}

// Generic export/import: a game is named by its unique data key (GameInfo::minorDataString).
static const GameInfo* FindGame(const std::wstring& key)
{
    const auto name = Helpers::to_utf8(key);
    for (unsigned i = 0; i < Games::GetGamesCount(); ++i) {
        const auto* game = Games::GetGameInfoFromIndex(i);
        if (game->minorDataString && name == game->minorDataString) return game;
    }
    throw std::invalid_argument("Unknown game '" + name + "'; `--headless games` lists them");
}

// "a,b,c" -> mask of the given bit numbers, each below [bits]
static uint64_t BitList(const std::wstring& list, unsigned bits, const char* what)
{
    uint64_t mask = 0;
    for (size_t at = 0; at <= list.size();) {
        const auto end = (std::min)(list.find(L',', at), list.size());
        const auto token = list.substr(at, end - at);
        size_t consumed = 0; unsigned long bit = 0;
        try { bit = std::stoul(token, &consumed); } catch (...) { consumed = 0; }
        if (token.empty() || consumed != token.size() || bit >= bits) throw std::invalid_argument(std::string("Invalid ") + what + " list");
        mask |= 1ULL << bit; at = end + 1;
    }
    return mask;
}

// default|all|none|body,hands,face,swing,camera,extra -> the six retail T8 animation category bits (bit 0 = body)
static uint64_t RetailCategories(const std::wstring& value)
{
    if (value == L"default") return (ExtractSettings_T8_DEFAULT >> 18) & 63;
    if (value == L"all") return 63;
    if (value == L"none") return 0;
    constexpr const wchar_t* names[] = {L"body", L"hands", L"face", L"swing", L"camera", L"extra"};
    uint64_t categories = 0;
    for (size_t at = 0; at <= value.size();) {
        const auto end = (std::min)(value.find(L',', at), value.size());
        const auto token = value.substr(at, end - at); bool found = false;
        for (unsigned i = 0; i < 6; ++i) if (token == names[i]) { categories |= 1ULL << i; found = true; }
        if (!found) throw std::invalid_argument("Unknown retail animation category");
        at = end + 1;
    }
    return categories;
}

// True if both paths name the same file. The lexical check covers an output that does not exist yet;
// equivalent() covers different case, hard links and 8.3 short names.
static bool SameFile(const std::filesystem::path& a, const std::filesystem::path& b)
{
    if (std::filesystem::absolute(a).lexically_normal() == std::filesystem::absolute(b).lexically_normal()) return true;
    std::error_code error;
    if (!std::filesystem::exists(a, error) || !std::filesystem::exists(b, error)) return false;
    return std::filesystem::equivalent(a, b, error) && !error;
}

static uint8_t PlayerSlot(const std::wstring& value, const GameInfo* game)
{
    size_t consumed = 0; unsigned long slot = 0;
    try { slot = std::stoul(value, &consumed); } catch (...) { consumed = 0; }
    if (value.empty() || consumed != value.size() || slot >= game->characterCount)
        throw std::invalid_argument("PLAYER_SLOT must be 0.." + std::to_string(game->characterCount - 1));
    return static_cast<uint8_t>(slot);
}

static const char* ExtractionErrorName(ExtractionErrcode_ code)
{
    switch (code) {
    case ExtractionErrcode_Successful: return "successful";
    case ExtractionErrcode_AllocationErr: return "allocation_error";
    case ExtractionErrcode_FileCreationErr: return "file_creation_error";
    case ExtractionErrcode_CompressionFailure: return "compression_failure";
    case ExtractionErrcode_MovesetDataError: return "moveset_data_error";
    }
    return "unknown";
}

static const char* ImportationErrorName(ImportationErrcode_ code)
{
    switch (code) {
    case ImportationErrcode_Successful: return "successful";
    case ImportationErrcode_AllocationErr: return "allocation_error";
    case ImportationErrcode_GameAllocationErr: return "game_allocation_error";
    case ImportationErrcode_FileReadErr: return "file_read_error";
    case ImportationErrcode_DecompressionError: return "decompression_error";
    case ImportationErrcode_UnsupportedGameVersion: return "unsupported_game_version";
    case ImportationErrcode_MovesetDataError: return "moveset_data_error";
    case ImportationErrcode_GameWriteError: return "game_write_error";
    case ImportationErrcode_GameRollbackError: return "game_rollback_error";
    }
    return "unknown";
}

static std::set<std::filesystem::path> DirectoryFiles(const std::filesystem::path& directory)
{
    std::set<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) if (entry.is_regular_file()) files.insert(entry.path());
    return files;
}

static int UiSmoke()
{
    auto check = [](bool valid, const char* error) { if (!valid) throw std::runtime_error(error); };
    static_assert(GameVersions::T8::CLOSED_NETWORK_TEST == 0);
    static_assert(GameVersions::T8::CLOSED_BETA_TEST == 1);
    static_assert(GameVersions::T8::RETAIL == 2);
    auto retail = Games::GetGameInfoFromIdentifier(GameId_T8, GameVersions::T8::RETAIL);
    auto cnt = Games::GetGameInfoFromIdentifier(GameId_T8, GameVersions::T8::CLOSED_NETWORK_TEST);
    auto cbt = Games::GetGameInfoFromIdentifier(GameId_T8, GameVersions::T8::CLOSED_BETA_TEST);
    check(retail && cnt && cbt && retail != cnt && retail != cbt, "T8 profiles are not distinct");
    check(std::string(retail->name) == "Tekken 8" && std::string(retail->dataString) == "t8-retail" &&
        std::string(retail->movesetNamePrefix) == "T8_", "retail T8 identity is incorrect");
    check(retail->extractor && retail->characterCount == 2 && retail->importer && retail->editorLogic && retail->editorVisuals &&
        !retail->onlineHandler && retail->GetBaseAddressFunc &&
        Games::IsGameEditable(GameId_T8, GameVersions::T8::RETAIL) && Games::IsGameLiveEditable(GameId_T8, GameVersions::T8::RETAIL), "retail T8 extraction registration differs");
    check(Games::GetGameInfoFromIdentifier(GameId_T8) == retail, "default T8 identity is not retail");
    check(!cnt->onlineHandler, "T8 CNT must not be injectable");
    GameAddressesFile retailAddresses;
    retailAddresses.Reload();
    check(retailAddresses.GetValue("t8-retail", "motbin_offset") == 0x3B50 &&
        retailAddresses.GetValue("t8-retail", "chara_id_offset") == 0x168 &&
        retailAddresses.GetValue("t8-retail", "base") == 0, "retail address values differ");
    bool relative = false;
    const auto& retailPath = retailAddresses.GetPtrPath("t8-retail", "anim_manager", relative);
    check(relative && retailPath.size() == 3 && retailPath[0] == 0x98D0C40 && retailPath[1] == 8 && retailPath[2] == 0,
        "retail animation address path differs");
    check(retailAddresses.GetValue("t8-retail", "registry_rva") == 0x959A320 &&
        retailAddresses.GetValue("t8-retail", "round_key_rva") == 0x959A088, "retail registry addresses differ");



    // Exercise actual packaged resources through the production label loader.
    const auto previousDirectory = std::filesystem::current_path();
    wchar_t executable[32768];
    const auto executableLength = GetModuleFileNameW(nullptr, executable, _countof(executable));
    check(executableLength && executableLength < _countof(executable), "cannot locate label resources");
    std::filesystem::current_path(std::filesystem::path(executable).parent_path());
    try {
        EditorLabel t7Labels(Games::GetGameInfoFromIdentifier(GameId_T7), nullptr);
        EditorLabel retailLabels(retail, nullptr);
        check(t7Labels.GetText(881) && std::string(t7Labels.GetText(881)) == "End", "T7 sheet labels did not load");
        check(retailLabels.GetText(1100) && std::string(retailLabels.GetText(1100)) == "End", "retail labels did not load");
        check(!retailLabels.GetText(881), "retail labels inherited T7 identifiers");
        check(t7Labels.GetText(33036) && std::string(t7Labels.GetText(33036)).find("# 2") != std::string::npos,
            "hash inside label was truncated");
        check(t7Labels.GetText(32937) && std::string(t7Labels.GetText(32937)).find('\n') != std::string::npos,
            "multiline sheet label did not load");
    } catch (...) { std::filesystem::current_path(previousDirectory); throw; }
    std::filesystem::current_path(previousDirectory);

    std::vector<std::string> errors;
    auto context = std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)>(ImGui::CreateContext(), ImGui::DestroyContext);
    ImGuiIO& io = ImGui::GetIO();
    AppTheme::Apply();
    context->ErrorCallbackUserData = &errors;
    context->ErrorCallback = [](ImGuiContext*, void* state, const char* message) {
        static_cast<std::vector<std::string>*>(state)->emplace_back(message);
    };
    io.ConfigErrorRecoveryEnableAssert = false;
    io.ConfigErrorRecoveryEnableTooltip = false;
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(PROGRAM_WIN_WIDTH, PROGRAM_WIN_HEIGHT);
    io.DeltaTime = 1.0f / 60;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    Settings::LoadFile();
    int frames = 0;
    for (const char* locale : {"en-US", "fr-FR"}) {
        Localization::LoadFile(locale);
        MainWindow program(true);
        const NAV__MENU menus[] = {NAV__MENU_EXTRACT, NAV__MENU_IMPORT, NAV__MENU_EDITION,
                NAV__MENU_PERSISTENT_PLAY, NAV__MENU_ONLINE_PLAY, NAV__MENU_ABOUT, NAV__MENU_ADDRESSES, NAV__MENU_EXTRA};
        for (const ImVec2 size : {ImVec2(PROGRAM_WIN_WIDTH, PROGRAM_WIN_HEIGHT), ImVec2(1279, 746), ImVec2(640, 480)}) {
            io.DisplaySize = size;
            for (auto menu : menus) {
                program.SetActiveMenu(menu);
                for (int frame = 0; frame < 3; ++frame) {
                    ImGui::NewFrame();
                    program.Update();
                    ImGui::Begin("Textbox extent regression");
                    ImGuiExtra::RenderTextbox("A padded textbox as the final item.");
                    ImGui::End();
                    ImGui::Render();
                    if (!errors.empty()) throw std::runtime_error("ImGui: " + errors.front());
                    ++frames;
                    auto draw = ImGui::GetDrawData();
                    if (!draw || !draw->Valid || draw->TotalVtxCount == 0)
                        throw std::runtime_error("Application produced no UI draw data");
                }
            }
        }
    }
    Localization::Clear();
    return frames;
}

int RunHeadless(std::span<const std::wstring> arguments)
{
    using nlohmann::json;
    try {
        if (arguments.empty() || arguments[0] == L"--help" || arguments[0] == L"-h" || arguments[0] == L"/?") {
            std::cout <<
                "Usage: TKMovesets2 --headless COMMAND [ARGUMENTS]\n"
                "       TKMovesets2 --help                         this text (also -h, /?)\n"
                "Commands print JSON. Exit status: 0 success, 1 failure, 2 usage error.\n"
                "Relative paths are resolved from the current directory.\n"
                "Codecs: none | lz4 | lzma | compact-zstd. PLAYER_SLOT: 0 = P1, 1 = P2.\n"
                "File commands default to none; retail T8 exports default to compact-zstd.\n"
                "CATEGORIES (retail T8 animations): default | all | none | body,hands,face,swing,camera,extra\n"
                "\n"
                "Moveset files (offline)\n"
                "  list DIRECTORY                                  scan a moveset folder like the GUI list\n"
                "  inspect FILE...                                 container, decompression and metadata\n"
                "  validate FILE...                                inspect + layout and reference checks\n"
                "  convert INPUT OUTPUT [CODEC]                    older games -> T7\n"
                "  convert-t8 INPUT OUTPUT [CODEC]                 T7 -> retail T8 (as convert-t8-full, no report)\n"
                "  convert-t8-full T7_INPUT OUTPUT [CODEC] [REPORT.json]\n"
                "                                                  full T7 -> retail T8 conversion\n"
                "  convert-t7 T8_INPUT OUTPUT [CODEC] [REPORT.json]\n"
                "                                                  full retail T8 -> T7 conversion\n"
                "  profile FILE                                    the file's import profile\n"
                "  set-profile FILE OUTPUT [KEY=VALUE...]          copy with import profile options: identity=target|ID\n"
                "                                                  gender=target|male|female story_flag=game|N in_heat=0|1\n"
                "                                                  flags_clear=0|1 story_mode=0|1 dlc_story=0|1\n"
                "                                                  special_modes=0|1 stage=0|1 wave_index=game|N\n"
                "                                                  battle_script=game|N item=game|none|all\n"
                "                                                  body=game|normal|big|bear story_effects=0|1\n"
                "                                                  global_writes=0|1\n"
                "  check-rules [FILE]                              T7<->T8 conversion rules of a game_addresses.ini or a rules\n"
                "                                                  file; no FILE: the rules in use\n"
                "  recompress INPUT OUTPUT [CODEC]\n"
                "  convert-records-t8 INPUT OUTPUT.json            supported T7 records as T8 JSON (not loadable)\n"
                "  plan-convert-t8 T7_INPUT OUTPUT.json\n"
                "  plan-import-t8 FILE...                          offline pointer diagnostic\n"
                "  repair-handles-t8 CONVERTED DONOR OUTPUT [CODEC]\n"
                "  animation-info FILE...\n"
                "\n"
                "Retail T8 animations (offline)\n"
                "  read-animation-t8 ANIMATION_FILE NEW_PANM_FILE  .panm as is, T7 0x64/0xC8 converted\n"
                "  extract-animations-t8 FILE NEW_DIRECTORY\n"
                "  export-panm-t8 DOC MOVE OUTPUT.panm             MOVE: row or alias (e.g. 0x8001)\n"
                "  export-panms-t8 DOC NEW_DIRECTORY\n"
                "  import-panm-t8 DOC MOVE FILE.panm OUTPUT [NAME|-] [CODEC]\n"
                "  check-panm-t8 PANM_FILE...\n"
                "  panm-dump PANM_FILE OUTPUT.json\n"
                "  panm-reencode PANM_FILE OUTPUT.panm\n"
                "  panm-to-t7 PANM_FILE OUTPUT_FILE c8|64\n"
                "  panm-skeleton-inc INPUT.panm OUTPUT.inc\n"
                "  t7-anim-to-panm T7_FILE MOVE_ROW T8_DONOR_WITH_ANIMATIONS OUTPUT.panm\n"
                "  dump-t7-anims T7_FILE OUTPUT_DIRECTORY\n"
                "\n"
                "Game files on disk (offline)\n"
                "  read-tkdata-t8 TKDATA_BIN VFS_PATH OUT_FILE     one decrypted file of the archive\n"
                "  roster-t8 TKDATA_BIN                             every character moveset in the archive, with its fighter id\n"
                "  extract-disk-animations-t8 TKDATA_BIN NEW_DIRECTORY [CODE,...]\n"
                "  build-disk-movesets-t8 TKDATA_BIN DIRECTORY [CODE,...|all] [CATEGORIES] [CODEC]   CODEC default compact-zstd\n"
                "  build-disk-movesets-t7 TKDATA_BIN DIRECTORY [CODE,...|all] [CODEC]                CODEC default lz4\n"
                "  convert-json DIRECTORY FOLDER... [--codec CODEC] old TekkenMovesetExtractor folders (T7, Tag2) to\n"
                "                                                  .tkmvst; CODEC default lz4, existing files left alone\n"
                "  verify-anmbin-t8 FILE ANMBIN_DIRECTORY\n"
                "  dump-payload-t8 FILE OUT_FILE                   decompressed T8RS snapshot\n"
                "  t8-record-stats OUTPUT.json IDLE_DIRECTORY FILE...\n"
                "\n"
                "Game addresses\n"
                "  scan-addresses-t8 [EXE] [--ini FILE]            read the exe; change nothing\n"
                "  update-addresses-t8 [EXE] [--ini FILE]          write changed values to game_addresses.ini\n"
                "\n"
                "Running game, any supported game\n"
                "  games                                           list GAME keys (t7, t8-retail, ttt2, ...)\n"
                "  export GAME DIRECTORY PLAYER_SLOT|all [--codec CODEC] [--motas default|all|none|0,1,...]\n"
                "         [--animations CATEGORIES] [--no-archive] [--unknown-motas] [--overwrite] [--no-movelist]\n"
                "  import FILE GAME PLAYER_SLOT [--no-instant] [--keep-unused] [--motas all|none|0,1,...]\n"
                "\n"
                "Running game, retail T8\n"
                "  export-t8 DIRECTORY PLAYER_SLOT [CATEGORIES] [no-archive]\n"
                "  check-import-t8 FILE PLAYER_SLOT                import into local shadow memory; no game writes\n"
                "  import-t8 FILE PLAYER_SLOT\n"
                "  stage-t8 FILE PLAYER_SLOT\n"
                "  check-preview-t8 FILE PLAYER_SLOT MOVE_ID       shadow import + move queue; no game writes\n"
                "  preview-t8 FILE PLAYER_SLOT MOVE_ID\n"
                "  queue-t8 PLAYER_SLOT MOVE_ID\n"
                "  state-t8 PLAYER_SLOT\n"
                "  live-edit-t8 PLAYER_SLOT PUBLISHED_FILE EDITED_FILE\n"
                "  live-verify-t8 PLAYER_SLOT DOC [EDIT...]        EDIT: TABLE:ROW:FIELD=VALUE | delete:TABLE:ROW=COUNT\n"
                "                                                        | rows:TABLE:FIRST=ID,...\n"
                "  compare-compiled-t8 PLAYER_SLOT [COUNT]         read-only\n"
                "  anim-info-t8 PLAYER_SLOT MOVE...                read-only\n"
                "  poke-t8 ADDRESS QWORD                           research: writes one qword to game memory\n"
                "  tag-t8 P3 P4|none [SECONDS] | tag-t8 off        Tools > Extra: Tag in Practice (runtime fighter ids)\n"
                "\n"
                "Self-tests\n"
                "  ui-smoke                                        render every menu offscreen\n";
#ifdef TKM_EDITOR_FORM_TESTS
            std::cout <<
                "  editor-smoke\n"
                "  retail-editor-smoke INPUT OUTPUT\n"
                "  retail-editor-structure-smoke INPUT OUTPUT\n"
                "  retail-editor-render INPUT OUTPUT.json [references|lists]\n"
                "  retail-validation-audit FILE|FOLDER...          every untouched T8 field must pass form validation\n"
                "  retail-editor-ui-test\n"
                "  retail-panm-fixture OUTPUT_DIRECTORY\n"
                "  anim-preview-check REFERENCE.json               animation preview pose against Blender\n"
                "  anim-preview-shot FILE MOVE OUT_DIR [FRAMES] [yaw=R] [size=WxH]  move form with its preview, as PNG\n"
                "  anim-preview-joints ANIMATION OUT.json          preview joint positions per frame\n"
                "  anim-preview-export T7_FILE MOVE OUT [panm]     a T7 move's animation (or its PANM conversion)\n"
                "  anim-preview-scan FILE                          decode and pose every move animation\n";
#endif
            std::cout.flush();
            return arguments.empty() ? 2 : 0;
        }
        auto& command = arguments[0];
        if (command == L"scan-addresses-t8" || command == L"update-addresses-t8") {
            // Tools > Addresses. scan: report only. update: also write the changed values to game_addresses.ini.
            // EXE defaults to the running game's, else the Steam install; the ini to the one next to this program (the GUI's).
            const bool update = command == L"update-addresses-t8";
            std::optional<std::filesystem::path> exe, ini;
            for (size_t i = 1; i < arguments.size(); ++i) {
                if (arguments[i] == L"--ini" && i + 1 < arguments.size()) ini = arguments[++i];
                else if (!exe && !arguments[i].starts_with(L"--")) exe = arguments[i];
                else throw std::invalid_argument("Expected [EXE] [--ini GAME_ADDRESSES_INI]");
            }
            if (!exe) exe = ExeSignatures::FindT8Executable();
            if (!exe) throw std::runtime_error("Tekken 8 executable not found: pass its path");
            if (!ini) {
                wchar_t program[32768];
                const auto length = GetModuleFileNameW(nullptr, program, _countof(program));
                if (!length || length >= _countof(program)) throw std::runtime_error("Cannot locate the program directory");
                ini = std::filesystem::path(program).parent_path() / GAME_ADDRESSES_FILE;
            }
            GameAddressesFile addresses; addresses.ReloadFrom(*ini);
            const auto report = ExeSignatures::ScanGame(addresses, "t8-retail", *exe);
            const auto result = ExeSignatures::ApplyScan(addresses, "t8-retail", report, *ini, update);
            json keys = json::object();
            for (const auto& row : result.rows) {
                json patterns = json::object();
                for (const auto& p : row.scan.patterns) {
                    static const char* names[] = { "found", "not_found", "ambiguous", "invalid" };
                    patterns[p.name] = p.status == ExeSignatures::PatternStatus::Found ? json(ExeSignatures::Hex(p.value)) :
                        p.status == ExeSignatures::PatternStatus::Invalid ? json(p.detail) : json(names[static_cast<int>(p.status)]);
                }
                const bool found = row.scan.status == ExeSignatures::KeyStatus::Found;
                keys[row.scan.key] = { {"result", ExeSignatures::OutcomeName(row.outcome)},
                    {"ini", row.before ? json(ExeSignatures::Hex(*row.before)) : json(nullptr)},
                    {"exe", found ? json(ExeSignatures::Hex(row.scan.value)) : json(nullptr)}, {"patterns", patterns} };
            }
            std::cout << json({{"ok", !result.writeFailed}, {"written", update && result.updated > 0}, {"exe", Helpers::to_utf8(exe->wstring())},
                {"ini", Helpers::to_utf8(ini->wstring())}, {"pe_timestamp", ExeSignatures::Hex(result.peTimestamp)},
                {"changes", result.changes}, {"updated", result.updated}, {"failed", result.failed}, {"keys", keys}}).dump(2) << '\n';
            return result.writeFailed ? 1 : 0;
        }
        if (command == L"read-tkdata-t8") { // diagnostic: one file of the game's tkdata.bin, decrypted, to a new file
            if (arguments.size() != 4) throw std::invalid_argument("Expected TKDATA_BIN VFS_PATH OUT_FILE");
            if (std::filesystem::exists(arguments[3])) throw std::runtime_error("Output file must be new");
            const auto bytes = RetailT8GameData::ReadTkdataFile(arguments[1], Helpers::to_utf8(arguments[2]));
            if (!FileIO::WriteAtomically(arguments[3], {bytes.data(), bytes.size()})) throw std::runtime_error("Cannot write output file");
            size_t panms = 0; if (arguments[2].ends_with(L".anmbin")) panms = RetailT8GameData::Anmbin(bytes).Count();
            std::cout << json({{"ok", true}, {"bytes", bytes.size()}, {"anmbin_identities", panms}}).dump() << '\n'; return 0;
        }
        if (command == L"roster-t8") { // every character moveset tkdata.bin holds, with its in-match fighter id
            if (arguments.size() != 2) throw std::invalid_argument("Expected TKDATA_BIN");
            const auto started = std::chrono::steady_clock::now();
            const auto& roster = RetailT8GameData::TkdataRoster(arguments[1]);
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
            const auto names = GameStaticData::LoadOptional("t8-retail");
            json fighters = json::array();
            for (const auto& f : roster) {
                const auto* c = names.FindCharacter(f.code);
                fighters.push_back({{"code", f.code}, {"id", f.id}, {"name", c && !c->name.empty() ? c->name : std::string()}});
            }
            std::cout << json({{"ok", !roster.empty()}, {"count", roster.size()}, {"ms", ms}, {"fighters", fighters}}).dump() << '\n';
            return roster.empty() ? 1 : 0;
        }
        if (command == L"read-animation-t8") { // the editor's animation import reader: .panm as is, T7 0x64 / 0xC8 converted
            if (arguments.size() != 3) throw std::invalid_argument("Expected ANIMATION_FILE NEW_PANM_FILE");
            if (std::filesystem::exists(arguments[2])) throw std::runtime_error("Output file must be new");
            const auto panm = EditorT8Retail::ReadAnimationFile(arguments[1]);
            EditorT8Retail::ValidatePanm(panm);
            if (!FileIO::WriteAtomically(arguments[2], panm)) throw std::runtime_error("Cannot write output file");
            std::cout << json({{"ok", true}, {"bytes", panm.size()}, {"frames", EditorT8Retail::PanmFrames(panm)}}).dump() << '\n'; return 0;
        }
        if (command == L"extract-animations-t8") { // the editor's "Extract animations" for one retail file
            if (arguments.size() != 3) throw std::invalid_argument("Expected FILE NEW_DIRECTORY");
            MovesetFile file(arguments[1]); file.Validate();
            const auto count = RetailT8Animations::Extract(file.Payload(), arguments[2]);
            std::cout << json({{"ok", true}, {"animations", count}}).dump() << '\n'; return 0;
        }
        if (command == L"dump-payload-t8") { // diagnostic: the decompressed T8RS snapshot of a retail file
            if (arguments.size() != 3) throw std::invalid_argument("Expected FILE OUT_FILE");
            if (std::filesystem::exists(arguments[2])) throw std::runtime_error("Output file must be new");
            MovesetFile file(arguments[1]); file.Validate();
            if (!FileIO::WriteAtomically(arguments[2], file.Payload())) throw std::runtime_error("Cannot write output file");
            json report{{"ok", true}, {"bytes", file.Payload().size()}};
            if (file.Header().gameId == GameId_T7) { // block offsets within the payload, in TKMovesetHeaderBlocks order
                json blocks = json::array();
                const auto* list = reinterpret_cast<const uint64_t*>(file.Bytes().data() + file.Header().block_list);
                for (size_t i = 0; i < file.Header().block_list_size; ++i) blocks.push_back(list[i]);
                report["blocks"] = blocks;
            }
            std::cout << report.dump() << '\n'; return 0;
        }
        if (command == L"build-disk-movesets-t7") { // every Tekken 7 moveset from the game's files
            if (arguments.size() < 3 || arguments.size() > 5) throw std::invalid_argument("Expected TKDATA_BIN DIRECTORY [CODE,...|all] [CODEC]");
            const auto codec = arguments.size() == 5 ? Codec(arguments[4]) : TKMovesetCompressionType_LZ4;
            std::filesystem::create_directories(arguments[2]); // may already exist: files already there are left alone
            std::vector<std::string> codes;
            if (arguments.size() >= 4 && arguments[3] != L"all") {
                const auto list = Helpers::to_utf8(arguments[3]);
                for (size_t at = 0; at <= list.size();) { const auto end = (std::min)(list.find(',', at), list.size()); codes.push_back(list.substr(at, end - at)); at = end + 1; }
            } else codes = T7Disk::Characters(arguments[1]);
            const auto common = T7Disk::ReadTkdataFile(arguments[1], "mothead\\mask\\common.arc");
            json movesets = json::array(); bool ok = true; // Same as the export batch: ok (exit 0) only when every moveset was built
            for (const auto& code : codes) {
                try {
                    const auto r = T7Disk::WriteMovesetFile(arguments[1], code, common, arguments[2], codec);
                    movesets.push_back({{"code", code}, {"ok", true}, {"name", r.name}, {"character", r.characterId}, {"moves", r.moves}, {"animations", r.animations}, {"existed", r.existed}});
                } catch (const std::exception& error) { movesets.push_back({{"code", code}, {"ok", false}, {"error", error.what()}}); ok = false; }
            }
            std::cout << json({{"ok", ok}, {"movesets", movesets}}).dump() << '\n'; return ok ? 0 : 1;
        }
        if (command == L"convert-json") { // movesets saved by the old Python TekkenMovesetExtractor
            std::vector<std::wstring> folders;
            auto codec = TKMovesetCompressionType_LZ4;
            for (size_t i = 2; i < arguments.size(); ++i) {
                if (arguments[i] == L"--codec") {
                    if (++i == arguments.size()) throw std::invalid_argument("--codec needs a value");
                    codec = Codec(arguments[i]);
                } else folders.push_back(arguments[i]);
            }
            if (arguments.size() < 3 || folders.empty()) throw std::invalid_argument("Expected DIRECTORY FOLDER... [--codec CODEC]");
            std::filesystem::create_directories(arguments[1]);
            json movesets = json::array(); bool ok = true;
            for (const auto& folder : folders) {
                const auto source = Helpers::to_utf8(std::filesystem::path(folder).filename().wstring());
                try {
                    const auto r = LegacyJson::WriteMovesetFile(folder, arguments[1], codec);
                    movesets.push_back({{"folder", source}, {"ok", true}, {"game", r.game}, {"name", r.name}, {"character", r.characterId},
                        {"moves", r.moves}, {"animations", r.animations}, {"borrowed_animations", r.borrowedAnimations},
                        {"padded_animations", r.paddedAnimations}, {"unswapped_animations", r.unswappedAnimations},
                        {"unswapped_mota_animations", r.unswappedMotaAnimations}, {"replaced_animations", r.replacedAnimations}, {"motas", r.motas},
                        {"skipped_motas", r.skippedMotas}, {"dropped_references", r.droppedReferences},
                        {"retargeted_cancels", r.retargetedCancels},
                        {"retargeted_reactions", r.retargetedReactions}, {"filled_projectiles", r.filledProjectiles}, {"file", Helpers::to_utf8(r.path.filename().wstring())}, {"existed", r.existed}});
                } catch (const std::exception& error) { movesets.push_back({{"folder", source}, {"ok", false}, {"error", error.what()}}); ok = false; }
            }
            std::cout << json({{"ok", ok}, {"movesets", movesets}}).dump() << '\n'; return ok ? 0 : 1;
        }
        if (command == L"build-disk-movesets-t8") { // every moveset from the game's files, like the editor's "extract all from disk"
            if (arguments.size() < 3 || arguments.size() > 6) throw std::invalid_argument("Expected TKDATA_BIN DIRECTORY [CODE,...|all] [default|all|none|body,hands,...] [CODEC]");
            const auto codec = arguments.size() == 6 ? Codec(arguments[5]) : TKMovesetCompressionType_CompactZstd;
            uint8_t categories = RetailAnimationSelection(ExtractSettings_T8_PANM_SELECTION | ExtractSettings_T8_DEFAULT);
            if (arguments.size() >= 5) {
                if (arguments[4] == L"all") categories = 63;
                else if (arguments[4] == L"none") categories = 0;
                else if (arguments[4] != L"default") {
                    constexpr const wchar_t* names[] = {L"body", L"hands", L"face", L"swing", L"camera", L"extra"};
                    categories = 0;
                    for (size_t at = 0; at <= arguments[4].size();) {
                        const auto end = (std::min)(arguments[4].find(L',', at), arguments[4].size());
                        const auto token = arguments[4].substr(at, end - at); bool found = false;
                        for (unsigned i = 0; i < 6; ++i) if (token == names[i]) { categories |= uint8_t(1u << i); found = true; }
                        if (!found) throw std::invalid_argument("Unknown retail animation category");
                        at = end + 1;
                    }
                }
            }
            std::filesystem::create_directories(arguments[2]); // may already exist: a moveset whose file is already there fails, nothing is overwritten
            std::vector<std::string> codes;
            if (arguments.size() >= 4 && arguments[3] != L"all") {
                const auto list = Helpers::to_utf8(arguments[3]);
                for (size_t at = 0; at <= list.size();) { const auto end = (std::min)(list.find(',', at), list.size()); codes.push_back(list.substr(at, end - at)); at = end + 1; }
            } else {
                std::vector<std::string> paths;
                for (const auto names = GameStaticData::LoadOptional("t8-retail"); const auto& character : names.Characters())
                    if (!character.code.empty() && character.id) paths.push_back("mothead/bin/" + character.code + ".motbin");
                for (const auto& path : RetailT8GameData::TkdataContains(arguments[1], paths)) codes.push_back(path.substr(12, path.size() - 12 - 7));
            }
            const RetailT8GameData::Anmbin common(RetailT8GameData::ReadTkdataFile(arguments[1], "mothead/bin/com.anmbin"));
            json movesets = json::array(); bool ok = true; // Same as the export batch: ok (exit 0) only when every moveset was built
            for (const auto& code : codes) {
                const auto path = std::filesystem::path(arguments[2]) / Helpers::string_to_wstring(RetailT8Disk::FileName(code));
                try {
                    const auto report = RetailT8Disk::WriteMovesetFile(arguments[1], code, common, path, categories, codec);
                    uint64_t dataless = 0; for (auto n : report.dataless) dataless += n;
                    movesets.push_back({{"code", code}, {"ok", true}, {"domain", report.domain}, {"moves", report.moves}, {"animations", report.animations}, {"dataless", dataless}});
                } catch (const std::exception& error) { movesets.push_back({{"code", code}, {"ok", false}, {"error", error.what()}}); ok = false; }
            }
            std::cout << json({{"ok", ok}, {"movesets", movesets}}).dump() << '\n'; return ok ? 0 : 1;
        }
        if (command == L"extract-disk-animations-t8") { // every PANM of the game's archives, like the editor's disk extraction
            if (arguments.size() != 3 && arguments.size() != 4) throw std::invalid_argument("Expected TKDATA_BIN NEW_DIRECTORY [CODE,...]");
            if (!std::filesystem::create_directory(arguments[2])) throw std::runtime_error("Output directory must be new");
            std::vector<std::string> paths;
            if (arguments.size() == 4) {
                const auto codes = Helpers::to_utf8(arguments[3]);
                for (size_t at = 0; at <= codes.size();) {
                    const auto end = (std::min)(codes.find(',', at), codes.size());
                    paths.push_back("mothead/bin/" + codes.substr(at, end - at) + ".anmbin"); at = end + 1;
                }
            } else {
                paths.push_back("mothead/bin/com.anmbin");
                for (const auto names = GameStaticData::LoadOptional("t8-retail"); const auto& character : names.Characters())
                    if (!character.code.empty()) paths.push_back("mothead/bin/" + character.code + ".anmbin");
                paths = RetailT8GameData::TkdataContains(arguments[1], paths);
            }
            json archives = json::array(); uint64_t animations = 0;
            for (const auto& path : paths) {
                const auto code = path.substr(12, path.size() - 12 - 7);
                std::atomic<uint32_t> done{0};
                RetailT8GameData::Anmbin parsed(RetailT8GameData::ReadTkdataFile(arguments[1], path));
                RetailT8Animations::ExtractArchive(parsed, std::filesystem::path(arguments[2]) / Helpers::string_to_wstring(code), nullptr, &done);
                archives.push_back({{"code", code}, {"animations", done.load()}}); animations += done;
            }
            std::cout << json({{"ok", true}, {"archives", archives}, {"animations", animations}}).dump() << '\n'; return 0;
        }
        if (command == L"verify-anmbin-t8") {
            if (arguments.size() != 3) throw std::invalid_argument("Expected FILE ANMBIN_DIRECTORY");
            MovesetFile file(arguments[1]); file.Validate();
            if (file.Header().gameId != GameId_T8 || file.Header().minorVersion != GameVersions::T8::RETAIL)
                throw std::runtime_error("Expected retail T8 snapshot");
            const auto animationCharacter = RetailT8Runtime::AnimationCharacter(RetailT8Snapshot::Table(file.Payload(), 12), file.Header().characterId);
            auto names = GameStaticData::LoadOptional("t8-retail"); auto character = names.FindCharacter(animationCharacter);
            if (!character) throw std::runtime_error("Unknown retail source character");
            auto report = RetailT8Resources::VerifyArchives(file.Payload(), arguments[2], character->code);
            std::cout << json({{"ok", true}, {"character", file.Header().characterId}, {"animation_character", animationCharacter}, {"code", character->code},
                {"blobs", report.blobs}, {"bytes", report.bytes}}).dump() << '\n'; return 0;
        }
        if (command == L"games") {
            if (arguments.size() != 1) throw std::invalid_argument("Expected no argument");
            json games = json::array();
            for (unsigned i = 0; i < Games::GetGamesCount(); ++i) {
                const auto* game = Games::GetGameInfoFromIndex(i);
                json imports = json::array();
                for (auto id : game->supportedImports) imports.push_back(id);
                games.push_back({{"key", game->minorDataString}, {"name", game->name}, {"process", game->processName},
                    {"game_id", game->gameId}, {"minor_version", game->minorVersion}, {"characters", game->characterCount},
                    {"export", game->extractor != nullptr}, {"import", game->importer != nullptr}, {"imports_from", imports}});
            }
            std::cout << json({{"ok", true}, {"games", games}}).dump() << '\n'; return 0;
        }
        if (command == L"export") { // the Extract menu, for any game with an extractor
            if (arguments.size() < 4) throw std::invalid_argument("Expected GAME DIRECTORY PLAYER_SLOT|all [options]");
            const auto* info = FindGame(arguments[1]);
            if (!info->extractor) throw std::runtime_error(std::string(info->name) + " has no extractor");
            const bool retailT8 = info->gameId == GameId_T8 && info->minorVersion == GameVersions::T8::RETAIL;
            // Same defaults as the Extract menu
            ExtractSettings settings = ExtractSettings_DisplayableMovelist;
            ExtractSettings compression = retailT8 ? ExtractSettings_CompressCompactZstd :
                CompressionUtils::GetCompressionSetting(CompressionUtils::GetDefaultCompressionSetting()).extractSetting;
            uint64_t motas = ExtractSettings_MOTA_HANDS | ExtractSettings_MOTA_CAMERAS;
            uint64_t categories = (ExtractSettings_T8_DEFAULT >> 18) & 63;
            for (size_t i = 4; i < arguments.size(); ++i) {
                const auto& option = arguments[i];
                const bool hasValue = i + 1 < arguments.size();
                if (option == L"--codec" && hasValue) {
                    const auto codec = Codec(arguments[++i]);
                    compression = codec == TKMovesetCompressionType_CompactZstd ? ExtractSettings_CompressCompactZstd :
                        codec == TKMovesetCompressionType_LZ4 ? ExtractSettings_CompressLZ4 :
                        codec == TKMovesetCompressionType_LZMA ? ExtractSettings_CompressLZMA : 0;
                }
                else if (option == L"--motas" && hasValue) {
                    const auto& value = arguments[++i];
                    motas = value == L"default" ? uint64_t(ExtractSettings_MOTA_HANDS | ExtractSettings_MOTA_CAMERAS) :
                        value == L"all" ? 0xfffULL : value == L"none" ? 0 : BitList(value, 12, "MOTA");
                }
                else if (option == L"--animations" && hasValue && retailT8) categories = RetailCategories(arguments[++i]);
                else if (option == L"--no-archive" && retailT8) settings |= ExtractSettings_T8_NO_ARCHIVE;
                else if (option == L"--unknown-motas") settings |= ExtractSettings_UnknownMotas;
                else if (option == L"--overwrite") settings |= ExtractSettings_OVERWRITE_SAME_FILENAME;
                else if (option == L"--no-movelist") settings &= ~ExtractSettings(ExtractSettings_DisplayableMovelist);
                else throw std::invalid_argument("Unknown or incomplete export option '" + Helpers::to_utf8(option) + "'" +
                    (retailT8 ? "" : " (--animations and --no-archive are retail T8 only)"));
            }
            settings |= compression | motas;
            if (retailT8) settings |= ExtractSettings_T8_PANM_SELECTION | (categories << 18);

            std::vector<uint8_t> slots;
            if (arguments[3] == L"all") for (uint8_t slot = 0; slot < info->characterCount; ++slot) slots.push_back(slot);
            else slots.push_back(PlayerSlot(arguments[3], info));

            const auto directory = std::filesystem::absolute(std::filesystem::path(arguments[2]));
            std::filesystem::create_directories(directory);
            const auto directoryUtf8 = directory.u8string();

            GameProcess process;
            if (!process.Attach(info->processName, 0)) throw std::runtime_error(std::string("Cannot attach to ") + info->processName);
            GameAddressesFile addresses; addresses.Reload();
            GameData game; game.SetProcess(&process, &addresses); game.SetCurrentGame(info);
            std::unique_ptr<Extractor> extractor(Games::FactoryGetExtractor(info, process, game));
            extractor->SetExtractionDirectory(reinterpret_cast<const char*>(directoryUtf8.c_str()));
            if (!extractor->CanExtract()) throw std::runtime_error("Nothing to extract: no character is loaded");

            json results = json::array(); bool ok = true;
            for (auto slot : slots) {
                const auto player = extractor->GetCharacterAddress(slot);
                const auto before = DirectoryFiles(directory);
                uint8_t progress = 0;
                const auto code = extractor->Extract(player, settings, progress);
                json files = json::array();
                for (const auto& file : DirectoryFiles(directory)) if (!before.contains(file)) files.push_back(Helpers::to_utf8(file.wstring()));
                // With --overwrite the file can already exist; report the path it replaced
                results.push_back({{"slot", slot}, {"ok", code == ExtractionErrcode_Successful}, {"code", code}, {"error", ExtractionErrorName(code)},
                    {"character", extractor->GetCharacterID(player)}, {"name", extractor->GetPlayerCharacterName(player)}, {"files", files}});
                ok = ok && code == ExtractionErrcode_Successful;
            }
            std::cout << json({{"ok", ok}, {"game", info->minorDataString}, {"directory", Helpers::to_utf8(directory.wstring())},
                {"exports", results}}).dump() << '\n';
            return ok ? 0 : 1;
        }
        if (command == L"import") { // the Import menu, for any game with an importer
            if (arguments.size() < 4) throw std::invalid_argument("Expected FILE GAME PLAYER_SLOT [options]");
            const auto* info = FindGame(arguments[2]);
            if (!info->importer) throw std::runtime_error(std::string(info->name) + " has no importer");
            const auto slot = PlayerSlot(arguments[3], info);
            // Same defaults as the Import menu: instant apply, free unused movesets, every MOTA
            ImportSettings settings = ImportSettings_ApplyInstantly | ImportSettings_FreeUnusedMovesets;
            uint16_t motas = ImportSettingsUtils::AllMotas;
            for (size_t i = 4; i < arguments.size(); ++i) {
                const auto& option = arguments[i];
                if (option == L"--no-instant") settings &= ~ImportSettings(ImportSettings_ApplyInstantly);
                else if (option == L"--keep-unused") settings &= ~ImportSettings(ImportSettings_FreeUnusedMovesets);
                else if (option == L"--motas" && i + 1 < arguments.size()) {
                    const auto& value = arguments[++i];
                    motas = static_cast<uint16_t>(value == L"all" ? ImportSettingsUtils::AllMotas : value == L"none" ? 0 : BitList(value, 12, "MOTA"));
                }
                else throw std::invalid_argument("Unknown or incomplete import option '" + Helpers::to_utf8(option) + "'");
            }
            settings = ImportSettingsUtils::WithMotaMask(settings, motas);

            if (!std::filesystem::is_regular_file(arguments[1])) throw std::runtime_error("Moveset file not found: " + Helpers::to_utf8(arguments[1]));
            MovesetFile file(arguments[1]); file.Validate();
            if (!info->SupportsGameImport(file.Header().gameId))
                throw std::runtime_error(std::string(info->name) + " cannot import movesets of game ID " + std::to_string(file.Header().gameId));

            GameProcess process;
            if (!process.Attach(info->processName, PROCESS_VM_OPERATION | PROCESS_VM_WRITE))
                throw std::runtime_error(std::string("Cannot attach to ") + info->processName);
            GameAddressesFile addresses; addresses.Reload();
            GameData game; game.SetProcess(&process, &addresses); game.SetCurrentGame(info);
            std::unique_ptr<Importer> importer(Games::FactoryGetImporter(info, process, game));
            if (!importer->CanImport()) throw std::runtime_error("Cannot import: no character is loaded");

            const auto player = importer->GetCharacterAddress(slot);
            const auto previous = importer->GetMovesetAddress(slot);
            uint8_t progress = 0;
            const auto code = importer->Import(arguments[1].c_str(), player, settings, progress);
            if (settings & ImportSettings_FreeUnusedMovesets) importer->CleanupUnusedMovesets();
            const bool ok = code == ImportationErrcode_Successful;
            std::cout << json({{"ok", ok}, {"game", info->minorDataString}, {"slot", slot}, {"code", code},
                {"error", ok ? std::string() : importer->LastError().empty() ? std::string(ImportationErrorName(code)) : importer->LastError()},
                {"source_game", file.Header().gameId}, {"source_character", file.Header().characterId},
                {"previous_moveset", previous}, {"moveset", ok ? importer->lastLoaded.address : 0},
                {"bytes", ok ? importer->lastLoaded.size : 0}}).dump() << '\n';
            return ok ? 0 : 1;
        }
        if (command == L"export-t8") {
            // Trailing "no-archive": animations the game has not loaded are recorded as missing instead of read from tkdata.bin.
            const bool noArchive = arguments.size() >= 4 && arguments.back() == L"no-archive";
            const auto count = arguments.size() - (noArchive ? 1 : 0);
            if ((count != 3 && count != 4) || arguments[2].size() != 1 || arguments[2][0] < L'0' || arguments[2][0] > L'1')
                throw std::invalid_argument("Expected DIRECTORY PLAYER_SLOT (0..1) [ANIMATION_CATEGORIES] [no-archive]");
            ExtractSettings selection=ExtractSettings_T8_PANM_SELECTION | ExtractSettings_T8_DEFAULT;
            if(count==4) {
                selection=ExtractSettings_T8_PANM_SELECTION;
                if(arguments[3]==L"all")selection|=ExtractSettings_T8_ALL;
                else if(arguments[3]==L"default")selection|=ExtractSettings_T8_DEFAULT;
                else if(arguments[3]!=L"none") {
                    constexpr const wchar_t* names[]={L"body",L"hands",L"face",L"swing",L"camera",L"extra"};
                    size_t at=0;
                    do {
                        auto end=arguments[3].find(L',',at);auto token=arguments[3].substr(at,end==std::wstring::npos?end:end-at);
                        bool found=false;for(unsigned i=0;i<6;++i)if(token==names[i]){selection|=1ULL<<(18+i);found=true;}
                        if(!found)throw std::invalid_argument("Unknown retail animation category");
                        if(end==std::wstring::npos)break;at=end+1;
                    }while(at<=arguments[3].size());
                }
            }
            if (!std::filesystem::create_directory(arguments[1])) throw std::runtime_error("Export directory must be new");
            struct Exporter : ExtractorT8 {
                std::string path;
                Exporter(GameProcess& p, GameData& g, const GameInfo* info, const std::wstring& directory)
                    : ExtractorT8(p, g, info), path(Helpers::to_utf8(directory)) { cm_extractionDir = path.c_str(); }
            };
            GameProcess process;
            if (!process.Attach("Polaris-Win64-Shipping.exe", 0)) throw std::runtime_error("Cannot attach read-only to retail T8");
            GameAddressesFile addresses; addresses.Reload(); GameData game; game.SetProcess(&process, &addresses);
            auto info = Games::GetGameInfoFromIdentifier(GameId_T8, GameVersions::T8::RETAIL); game.SetCurrentGame(info);
            Exporter exporter(process, game, info, arguments[1]);
            auto slot = static_cast<uint8_t>(arguments[2][0] - L'0'); auto player = exporter.GetCharacterAddress(slot);
            uint8_t progress = 0;
            if (noArchive) selection |= ExtractSettings_T8_NO_ARCHIVE;
            auto code = exporter.Extract(player, ExtractSettings_CompressCompactZstd | selection, progress);
            std::cout << json({{"ok", code == ExtractionErrcode_Successful}, {"code", code}, {"slot", slot},
                {"character", exporter.GetCharacterID(player)}, {"error", exporter.LastError()}}).dump() << '\n';
            return code == ExtractionErrcode_Successful ? 0 : 1;
        }
        if (command == L"import-t8" || command == L"check-import-t8" || command == L"preview-t8" || command == L"check-preview-t8" ||
            command == L"stage-t8") {
            // stage-t8: the full import with writes, except the player's pointers (benchmark/staging).
            const bool stage = command == L"stage-t8";
            const auto importStart = std::chrono::steady_clock::now();
            const bool preview = command == L"preview-t8" || command == L"check-preview-t8";
            const bool dryRun = command == L"check-import-t8" || command == L"check-preview-t8";
            if (arguments.size() != (preview ? 4 : 3) ||
                arguments[2].size() != 1 || arguments[2][0] < L'0' || arguments[2][0] > L'1')
                throw std::invalid_argument("Expected FILE PLAYER_SLOT (0..1); import uses only the moveset file");
            uint32_t previewMove=0;
            if(preview){size_t consumed=0;auto requested=std::stoul(arguments[3],&consumed,0);
                if(consumed!=arguments[3].size() || requested>UINT32_MAX)throw std::invalid_argument("Invalid preview move ID");previewMove=static_cast<uint32_t>(requested);}
            MovesetFile file(arguments[1]); file.Validate();
            if (file.Header().gameId != GameId_T7 && (file.Header().gameId != GameId_T8 || file.Header().minorVersion != GameVersions::T8::RETAIL))
                throw std::runtime_error("Expected a retail T8 snapshot or a T7 moveset");
            GameProcess process;
            if (!process.Attach("Polaris-Win64-Shipping.exe", dryRun ? 0 : PROCESS_VM_WRITE | PROCESS_VM_OPERATION))
                throw std::runtime_error("Cannot attach to running retail T8");
            GameAddressesFile addresses; addresses.Reload();
            GameData game; game.SetProcess(&process, &addresses);
            auto info = Games::GetGameInfoFromIdentifier(GameId_T8, GameVersions::T8::RETAIL);
            RetailImportShadow shadow(process);
            GameProcess& target = dryRun ? static_cast<GameProcess&>(shadow) : process;
            game.SetProcess(&target, &addresses);
            game.SetCurrentGame(info); ImporterT8 importer(target, game, info);
            auto slot = static_cast<uint8_t>(arguments[2][0] - L'0');
            auto player = importer.GetCharacterAddress(slot);
            auto before = importer.GetMovesetAddress(slot);
            auto targetCharacter = target.readUInt32(player + importer.Offsets().character);
            // T7 files: same self-contained conversion as the GUI import (same output for every target).
            std::vector<Byte> converted;
            if (file.Header().gameId == GameId_T7) {
                StructsT7::TKMovesetHeaderBlocks blocks;
                memcpy(&blocks, file.Bytes().data() + file.Header().block_list, sizeof(blocks));
                try { converted = importer.ConvertT7(file.Header(), blocks, file.Payload(), player); }
                catch (const std::exception& error) {
                    std::cout << json({{"ok",false},{"error",error.what()},{"process_attached",true}}).dump() << '\n';
                    return 1;
                }
            }
            const auto importBytes = converted.empty() ? file.Bytes() : std::span<const Byte>(converted);
            const auto& importHeader = *reinterpret_cast<const TKMovesetHeader*>(importBytes.data());
            const auto importPayload = importBytes.subspan(importHeader.moveset_data_start);
            uint8_t progress = 0;
            std::vector<Byte> stageRoots(40);  // player root + 4 contexts, which a stage must leave untouched
            if (stage && !process.readBytes(player + importer.Offsets().moveset, stageRoots.data(), stageRoots.size()))
                throw std::runtime_error("Cannot read the player's moveset roots");
            const auto importCall = std::chrono::steady_clock::now();
            auto result = importer.Import(importBytes.data(), importBytes.size(), player,
                stage ? ImportSettings_StageOnly : ImportSettings_ApplyInstantly, progress);
            const auto importEnd = std::chrono::steady_clock::now();
            json report{{"ok", result == ImportationErrcode_Successful}, {"dry_run", dryRun}, {"code", result}, {"error", importer.LastError()},
                {"slot", slot}, {"previous_moveset", before}, {"moveset", importer.lastLoaded.address},
                {"bytes", importer.lastLoaded.size}, {"progress", progress}, {"source_character", importHeader.characterId},
                {"target_character", targetCharacter}, {"binding_root", importer.BindingRoot()}, {"character_fixes", importer.CharacterFixes()}};
            report["custom_animations"] = importer.CustomAnimations();
            report["custom_animations_missing_carrier"] = importer.CustomAnimationsMissing();
            report["carrier_fallback_moves"] = importer.CarrierFallbackMoves();
            report["restored_carriers"] = importer.RestoredCarriers();
            report["memory_only"] = importer.MemoryOnly();
            report["published_animations"] = importer.PublishedAnimations();
            report["hand_animations"] = {{"carried", importer.HandAnimations()}, {"published_indices", importer.PublishedHandIndices()}, {"mapped", importer.HandIndices().size()}, {"silenced_rows", importer.SilencedHandProperties()}};
            report["reused_animations"] = importer.ReusedAnimations();
            report["memory_only_fallback_moves"] = importer.MemoryOnlyFallbackMoves();
            report["native_bound_animations"] = importer.NativeBoundAnimations();
            report["native_unloaded_kept"] = importer.NativeUnloadedKept();
            report["archive_animations"] = importer.ArchiveAnimations();
            report["dataless_animations"] = importer.DatalessAnimations().size();
            report["unavailable_animations"] = importer.UnavailableAnimations().size();
            if (!importer.ArchiveError().empty()) report["archive_error"] = importer.ArchiveError();
            report["binding_identity_mismatches"] = importer.BindingIdentityMismatches();
            report["binding_fallback_moves"] = importer.BindingFallbackMoves();
            report["dropped_external_cancels"] = importer.DroppedExternalCancels();
            report["target_fallback"] = importer.TargetFallback();
            report["fallback_cancel_lists"] = importer.FallbackCancelLists();
            report["edited_animations_kept"] = importer.EditedAnimationsKept();
            report["native_snapshot_v4"] = importer.Native().has_value();
            if(preview && result==ImportationErrcode_Successful) {
                report["preview_move"]=RetailT8Preview::Queue(target,game,*info,slot,importer.lastLoaded.address,previewMove);
                report["preview_next_pointer"]=target.readUInt64(player+importer.Offsets().nextMove);
                report["preview_next_id"]=target.readUInt32(player+importer.Offsets().nextMoveId);
            }
            if (dryRun) {
                report["live_root_unchanged"] = process.readUInt64(player + importer.Offsets().moveset) == before;
                report["shadow_published"] = shadow.readUInt64(player + importer.Offsets().moveset) == importer.lastLoaded.address && importer.lastLoaded.address;
                report["shadow_writes"] = shadow.writes;
                if (result == ImportationErrcode_Successful) {
                    auto metadata=RetailEditorData::Read(importHeader,importPayload);
                    RetailT8Import::MemoryOnlyAudit memoryOnly{importer.MemoryOnlyContainer(), importer.MemoryOnlyDomain(),
                        importer.MemoryOnlyStringRow(), importer.MemoryOnlyHandles(), importer.MemoryOnlyNames(), importer.MemoryOnlyAnimations(),
                        importer.Native() ? &importer.Native()->cancels : nullptr, &importer.DatalessAnimations()};
                    if (importer.MemoryOnly()) memoryOnly.compiledVtable = importer.Engine().compiledVtable; memoryOnly.handIndices = &importer.HandIndices();
                    const auto importProfile = ImportProfile::Read(importHeader);
                    auto audit = RetailT8Import::AuditImage(importPayload, shadow, importer.lastLoaded.address,
                        importer.lastLoaded.size, process, importer.BindingRoot(), importHeader.characterId, targetCharacter,
                        importHeader.GetConversionOrigin() == GameVersions::T8::FROM_T7,metadata.stored?&metadata:nullptr,
                        nullptr, importer.MemoryOnly() ? &memoryOnly : nullptr, &importProfile, importer.TargetSecondary(),
                        importer.TargetFallback() ? &importer.FallbackCancels() : nullptr);
                    report["image_audit_ok"] = true; report["audited_bytes"] = audit.bytes;
                    report["audited_animations"] = audit.animations;
                    report["external_cancel_lists"] = audit.externalLists;
                }
            } else if (stage) {
                std::vector<Byte> after(40);
                const bool read = process.readBytes(player + importer.Offsets().moveset, after.data(), after.size());
                report["player_roots_unchanged"] = read && after == stageRoots;
                report["process_alive"] = process.CheckRunning();
                json timings = json::object();
                for (const auto& [name, ms] : importer.Timings()) timings[name] = ms;
                timings["setup"] = std::chrono::duration<double, std::milli>(importCall - importStart).count();
                timings["import_total"] = std::chrono::duration<double, std::milli>(importEnd - importCall).count();
                report["timings_ms"] = timings;
            } else if (result == ImportationErrcode_Successful) {
                size_t observations = 0;
                auto end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
                while (std::chrono::steady_clock::now() < end && process.CheckRunning()) {
                    const auto move = process.readUInt64(player + importer.Offsets().currentMove);
                    if (move >= importer.lastLoaded.address && move - importer.lastLoaded.address < importer.lastLoaded.size) ++observations;
                    std::this_thread::sleep_for(std::chrono::milliseconds(16));
                }
                report["active_move_observations"] = observations;
                report["process_alive"] = process.CheckRunning();
                report["published"] = process.readUInt64(player + importer.Offsets().moveset) == importer.lastLoaded.address;
            }
            std::cout << report.dump() << '\n'; return result == ImportationErrcode_Successful ? 0 : 1;
        }
        if (command == L"compare-compiled-t8") {
            // Read-only diagnostic (.docs/retail-t8-memory-only-import.md, check L1): engine-compiled animations
            // of a player's animation map against RetailT8Resources::BuildCompiledAnimation for the same PANM.
            if (arguments.size() < 2 || arguments.size() > 3 || arguments[1].size() != 1 || arguments[1][0] < L'0' || arguments[1][0] > L'1')
                throw std::invalid_argument("Expected PLAYER_SLOT (0..1) [COUNT]");
            const size_t limit = arguments.size() == 3 ? std::stoul(arguments[2]) : 64;
            namespace RT = RetailT8Runtime; namespace RS = RetailT8Resources;
            GameProcess process;
            if (!process.Attach("Polaris-Win64-Shipping.exe", 0)) throw std::runtime_error("Cannot attach to running retail T8");
            GameAddressesFile addresses; addresses.Reload();
            GameData game; game.SetProcess(&process, &addresses);
            auto info = Games::GetGameInfoFromIdentifier(GameId_T8, GameVersions::T8::RETAIL);
            game.SetCurrentGame(info); ImporterT8 importer(process, game, info);
            const auto player = importer.GetCharacterAddress(static_cast<uint8_t>(arguments[1][0] - L'0'));
            if (!player) throw std::runtime_error("Retail player is not available");
            auto q = [&](gameAddr at) { return RT::Read<uint64_t>(process, at); };
            const auto root = q(player + importer.Offsets().moveset), count = q(root + 0x238);
            if (!count || count > 100000) throw std::runtime_error("Invalid move count");
            std::vector<Byte> moves(count * 0x448);
            if (!process.readBytes(q(root + 0x230), moves.data(), moves.size())) throw std::runtime_error("Cannot read moves");
            const auto domain = RT::AnimationCharacter(moves, RT::Read<uint32_t>(process, player + importer.Offsets().character));
            const auto container = game.ReadPtrPath("anim_container");
            const auto engine = importer.Engine();
            const auto map = RT::DomainMap(process, container, domain);
            if (!map) throw std::runtime_error("Player domain has no animation map");
            static constexpr gameAddr localBase = 0x10000;
            struct View {
                const GameProcess* process; const std::vector<Byte>* image;
                uint64_t Get(gameAddr at, size_t size) const { // zero-extended little-endian field
                    uint64_t value = 0;
                    if (process) { if (!process->readBytes(at, &value, size)) throw std::runtime_error("Unreadable compiled animation"); return value; }
                    if (at < localBase || at - localBase > image->size() || size > image->size() - (at - localBase))
                        throw std::runtime_error("Local image read out of range");
                    memcpy(&value, image->data() + (at - localBase), size); return value;
                }
                std::string String(gameAddr at) const {
                    size_t size; gameAddr text;
                    if ((Get(at + 23, 1) & 0x80)) { size = Get(at + 8, 8); text = Get(at, 8); }
                    else { size = 23 - Get(at + 23, 1); text = at; }
                    if (size > 4096) throw std::runtime_error("Invalid string");
                    std::string result(size, '\0');
                    for (size_t i = 0; i < size; ++i) result[i] = char(Get(text + i, 1));
                    return result;
                }
                void InOrder(gameAddr node, const std::function<void(gameAddr)>& visit, int depth = 0) const {
                    if (!node) return;
                    if (depth > 64) throw std::runtime_error("Tree too deep");
                    InOrder(Get(node + 8, 8), visit, depth + 1); visit(node); InOrder(Get(node, 8), visit, depth + 1);
                }
                // Canonical text of a compiled animation: scalars, sample offsets, maps and every tree entry.
                std::string Describe(gameAddr object, gameAddr panm, gameAddr sentinel) const {
                    std::ostringstream out;
                    out << "frames " << Get(object + 8, 4) << " fps " << Get(object + 0xC, 4)
                        << " f3 " << int(Get(object + 0x10, 1)) << " mode " << int(Get(object + 0x18, 1))
                        << " rate " << Get(object + 0x1C, 4)
                        << " base " << (Get(object + 0xB0, 8) - panm) << " data " << (Get(object + 0xD8, 8) - panm)
                        << " owner " << Get(object + 0xE0, 8) << "/" << Get(object + 0xE8, 8);
                    for (gameAddr at = 0xB8; at <= 0xD0; at += 8) out << " s" << at << "=" << Get(object + at, 8);
                    for (gameAddr hash : {object + 0x20, object + 0x50})
                        out << " hash(" << (Get(hash + 8, 8) == sentinel) << "," << Get(hash + 0x10, 8) << "," << Get(hash + 0x18, 8) << ")";
                    out << " groups " << Get(object + 0x80 + 0x20, 8);
                    InOrder(Get(object + 0x80 + 0x10, 8), [&](gameAddr group) {
                        out << "\n G" << int(Get(group + 0x20, 1)) << " bones " << Get(group + 0x28 + 0x20, 8);
                        InOrder(Get(group + 0x28 + 0x10, 8), [&](gameAddr bone) {
                            out << "\n  " << String(bone + 0x20) << " tracks " << Get(bone + 0x38 + 0x20, 8);
                            InOrder(Get(bone + 0x38 + 0x10, 8), [&](gameAddr track) {
                                const auto e = track + 0x38;
                                out << " [" << String(track + 0x20) << " t" << int(Get(e, 1)) << " e" << int(Get(e + 1, 1))
                                    << " n" << Get(e + 4, 4) << " f" << Get(e + 8, 4) << " o" << Get(e + 0xC, 4)
                                    << " z" << Get(e + 0x10, 4) << " k" << ((Get(e + 0x20, 8) - Get(e + 0x18, 8)) / 8) << "]";
                            });
                        });
                    });
                    return out.str();
                }
            };
            json mismatches = json::array();
            size_t compared = 0, matched = 0, skipped = 0, unsupported = 0;
            const auto head = q(map + 0x10);
            std::set<gameAddr> seen;
            for (auto node = q(head); node != head && compared < limit && seen.insert(node).second && seen.size() < 200000; node = q(node)) {
                const auto id = RT::Read<uint32_t>(process, node + 0x10);
                const auto ref = q(node + 0x18);
                const auto resource = ref ? q(ref + 8) : 0;
                const auto object = resource && q(resource) == engine.animResourceVtable ? q(resource + 0x190) : 0;
                if (!object || q(object) != engine.compiledVtable) { ++skipped; continue; }
                // The PANM the engine compiled from: its sample block is where the object's +0xB0 points.
                const auto panm = q(resource + 0x1A0);
                if (!panm || RT::Read<uint32_t>(process, panm + 4) != 0x4d4e4150) { ++skipped; continue; }
                std::vector<Byte> bytes;
                uint64_t size = 0;
                for (size_t length = 128; length <= (size_t(1) << 24) && !size; length *= 4) {
                    bytes.resize(length);
                    if (!process.probeBytes(panm, bytes.data(), bytes.size())) break;
                    try { size = RetailT8Snapshot::PanmSize(bytes); } catch (const std::runtime_error&) {}
                }
                if (!size) { ++skipped; continue; }
                bytes.resize(size);
                if (!process.readBytes(panm, bytes.data(), bytes.size())) { ++skipped; continue; }
                ++compared;
                try {
                    const auto built = RS::BuildCompiledAnimation(bytes, localBase, engine);
                    const auto native = View{&process, nullptr}.Describe(object, panm, engine.emptyBucket);
                    const auto local = View{nullptr, &built.image}.Describe(built.object, localBase, engine.emptyBucket);
                    if (native == local) { ++matched; continue; }
                    size_t at = 0; while (at < native.size() && at < local.size() && native[at] == local[at]) ++at;
                    const auto from = at > 80 ? at - 80 : 0;
                    if (mismatches.size() < 8) mismatches.push_back({{"id", id}, {"native", native.substr(from, 240)}, {"local", local.substr(from, 240)}});
                } catch (const std::exception& error) {
                    ++unsupported;
                    if (mismatches.size() < 8) mismatches.push_back({{"id", id}, {"error", error.what()}});
                }
            }
            std::cout << json({{"ok", compared && matched == compared}, {"domain", domain}, {"compared", compared}, {"matched", matched},
                {"skipped", skipped}, {"unsupported", unsupported}, {"mismatches", mismatches}}).dump() << '\n';
            return compared && matched == compared ? 0 : 1;
        }
        if (command == L"plan-import-t8") {
            if (arguments.size() < 2) throw std::invalid_argument("Expected at least one retail snapshot");
            json reports = json::array();
            bool valid = true;
            for (const auto& path : arguments.subspan(1)) {
                json report{{"file", Helpers::to_utf8(path)}, {"loadable_moveset", false}};
                try {
                    MovesetFile file(path); file.Validate();
                    auto plan = RetailT8Import::Prepare(file.Header(), file.Payload());
                    report.update({{"ok", plan.Ok()}, {"table_bytes", plan.tables.size()},
                        {"relocations", plan.relocations.size()}, {"issues_total", plan.issueCount},
                        {"issues", json::array()}, {"dependencies", json::object()},
                        {"available_panm", plan.availableAnimations}, {"unavailable_panm", plan.unavailableAnimations},
                        {"remaining_work", {"runtime resource binding", "animation registration", "player publication and lifetime management"}}});
                    for (const auto& dependency : plan.dependencies) {
                        auto& count = report["dependencies"][dependency.kind];
                        count = count.is_null() ? 1 : count.get<size_t>() + 1;
                    }
                    for (const auto& issue : plan.issues) report["issues"].push_back({
                        {"table", RetailT8Snapshot::Tables[issue.table].name}, {"record", issue.record},
                        {"field", issue.field}, {"source_address", issue.source},
                        {"target_table", RetailT8Snapshot::Tables[issue.targetTable].name}, {"reason", issue.reason}});
                    valid &= plan.Ok();
                } catch (const std::exception& error) {
                    report.update({{"ok", false}, {"error", error.what()}}); valid = false;
                }
                reports.push_back(std::move(report));
            }
            std::cout << reports.dump() << '\n'; return valid ? 0 : 1;
        }
        if (command == L"plan-convert-t8") {
            if (arguments.size() != 3) throw std::invalid_argument("Expected INPUT OUTPUT.json");
            const std::filesystem::path input(arguments[1]), output(arguments[2]);
            if (std::filesystem::absolute(input).lexically_normal() == std::filesystem::absolute(output).lexically_normal() ||
                (std::filesystem::exists(output) && std::filesystem::equivalent(input, output)))
                throw std::invalid_argument("Input and output must differ");
            MovesetFile file(input); file.Validate();
            if (file.Header().gameId != GameId_T7) throw std::invalid_argument("Expected T7 input");
            StructsT7::TKMovesetHeaderBlocks blocks;
            memcpy(&blocks, file.Bytes().data() + file.Header().block_list, sizeof(blocks));
            auto plan = T7ToRetailT8::Build(file.Header(), blocks, file.Payload());
            auto report = T7ToRetailT8::Report(plan);
            // This is a diagnostic artifact: valid T7 files with unsupported
            // mappings still produce a report, never a purported T8 moveset.
            if (!FileIO::WriteAtomically(output, report.dump(2, ' ', false, json::error_handler_t::replace) + "\n"))
                throw std::runtime_error("Cannot write T7 to retail T8 plan");
            std::cout << json({{"ok",plan.RecordsConverted()},{"loadable_moveset",false},
                {"issues_total",plan.issueCount}}).dump() << '\n';
            return plan.RecordsConverted() ? 0 : 1;
        }
        if (command == L"convert-records-t8") {
            if (arguments.size() != 3) throw std::invalid_argument("Expected INPUT OUTPUT.json");
            if ((std::filesystem::exists(arguments[2]) && std::filesystem::equivalent(arguments[1], arguments[2])) ||
                std::filesystem::absolute(arguments[1]).lexically_normal() == std::filesystem::absolute(arguments[2]).lexically_normal())
                throw std::invalid_argument("Source and output must differ");
            MovesetFile file(arguments[1]); file.Validate();
            if (file.Header().gameId != GameId_T7) throw std::runtime_error("Record converter requires T7 input");
            const auto bytes = file.Bytes(), payload = file.Payload();
            const auto& header = file.Header();
            const auto blocks = reinterpret_cast<const StructsT7::TKMovesetHeaderBlocks*>(bytes.data() + header.block_list);
            const auto info = reinterpret_cast<const StructsT7_gameAddr::MovesetInfo*>(payload.data());
            auto records = payload.data() + blocks->movesetBlock;
            auto result = RetailT8::ConvertRecords(
                {reinterpret_cast<const StructsT7::Requirement*>(records + info->table.requirement), static_cast<size_t>(info->table.requirementCount)},
                {reinterpret_cast<const StructsT7::ExtraMoveProperty*>(records + info->table.extraMoveProperty), static_cast<size_t>(info->table.extraMovePropertyCount)});
            json report{{"ok", result.Ok()}, {"format", "retail-t8-logical-records-v1"},
                {"loadable_moveset", false}, {"target_build", "unspecified"}, {"requirements", json::array()},
                {"properties", json::array()}, {"issues_total", result.issueCount}, {"issues", json::array()}};
            for (const auto& issue : result.issues) report["issues"].push_back({
                {"category", issue.property ? "extra_property" : "requirement"}, {"index", issue.index},
                {"id", issue.id}, {"parameter", issue.parameter}, {"reason", issue.reason}});
            if (!result.Ok()) { std::cout << report.dump() << '\n'; return 1; }
            for (const auto& row : result.requirements) report["requirements"].push_back({{"id", row.id}, {"parameters", row.parameters}});
            for (const auto& row : result.properties) report["properties"].push_back({
                {"frame", row.frame}, {"id", row.id}, {"guard", "unconditional"}, {"parameters", row.parameters}});
            if (!FileIO::WriteAtomically(arguments[2], report.dump(2) + "\n")) throw std::runtime_error("Cannot write converted records");
            std::cout << json({{"ok", true}, {"loadable_moveset", false}}).dump() << '\n';
            return 0;
        }
        if (command == L"animation-info") {
            if (arguments.size() < 2) throw std::invalid_argument("Expected an animation file");
            json reports = json::array();
            bool valid = true;
            for (const auto& path : arguments.subspan(1)) {
                s_animInfo info{};
                bool ok = TAnimUtils::FromFile::GetAnimationInfos(path.c_str(), info);
                reports.push_back({{"file", Helpers::to_utf8(path)}, {"ok", ok},
                    {"frames", info.duration}, {"bytes", info.size}, {"crc32", info.hash}});
                valid &= ok;
            }
            std::cout << reports.dump() << '\n';
            return valid ? 0 : 1;
        }
        if (command == L"list") {
            if (arguments.size() != 2) throw std::invalid_argument("Expected a moveset directory");
            LocalStorage storage(arguments[1]);
            storage.ReloadMovesetList();
            storage.CleanupUnusedMovesetInfos();
            json entries = json::array();
            for (const auto* entry : storage.extractedMovesets)
                entries.push_back({{"file", Helpers::to_utf8(entry->filename)}, {"valid", entry->is_valid},
                    {"game_id", entry->gameId}, {"editable", entry->editable},
                    {"character", entry->target_character}, {"original_character", entry->original_character}});
            std::cout << entries.dump() << '\n';
            return 0;
        }
#ifdef TKM_EDITOR_FORM_TESTS
        if(command==L"retail-editor-render") {
            if(arguments.size()!=3 && !(arguments.size()==4 && (arguments[3]==L"references" || arguments[3]==L"lists")))throw std::invalid_argument("retail-editor-render takes INPUT OUTPUT.json [references|lists]");
            return RenderRetailEditorFile(arguments[1],arguments[2],arguments.size()==4 && arguments[3]==L"references",arguments.size()==4 && arguments[3]==L"lists");
        }
        if(command==L"retail-validation-audit"){
            if(arguments.size()<2)throw std::invalid_argument("retail-validation-audit takes FILE|FOLDER...");
            return AuditRetailEditorValidation(arguments.subspan(1));
        }
        if(command==L"retail-editor-ui-test") {
            // Dear ImGui Test Engine drives the retail editor on a synthetic document (tests/RetailEditorTests.cpp).
            if(arguments.size()!=1)throw std::invalid_argument("retail-editor-ui-test takes no arguments");
            return RunRetailEditorUiTests();
        }
        if(command==L"anim-preview-check"){
            if(arguments.size()!=2)throw std::invalid_argument("anim-preview-check takes REFERENCE.json");
            return CheckAnimationPreview(arguments[1]);
        }
        if(command==L"anim-preview-shot")return ShootAnimationPreview(arguments.subspan(1));
        if(command==L"anim-preview-scan"){if(arguments.size()!=2)throw std::invalid_argument("anim-preview-scan takes FILE");return ScanAnimationPreview(arguments[1]);}
        if(command==L"anim-preview-export")return ExportAnimationPreviewSource(arguments.subspan(1));
        if(command==L"anim-preview-joints"){
            if(arguments.size()!=3)throw std::invalid_argument("anim-preview-joints takes ANIMATION OUT.json");
            return DumpAnimationPreviewJoints(arguments[1],arguments[2]);
        }
        if(command==L"retail-panm-fixture"){
            if(arguments.size()!=2)throw std::invalid_argument("retail-panm-fixture takes OUTPUT_DIRECTORY");
            return WriteRetailPanmFixture(arguments[1]);
        }
        if(command==L"retail-editor-structure-smoke"){
            if(arguments.size()!=3)throw std::invalid_argument("retail-editor-structure-smoke takes INPUT OUTPUT");
            return VerifyRetailEditorStructure(arguments[1],arguments[2]);
        }
        if (command == L"retail-editor-smoke") {
            if (arguments.size()!=3) throw std::invalid_argument("retail-editor-smoke takes INPUT OUTPUT");
            return VerifyRetailEditorFile(arguments[1],arguments[2]);
        }
        if (command == L"editor-smoke") {
            if (arguments.size() != 1) throw std::invalid_argument("editor-smoke takes no arguments");
            RunEditorFormTests();
            RunRetailEditorTests();
            std::cout << "{\"ok\":true,\"suite\":\"editor-forms\"}\n";
            return 0;
        }
#endif
        if (command == L"tag-t8") {
            // Tools > Extra's button without the GUI: inject the loader, send the Tag command, print the loader's status
            // lines for SECONDS. P3/P4 are runtime fighter ids; P4 "none" = P2 alone; "off" = Disable.
            if (arguments.size() < 2) throw std::invalid_argument("Expected P3 P4|none [SECONDS] or off [SECONDS] or trigger A|B");
            if (arguments[1] == L"trigger") {
                // a tag request for team A or B, as Ki Charge does (automation, tests); the loader must be running
                if (arguments.size() != 3 || (arguments[2] != L"A" && arguments[2] != L"B")) throw std::invalid_argument("Expected trigger A|B");
                GameAddressesFile addresses(false);
                GameProcess process;
                if (!process.Attach("Polaris-Win64-Shipping.exe", PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD))
                    throw std::runtime_error("Cannot attach to retail T8");
                const GameInfo* info = Games::GetGameInfoFromIdentifier(GameId_T8, GameVersions::T8::RETAIL);
                GameData game; game.SetProcess(&process, &addresses); game.SetCurrentGame(info);
                OnlineT8 loader(process, game, info);
                if (!loader.LoadSharedMemory() || !loader.SharedMemory()) throw std::runtime_error("The loader is not running in the game");
                auto mem = loader.SharedMemory();
                mem->tag_trigger_team = arguments[2] == L"A" ? 0 : 1;
                MemoryBarrier();
                mem->tag_trigger_id = mem->tag_trigger_id + 1;
                std::cout << json({{"ok", true}, {"team", Helpers::to_utf8(arguments[2])}}).dump() << '\n';
                return 0;
            }
            const bool off = arguments[1] == L"off";
            if (!off && arguments.size() < 3) throw std::invalid_argument("Expected P3 P4|none [SECONDS]");
            const int p3 = off ? -1 : std::stoi(arguments[1]);
            const int p4 = off || arguments[2] == L"none" ? -1 : std::stoi(arguments[2]);
            const size_t secIndex = off ? 2 : 3;
            const int seconds = arguments.size() > secIndex ? std::stoi(arguments[secIndex]) : 5;
            GameAddressesFile addresses(true);    // the loader reads its addresses from this shared copy at init
            GameProcess process;
            if (!process.Attach("Polaris-Win64-Shipping.exe", PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD))
                throw std::runtime_error("Cannot attach to retail T8");
            const GameInfo* info = Games::GetGameInfoFromIdentifier(GameId_T8, GameVersions::T8::RETAIL);
            GameData game; game.SetProcess(&process, &addresses); game.SetCurrentGame(info);
            OnlineT8 loader(process, game, info);
            // Always through Init/Run: Init answers AlreadyRunning when the main loop runs, else Run starts it (a loader
            // whose Init once failed leaves its shared memory without a main loop, 2026-11-14 22:12)
            const bool hadMemory = loader.LoadSharedMemory();
            bool injected = loader.InjectDllAndWaitEnd();
            if (!hadMemory && !loader.LoadSharedMemory()) {
                // the first injection into a freshly started game has failed without a trace (2026-11-14): once more
                injected = loader.InjectDllAndWaitEnd();
                loader.LoadSharedMemory();
            }
            auto mem = loader.SharedMemory();
            if (!mem) throw std::runtime_error("Loader shared memory not found (injection failed)");
            if (loader.VerifyDllVersionMismatch()) throw std::runtime_error("The game holds a loader from another version");
            for (int i = 0; i < 100 && mem->moveset_loader_version[0] == '\0'; ++i) Sleep(50);
            mem->tag_command = off ? TagCommand_Disable : TagCommand_Enable;
            mem->tag_p3_fighter = p3; mem->tag_p4_fighter = p4;
            MemoryBarrier();
            const int request = mem->tag_request_id + 1;
            mem->tag_request_id = request;
            std::string last;
            json lines = json::array();
            for (int t = 0; t < seconds * 10; ++t) {
                Sleep(100);
                if (mem->tag_ack_id != request) continue;
                std::string status(mem->tag_status, strnlen(mem->tag_status, sizeof(mem->tag_status)));
                if (status != last) { last = status; lines.push_back(status); std::cerr << status << '\n'; }
            }
            std::cout << json({{"ok", mem->tag_ack_id == request && mem->tag_state != TagState_Error}, {"injected_now", injected},
                {"state", mem->tag_state}, {"status", lines}}).dump() << '\n';
            return mem->tag_state == TagState_Error ? 1 : 0;
        }
        if (command == L"ui-smoke") {
            if (arguments.size() != 1) throw std::invalid_argument("ui-smoke takes no arguments");
            const auto frames = UiSmoke();
            std::cout << json({{"ok", true}, {"frames", frames}}).dump() << '\n';
            return 0;
        }
        if (command == L"inspect" || command == L"validate") {
            if (arguments.size() < 2) throw std::invalid_argument("Expected at least one file");
            json reports = json::array();
            bool valid = true;
            for (auto& argument : arguments.subspan(1)) {
                json report{{"file", Helpers::to_utf8(argument)}};
                try {
                    MovesetFile file(argument);
                    if (command == L"validate") file.Validate();
                    const auto& header = file.Header();
                    report.update({{"ok", true}, {"game_id", header.gameId}, {"minor_version", header.minorVersion},
                        {"origin", header.origin}, {"character", header.target_character}, {"payload_bytes", file.Payload().size()},
                        {"compression", file.OriginalCompression()}});
                } catch (const std::exception& error) {
                    report.update({{"ok", false}, {"error", error.what()}});
                    valid = false;
                }
                reports.push_back(std::move(report));
            }
            std::cout << reports.dump() << '\n';
            return valid ? 0 : 1;
        }
        if (command == L"live-edit-t8") {
            // Direct live edit: PUBLISHED is the file currently imported on the slot, EDITED the changed document.
            if (arguments.size() != 4 || arguments[1].size() != 1 || arguments[1][0] < L'0' || arguments[1][0] > L'1')
                throw std::invalid_argument("Expected PLAYER_SLOT PUBLISHED_FILE EDITED_FILE");
            const auto slot = static_cast<uint8_t>(arguments[1][0] - L'0');
            MovesetFile published(arguments[2]), edited(arguments[3]); published.Validate(); edited.Validate();
            GameProcess process;
            if (!process.Attach("Polaris-Win64-Shipping.exe", PROCESS_VM_WRITE | PROCESS_VM_OPERATION))
                throw std::runtime_error("Cannot attach to running retail T8");
            GameAddressesFile addresses; addresses.Reload();
            GameData game; game.SetProcess(&process, &addresses);
            auto info = Games::GetGameInfoFromIdentifier(GameId_T8, GameVersions::T8::RETAIL); game.SetCurrentGame(info);
            auto state = RetailT8Preview::Read(process, game, *info, slot);
            ImporterT8 importer(process, game, info);
            auto result = RetailLiveEdit::Sync(process, importer.GetCharacterAddress(slot), state.root, published.Bytes(), edited.Bytes(), importer.Offsets());
            std::cout << json({{"ok", !result.structural}, {"structural", result.structural}, {"reason", result.reason},
                {"records", result.records}, {"bytes", result.bytes}, {"root", state.root}}).dump() << '\n';
            return result.structural ? 1 : 0;
        }
        if (command == L"live-verify-t8") {
            // End-to-end check of the editor's live-editing path on the running game:
            // open DOC in the retail editor, import it on SLOT (Import button), then apply each EDIT
            // through the editor's forms and publish it with RetailLiveEdit::Apply (what the
            // Live editing checkbox runs). After every step the published image is audited
            // against a fresh import of the edited document.
            // EDIT: TABLE:ROW:FIELD=VALUE | anim:ROW=DONOR_ROW | rename:ROW=NAME | rows:TABLE:FIRST=ID,... | delete:TABLE:ROW=COUNT | revert
            if (arguments.size() < 3 || arguments[1].size() != 1 || arguments[1][0] < L'0' || arguments[1][0] > L'1')
                throw std::invalid_argument("Expected PLAYER_SLOT DOC [EDIT...]");
            const auto slot = static_cast<uint8_t>(arguments[1][0] - L'0');
            MovesetFile file(arguments[2]); file.Validate();
            if (file.Header().gameId != GameId_T8 || file.Header().minorVersion != GameVersions::T8::RETAIL)
                throw std::invalid_argument("DOC must be a retail T8 document (convert T7 files first)");
            GameProcess process;
            if (!process.Attach("Polaris-Win64-Shipping.exe", PROCESS_VM_WRITE | PROCESS_VM_OPERATION))
                throw std::runtime_error("Cannot attach to running retail T8");
            GameAddressesFile addresses; addresses.Reload();
            GameData game; game.SetProcess(&process, &addresses);
            auto info = Games::GetGameInfoFromIdentifier(GameId_T8, GameVersions::T8::RETAIL); game.SetCurrentGame(info);
            GameProcess offline; GameData offlineData;
            EditorT8Retail editor(offline, offlineData, info);
            auto load = [&](std::span<const Byte> bytes) {
                auto* copy = static_cast<Byte*>(malloc(bytes.size())); if (!copy) throw std::bad_alloc();
                memcpy(copy, bytes.data(), bytes.size());
                if (!editor.LoadMoveset(copy, bytes.size())) throw std::runtime_error("Editor rejected the document");
            };
            load(file.Bytes());
            ImporterT8 importer(process, game, info);
            const auto player = importer.GetCharacterAddress(slot);
            const auto targetCharacter = process.readUInt32(player + importer.Offsets().character);
            auto original = std::make_shared<std::vector<Byte>>(editor.BuildSaveSnapshot());
            auto published = RetailLiveEdit::Apply(importer, process, slot, 0, nullptr, *original);
            gameAddr root = published.root, bindingRoot = importer.BindingRoot(); uint64_t size = importer.lastLoaded.size;
            auto snapshot = original;
            json steps = json::array(); bool ok = true;
            // Expected body handle per move for the memory-only audit; an animation swap written in place
            // takes the donor row's handle, so it is tracked here between re-imports.
            auto handles = importer.MemoryOnlyHandles();
            auto audit = [&](json& step) {
                const auto& header = *reinterpret_cast<const TKMovesetHeader*>(snapshot->data());
                const auto payload = std::span<const Byte>(*snapshot).subspan(header.moveset_data_start);
                auto metadata = RetailEditorData::Read(header, payload);
                try {
                    // Same audit inputs as check-import-t8: memory-only imports (converted T7, v4 native) rebind the
                    // engine-bound move fields, so the audit needs the importer's memory-only state.
                    RetailT8Import::MemoryOnlyAudit memoryOnly{importer.MemoryOnlyContainer(), importer.MemoryOnlyDomain(),
                        importer.MemoryOnlyStringRow(), handles, importer.MemoryOnlyNames(), importer.MemoryOnlyAnimations(),
                        importer.Native() ? &importer.Native()->cancels : nullptr, &importer.DatalessAnimations()};
                    if (importer.MemoryOnly()) memoryOnly.compiledVtable = importer.Engine().compiledVtable; memoryOnly.handIndices = &importer.HandIndices();
                    const auto headerProfile = ImportProfile::Read(header);
                    auto result = RetailT8Import::AuditImage(payload, process, root, size, process, bindingRoot, header.characterId, targetCharacter,
                        header.GetConversionOrigin() == GameVersions::T8::FROM_T7, metadata.stored ? &metadata : nullptr, &importer.MissingCarriers(),
                        importer.MemoryOnly() ? &memoryOnly : nullptr, &headerProfile, importer.TargetSecondary(),
                        importer.TargetFallback() ? &importer.FallbackCancels() : nullptr);
                    step["audit_ok"] = true; step["audited_bytes"] = result.bytes;
                } catch (const std::exception& error) { step["audit_ok"] = false; step["audit_error"] = error.what(); ok = false; }
                const bool installed = process.readUInt64(player + importer.Offsets().moveset) == root, alive = process.CheckRunning();
                step["installed"] = installed; step["process_alive"] = alive;
                if (!installed || !alive) ok = false;
            };
            { json step{{"edit", "import"}, {"root", root}, {"bytes", size}}; audit(step); steps.push_back(step); }
            for (auto& argument : arguments.subspan(3)) {
                const auto edit = Helpers::to_utf8(argument);
                json step{{"edit", edit}};
                try {
                    const auto equals = edit.find('=');
                    const auto head = edit.substr(0, equals), value = equals == std::string::npos ? std::string() : edit.substr(equals + 1);
                    std::shared_ptr<std::vector<Byte>> next;
                    if (edit == "revert") next = original;
                    else {
                        if (equals == std::string::npos) throw std::invalid_argument("EDIT needs =VALUE");
                        if (head.rfind("anim:", 0) == 0) {
                            const auto row = std::stoul(head.substr(5), nullptr, 0), donor = std::stoul(value, nullptr, 0);
                            editor.SetAnimation(row, donor);
                            if (row < handles.size() && donor < handles.size()) handles[row] = handles[donor];
                        }
                        else if (head.rfind("rename:", 0) == 0) editor.RenameMove(std::stoul(head.substr(7), nullptr, 0), value);
                        else if (head.rfind("delete:", 0) == 0) {
                            // delete:TABLE:ROW=COUNT -> what the record/list form's Delete does (COUNT 0 = the whole list at ROW).
                            const auto colon = head.find(':', 7);
                            if (colon == std::string::npos) throw std::invalid_argument("EDIT must be delete:TABLE:ROW=COUNT");
                            const auto table = std::stoul(head.substr(7, colon - 7), nullptr, 0);
                            const auto row = static_cast<uint32_t>(std::stoul(head.substr(colon + 1), nullptr, 0));
                            auto count = std::stoul(value, nullptr, 0); if (!count) count = editor.ListSize(table, row);
                            step["deleted"] = count;
                            editor.DeleteStructures(EditorT8Retail::Type(table), row, count);
                        }
                        else if (head.rfind("rows:", 0) == 0) {
                            // rows:TABLE:FIRST=ID,ID,... -> typed reorder (EditRows), e.g. rows:14:64=65,64 swaps two input sequences.
                            const auto colon = head.find(':', 5);
                            if (colon == std::string::npos) throw std::invalid_argument("EDIT must be rows:TABLE:FIRST=ID,...");
                            std::vector<int> ids; for (size_t at = 0; at <= value.size();) { auto comma = value.find(',', at); if (comma == std::string::npos) comma = value.size(); ids.push_back(std::stoi(value.substr(at, comma - at), nullptr, 0)); at = comma + 1; }
                            editor.EditRows(std::stoul(head.substr(5, colon - 5), nullptr, 0), static_cast<uint32_t>(std::stoul(head.substr(colon + 1), nullptr, 0)), ids, {});
                        }
                        else {
                            const auto first = head.find(':'), second = head.find(':', first + 1);
                            if (first == std::string::npos || second == std::string::npos) throw std::invalid_argument("EDIT must be TABLE:ROW:FIELD=VALUE");
                            const auto table = std::stoul(head.substr(0, first), nullptr, 0);
                            const auto row = static_cast<uint32_t>(std::stoul(head.substr(first + 1, second - first - 1), nullptr, 0));
                            const auto name = head.substr(second + 1);
                            VectorSet<std::string> order; auto fields = editor.RecordFields(table, row, order);
                            EditorInput* input = nullptr;
                            try { input = fields.at(name); } catch (const std::exception&) {}
                            if (!input) {
                                std::string known; for (const auto& field : order) known += field + " ";
                                throw std::invalid_argument("Unknown field " + name + "; fields: " + known);
                            }
                            step["before"] = std::string(input->buffer);
                            // QueueValue refuses a value that does not fit the field (strcpy_s would abort the process
                            // with the document already imported); the step then reports the error.
                            input->QueueValue(value); input->ConsumeNextValue();
                            editor.SaveRecord(table, row, fields);
                            order.clear(); auto after = editor.RecordFields(table, row, order);
                            step["after"] = std::string(after.at(name)->buffer);
                        }
                        next = std::make_shared<std::vector<Byte>>(editor.BuildSaveSnapshot());
                    }
                    const auto start = std::chrono::steady_clock::now();
                    auto result = RetailLiveEdit::Apply(importer, process, slot, root, snapshot.get(), *next);
                    step["ms"] = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                    step["direct"] = result.direct; step["records"] = result.sync.records; step["bytes"] = result.sync.bytes;
                    if (result.sync.structural) step["reimport_reason"] = result.sync.reason;
                    if (!result.direct) { root = result.root; bindingRoot = importer.BindingRoot(); size = importer.lastLoaded.size; step["root"] = root; handles = importer.MemoryOnlyHandles(); }
                    snapshot = next;
                    if (edit == "revert") load(*original); // later edits start from the original document
                    audit(step);
                } catch (const std::exception& error) { step["error"] = error.what(); ok = false; }
                steps.push_back(step);
            }
            std::cout << json({{"ok", ok}, {"slot", slot}, {"steps", steps}}).dump(1) << '\n';
            return ok ? 0 : 1;
        }
        if (command == L"queue-t8" || command == L"state-t8") {
            // Queue a move (index or 0x80xx alias) on a player's current moveset, or read its state.
            if (arguments.size() != (command == L"queue-t8" ? 3 : 2) || arguments[1].size() != 1 || arguments[1][0] < L'0' || arguments[1][0] > L'1')
                throw std::invalid_argument("Expected PLAYER_SLOT (0..1) [MOVE_ID]");
            const auto slot = static_cast<uint8_t>(arguments[1][0] - L'0');
            GameProcess process;
            if (!process.Attach("Polaris-Win64-Shipping.exe", command == L"queue-t8" ? PROCESS_VM_WRITE | PROCESS_VM_OPERATION : 0))
                throw std::runtime_error("Cannot attach to running retail T8");
            GameAddressesFile addresses; addresses.Reload();
            GameData game; game.SetProcess(&process, &addresses);
            auto info = Games::GetGameInfoFromIdentifier(GameId_T8, GameVersions::T8::RETAIL); game.SetCurrentGame(info);
            auto state = RetailT8Preview::Read(process, game, *info, slot);
            json report{{"ok", true}, {"slot", slot}, {"root", state.root}, {"moves", state.count}, {"character", state.character},
                {"current", state.current ? json(*state.current) : json(nullptr)}};
            if (command == L"queue-t8") {
                size_t consumed = 0; auto requested = std::stoul(arguments[2], &consumed, 0);
                if (consumed != arguments[2].size() || requested > UINT32_MAX) throw std::invalid_argument("Invalid move ID");
                report["queued"] = RetailT8Preview::Queue(process, game, *info, slot, state.root, static_cast<uint32_t>(requested));
            }
            std::cout << report.dump() << '\n';
            return 0;
        }
        if (command == L"dump-t7-anims") {
            // Research: every distinct body animation of a T7 moveset as <offset>.f32 (u32 frames, u32 bones,
            // u16 descriptors[bones], pad to 4, then frames*bones*3 float32) + index.json (move -> offset, name).
            if (arguments.size() != 3) throw std::invalid_argument("Expected T7_FILE OUTPUT_DIRECTORY");
            MovesetFile file(arguments[1]); file.Validate();
            if (file.Header().gameId != GameId_T7) throw std::invalid_argument("Input must be a T7 moveset");
            StructsT7::TKMovesetHeaderBlocks blocks;
            memcpy(&blocks, file.Bytes().data() + file.Header().block_list, sizeof(blocks));
            const auto payload = file.Payload();
            const auto& info = *reinterpret_cast<const StructsT7_gameAddr::MovesetInfo*>(payload.data() + blocks.movesetInfoBlock);
            const auto anims = payload.subspan(blocks.animationBlock, blocks.motaBlock - blocks.animationBlock);
            std::filesystem::create_directories(arguments[2]);
            json index = json::array(); std::set<uint64_t> done; size_t written = 0, failed = 0;
            for (uint64_t row = 0; row < info.table.moveCount; ++row) {
                const auto& move = *reinterpret_cast<const StructsT7_gameAddr::Move*>(payload.data() + blocks.movesetBlock + info.table.move + row * sizeof(StructsT7_gameAddr::Move));
                const auto offset = static_cast<uint64_t>(move.anim_addr);
                const char* name = reinterpret_cast<const char*>(payload.data() + blocks.nameBlock + move.name_addr);
                const char* anim = reinterpret_cast<const char*>(payload.data() + blocks.nameBlock + move.anim_name_addr);
                index.push_back({{"row", row}, {"name", name}, {"anim", anim}, {"offset", offset}});
                if (offset >= anims.size() || !done.insert(offset).second) continue;
                try {
                    auto bytes = anims.subspan(offset);
                    auto decoded = RetailPanm::DecodeT7(bytes);
                    auto layout = TAnimUtils::ReadAnimationLayout(bytes);
                    std::string out(8, '\0'); uint32_t head[2] = {decoded.frames, decoded.channels}; memcpy(out.data(), head, 8);
                    for (uint16_t b = 0; b < decoded.channels; ++b) {
                        uint16_t d = 0; if (layout.type == 0xc8) d = static_cast<uint16_t>(bytes[8 + b * 4]); else memcpy(&d, bytes.data() + 4 + b * 2, 2);
                        out.append(reinterpret_cast<const char*>(&d), 2);
                    }
                    while (out.size() % 4) out.push_back('\0');
                    out.append(reinterpret_cast<const char*>(decoded.values.data()), decoded.values.size() * 4);
                    if (!FileIO::WriteAtomically(std::filesystem::path(arguments[2]) / (std::to_string(offset) + ".f32"), out))
                        throw std::system_error(std::make_error_code(std::errc::io_error), "Cannot write animation");
                    ++written;
                } catch (const std::system_error&) { throw; } // Output failure: the whole command fails
                catch (const std::exception&) { ++failed; } // Undecodable animation: counted
            }
            if (!FileIO::WriteAtomically(std::filesystem::path(arguments[2]) / "index.json", index.dump()))
                throw std::runtime_error("Cannot write index.json");
            std::cout << json({{"ok", true}, {"animations", written}, {"failed", failed}}).dump() << '\n';
            return 0;
        }
        if (command == L"panm-reencode") {
            // Diagnostic: decode a raw PANM and write it back with the independent encoder (RetailPanm::Encode),
            // to compare its layout with the native file (see .docs/retail-t8-panm-layout.md).
            if (arguments.size() != 3) throw std::invalid_argument("Expected PANM_FILE OUTPUT.panm");
            if (SameFile(arguments[1], arguments[2])) throw std::invalid_argument("Output must differ from the input");
            auto encoded = RetailPanm::Encode(RetailPanm::DecodeRaw(EditorT8Retail::ReadPanmFile(arguments[1])));
            if (!FileIO::WriteAtomically(arguments[2], encoded)) throw std::runtime_error("Cannot write PANM");
            std::cout << json({{"ok", true}, {"bytes", encoded.size()}}).dump() << '\n';
            return 0;
        }
        if (command == L"panm-to-t7") {
            // Raw/static retail body PANM -> Tekken 7 0xC8 (per-frame) or 0x64 (keyframed) body animation.
            if (arguments.size() != 4 || (arguments[3] != L"c8" && arguments[3] != L"64"))
                throw std::invalid_argument("Expected PANM_FILE OUTPUT_FILE c8|64");
            if (SameFile(arguments[1], arguments[2])) throw std::invalid_argument("Output must differ from the input");
            const auto format = arguments[3] == L"64" ? RetailT8AnimToT7::Format::Keyframed : RetailT8AnimToT7::Format::Frames;
            const auto bytes = RetailT8AnimToT7::Convert(EditorT8Retail::ReadPanmFile(arguments[1]), format);
            if (!FileIO::WriteAtomically(arguments[2], bytes)) throw std::runtime_error("Cannot write animation");
            const auto layout = TAnimUtils::ReadAnimationLayout(bytes);
            std::cout << json({{"ok", true}, {"bytes", bytes.size()}, {"frames", layout.frames}, {"type", layout.type}}).dump() << '\n';
            return 0;
        }
        if (command == L"panm-dump") {
            // Raw, static or packed-track PANM -> JSON {frames, bones: {name: [[sx,sy,sz,qx,qy,qz,qw,px,py,pz],...]}}
            if (arguments.size() != 3) throw std::invalid_argument("Expected PANM_FILE OUTPUT.json");
            std::ifstream in(arguments[1], std::ios::binary);
            std::vector<Byte> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            auto anim = RetailPanm::Decode(bytes);
            json bones = json::object();
            for (const auto& track : anim.tracks) {
                json rows = json::array();
                for (const auto& s : track.samples) rows.push_back({s.scale[0], s.scale[1], s.scale[2], s.rotation[0], s.rotation[1], s.rotation[2], s.rotation[3], s.translation[0], s.translation[1], s.translation[2]});
                bones[track.name] = rows;
            }
            if (!FileIO::WriteAtomically(arguments[2], json({{"frames", anim.frames}, {"bones", bones}}).dump()))
                throw std::runtime_error("Cannot write output file");
            std::cout << json({{"ok", true}, {"frames", anim.frames}, {"tracks", anim.tracks.size()}}).dump() << '\n';
            return 0;
        }
        if (command == L"panm-groups-reencode") { // research: DecodeGroups then EncodeGroups, for layout diffs
            if (arguments.size() != 3 || SameFile(arguments[1], arguments[2])) throw std::invalid_argument("Expected PANM_FILE OUTPUT.panm");
            std::ifstream in(std::filesystem::path(arguments[1]), std::ios::binary);
            std::vector<Byte> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            uint32_t frames = 0; const auto tracks = RetailPanm::DecodeGroups(bytes, &frames);
            if (!FileIO::WriteAtomically(arguments[2], RetailPanm::EncodeGroups(frames, tracks))) throw std::runtime_error("Cannot write PANM");
            std::cout << json({{"ok", true}, {"tracks", tracks.size()}}).dump() << '\n'; return 0;
        }
        if (command == L"panm-groups-check") {
            // Research: every *.panm under DIRECTORY with more than one bone group (native cameras) decoded with
            // DecodeGroups and re-encoded with EncodeGroups (byte comparison), and each packed track re-packed with
            // PackTransforms / PackScalars (worst value difference after a decode of both).
            if (arguments.size() != 2 && !(arguments.size() == 3 && arguments[2] == L"all")) throw std::invalid_argument("Expected DIRECTORY [all]");
            const bool everyFile = arguments.size() == 3; // also single-group files
            size_t files = 0, identical = 0, tracks = 0; double worstRotation = 0, worstPosition = 0, worstScalar = 0;
            json mismatches = json::array();
            for (const auto& entry : std::filesystem::recursive_directory_iterator(arguments[1])) {
                if (!entry.is_regular_file() || entry.path().extension() != ".panm") continue;
                std::ifstream in(entry.path(), std::ios::binary);
                std::vector<Byte> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                uint32_t frames = 0; std::vector<RetailPanm::GroupTrack> decoded;
                try { decoded = RetailPanm::DecodeGroups(bytes, &frames); } catch (const std::exception&) { continue; }
                std::set<uint32_t> groups; for (const auto& t : decoded) groups.insert(t.group);
                if (groups.size() < 2 && !everyFile) continue;
                ++files;
                if (RetailPanm::EncodeGroups(frames, decoded) == bytes) ++identical;
                else if (mismatches.size() < 10) mismatches.push_back(entry.path().filename().string());
                for (const auto& t : decoded) {
                    if (t.encoding != 3) continue;
                    ++tracks;
                    if (t.type == 1) {
                        const auto a = RetailPanm::DecodePackedTrack(t.data, t.samples);
                        const auto b = RetailPanm::DecodePackedTrack(RetailPanm::PackTransforms(a), t.samples);
                        for (size_t i = 0; i < a.size(); ++i) {
                            double dot = 0; for (int k = 0; k < 4; ++k) dot += double(a[i].rotation[k]) * b[i].rotation[k];
                            worstRotation = (std::max)(worstRotation, 2 * std::acos((std::min)(1.0, std::abs(dot))));
                            for (int k = 0; k < 3; ++k) worstPosition = (std::max)(worstPosition,
                                (std::max)(std::abs(double(a[i].translation[k]) - b[i].translation[k]), std::abs(double(a[i].scale[k]) - b[i].scale[k])));
                        }
                    } else {
                        const auto a = RetailPanm::UnpackScalars(t.data, t.samples);
                        const auto b = RetailPanm::UnpackScalars(RetailPanm::PackScalars(a), t.samples);
                        for (size_t i = 0; i < a.size(); ++i) worstScalar = (std::max)(worstScalar, std::abs(double(a[i]) - b[i]));
                    }
                }
            }
            std::cout << json({{"ok", true}, {"multi_group_files", files}, {"identical_reencodes", identical}, {"mismatches", mismatches},
                {"packed_tracks", tracks}, {"worst_rotation_rad", worstRotation}, {"worst_position_or_scale", worstPosition},
                {"worst_scalar", worstScalar}}).dump() << '\n';
            return 0;
        }
        if (command == L"t7-cameras-to-panm") {
            // Research: every animation of the T7 camera MOTAs (slots 8 and 9) converted, as camera_<slot>_<index>.panm,
            // plus index.json {slot, index, frames, descriptors, error}.
            if (arguments.size() != 3) throw std::invalid_argument("Expected T7_FILE NEW_DIRECTORY");
            if (std::filesystem::exists(arguments[2])) throw std::runtime_error("Output directory must be new");
            MovesetFile file(arguments[1]); file.Validate();
            if (file.Header().gameId != GameId_T7) throw std::invalid_argument("Input must be a T7 moveset");
            StructsT7::TKMovesetHeaderBlocks blocks;
            memcpy(&blocks, file.Bytes().data() + file.Header().block_list, sizeof(blocks));
            const auto payload = file.Payload();
            const auto& info = *reinterpret_cast<const StructsT7_gameAddr::MovesetInfo*>(payload.data() + blocks.movesetInfoBlock);
            const auto motaBlock = payload.subspan(blocks.motaBlock, blocks.movelistBlock - blocks.motaBlock);
            std::filesystem::create_directories(arguments[2]);
            json index = json::array(); size_t written = 0;
            for (int slot : {8, 9}) {
                const auto offset = static_cast<uint64_t>(info.motas.motas[slot]);
                if (offset == MOVESET_ADDR_MISSING || offset >= motaBlock.size()) continue;
                const auto animations = T7HandToRetailT8::MotaAnimations(motaBlock.subspan(offset));
                for (size_t i = 0; i < animations.size(); ++i) {
                    json entry{{"slot", slot}, {"index", i}};
                    try {
                        const auto camera = T7CameraToRetailT8::Read(animations[i]);
                        entry["frames"] = camera.frames; entry["descriptors"] = camera.descriptors;
                        const auto panm = T7CameraToRetailT8::Convert(camera);
                        const auto name = "camera_" + std::to_string(slot) + "_" + std::to_string(i) + ".panm";
                        if (!FileIO::WriteAtomically(std::filesystem::path(arguments[2]) / name, panm)) throw std::runtime_error("Cannot write PANM");
                        ++written;
                    } catch (const std::exception& error) { entry["error"] = error.what(); }
                    index.push_back(entry);
                }
            }
            if (!FileIO::WriteAtomically(std::filesystem::path(arguments[2]) / "index.json", index.dump(1))) throw std::runtime_error("Cannot write index");
            std::cout << json({{"ok", true}, {"written", written}}).dump() << '\n'; return 0;
        }
        if (command == L"t7-faces-to-panm") {
            // Research: every animation of the T7 face MOTAs (slots 4 and 5) converted, as face_<slot>_<index>.panm,
            // plus index.json {slot, index, error}.
            if (arguments.size() != 3) throw std::invalid_argument("Expected T7_FILE NEW_DIRECTORY");
            if (std::filesystem::exists(arguments[2])) throw std::runtime_error("Output directory must be new");
            MovesetFile file(arguments[1]); file.Validate();
            if (file.Header().gameId != GameId_T7) throw std::invalid_argument("Input must be a T7 moveset");
            StructsT7::TKMovesetHeaderBlocks blocks;
            memcpy(&blocks, file.Bytes().data() + file.Header().block_list, sizeof(blocks));
            const auto payload = file.Payload();
            const auto& info = *reinterpret_cast<const StructsT7_gameAddr::MovesetInfo*>(payload.data() + blocks.movesetInfoBlock);
            const auto motaBlock = payload.subspan(blocks.motaBlock, blocks.movelistBlock - blocks.motaBlock);
            std::filesystem::create_directories(arguments[2]);
            json index = json::array(); size_t written = 0;
            for (int slot : {4, 5}) {
                const auto offset = static_cast<uint64_t>(info.motas.motas[slot]);
                if (offset == MOVESET_ADDR_MISSING || offset >= motaBlock.size()) continue;
                const auto animations = T7HandToRetailT8::MotaAnimations(motaBlock.subspan(offset));
                for (size_t i = 0; i < animations.size(); ++i) {
                    json entry{{"slot", slot}, {"index", i}};
                    try {
                        const auto panm = T7FaceToRetailT8::Convert(animations[i]);
                        const auto name = "face_" + std::to_string(slot) + "_" + std::to_string(i) + ".panm";
                        if (!FileIO::WriteAtomically(std::filesystem::path(arguments[2]) / name, panm)) throw std::runtime_error("Cannot write PANM");
                        ++written;
                    } catch (const std::exception& error) { entry["error"] = error.what(); }
                    index.push_back(entry);
                }
            }
            if (!FileIO::WriteAtomically(std::filesystem::path(arguments[2]) / "index.json", index.dump(1))) throw std::runtime_error("Cannot write index");
            std::cout << json({{"ok", true}, {"written", written}}).dump() << '\n'; return 0;
        }
        if (command == L"t7-wings-to-panm") {
            // Research: every animation of the T7 wing MOTAs (slots 6 and 7) converted, as swing_<slot>_<index>.panm,
            // plus index.json {slot, index, rig, error}.
            if (arguments.size() != 3) throw std::invalid_argument("Expected T7_FILE NEW_DIRECTORY");
            if (std::filesystem::exists(arguments[2])) throw std::runtime_error("Output directory must be new");
            MovesetFile file(arguments[1]); file.Validate();
            if (file.Header().gameId != GameId_T7) throw std::invalid_argument("Input must be a T7 moveset");
            StructsT7::TKMovesetHeaderBlocks blocks;
            memcpy(&blocks, file.Bytes().data() + file.Header().block_list, sizeof(blocks));
            const auto payload = file.Payload();
            const auto& info = *reinterpret_cast<const StructsT7_gameAddr::MovesetInfo*>(payload.data() + blocks.movesetInfoBlock);
            const auto motaBlock = payload.subspan(blocks.motaBlock, blocks.movelistBlock - blocks.motaBlock);
            std::filesystem::create_directories(arguments[2]);
            json index = json::array(); size_t written = 0;
            for (int slot : {6, 7}) {
                const auto offset = static_cast<uint64_t>(info.motas.motas[slot]);
                if (offset == MOVESET_ADDR_MISSING || offset >= motaBlock.size()) continue;
                const auto animations = T7HandToRetailT8::MotaAnimations(motaBlock.subspan(offset));
                for (size_t i = 0; i < animations.size(); ++i) {
                    json entry{{"slot", slot}, {"index", i}};
                    try {
                        std::string rig;
                        const auto panm = T7WingToRetailT8::Convert(animations[i], &rig);
                        entry["rig"] = rig;
                        const auto name = "swing_" + std::to_string(slot) + "_" + std::to_string(i) + ".panm";
                        if (!FileIO::WriteAtomically(std::filesystem::path(arguments[2]) / name, panm)) throw std::runtime_error("Cannot write PANM");
                        ++written;
                    } catch (const std::exception& error) { entry["error"] = error.what(); }
                    index.push_back(entry);
                }
            }
            if (!FileIO::WriteAtomically(std::filesystem::path(arguments[2]) / "index.json", index.dump(1))) throw std::runtime_error("Cannot write index");
            std::cout << json({{"ok", true}, {"written", written}}).dump() << '\n'; return 0;
        }
        if (command == L"t7-hands-to-panm") {
            // Research: every animation of the T7 hand MOTAs (slots 2 and 3) converted for both hands, as
            // <slot>_<index>_<L|R>.panm, plus index.json {slot, index, frames, channels, error}.
            if (arguments.size() != 3) throw std::invalid_argument("Expected T7_FILE NEW_DIRECTORY");
            if (std::filesystem::exists(arguments[2])) throw std::runtime_error("Output directory must be new");
            MovesetFile file(arguments[1]); file.Validate();
            if (file.Header().gameId != GameId_T7) throw std::invalid_argument("Input must be a T7 moveset");
            StructsT7::TKMovesetHeaderBlocks blocks;
            memcpy(&blocks, file.Bytes().data() + file.Header().block_list, sizeof(blocks));
            const auto payload = file.Payload();
            const auto& info = *reinterpret_cast<const StructsT7_gameAddr::MovesetInfo*>(payload.data() + blocks.movesetInfoBlock);
            const auto motaBlock = payload.subspan(blocks.motaBlock);
            std::filesystem::create_directories(arguments[2]);
            json index = json::array(); size_t written = 0;
            for (int slot : {2, 3}) {
                const auto offset = static_cast<uint64_t>(info.motas.motas[slot]);
                if (offset == MOVESET_ADDR_MISSING || offset >= motaBlock.size()) continue;
                const auto animations = T7HandToRetailT8::MotaAnimations(motaBlock.subspan(offset));
                for (size_t i = 0; i < animations.size(); ++i) {
                    json entry{{"slot", slot}, {"index", i}};
                    try {
                        const auto source = RetailPanm::DecodeT7(animations[i]);
                        entry["frames"] = source.frames; entry["channels"] = source.channels;
                        for (auto side : {T7HandToRetailT8::Side::Left, T7HandToRetailT8::Side::Right}) {
                            const auto panm = T7HandToRetailT8::Convert(source, side);
                            const auto name = std::to_string(slot) + "_" + std::to_string(i) + (side == T7HandToRetailT8::Side::Left ? "_L" : "_R") + ".panm";
                            if (!FileIO::WriteAtomically(std::filesystem::path(arguments[2]) / name, panm)) throw std::runtime_error("Cannot write PANM");
                            ++written;
                        }
                    } catch (const std::exception& error) { entry["error"] = error.what(); }
                    index.push_back(entry);
                }
            }
            if (!FileIO::WriteAtomically(std::filesystem::path(arguments[2]) / "index.json", index.dump(1))) throw std::runtime_error("Cannot write index");
            std::cout << json({{"ok", true}, {"written", written}}).dump() << '\n'; return 0;
        }
        if (command == L"t7-anim-to-panm") {
            if (arguments.size() != 5) throw std::invalid_argument("Expected T7_FILE MOVE_ROW T8_DONOR_WITH_ANIMATIONS OUTPUT.panm");
            if (SameFile(arguments[1], arguments[4]) || SameFile(arguments[3], arguments[4]))
                throw std::invalid_argument("Output must differ from the inputs");
            MovesetFile donorFile(arguments[3]); donorFile.Validate();
            std::span<const Byte> animationTemplate;
            for (const auto& asset : RetailT8Snapshot::Assets(donorFile.Payload()))
                if (asset.type == 0 && asset.size && T7AnimToRetailT8::UsableTemplate(donorFile.Payload().subspan(asset.offset, asset.size))) {
                    animationTemplate = donorFile.Payload().subspan(asset.offset, asset.size); break;
                }
            if (animationTemplate.empty()) throw std::runtime_error("Donor has no usable template animation");
            MovesetFile file(arguments[1]); file.Validate();
            StructsT7::TKMovesetHeaderBlocks blocks;
            memcpy(&blocks, file.Bytes().data() + file.Header().block_list, sizeof(blocks));
            const auto payload = file.Payload();
            const auto& info = *reinterpret_cast<const StructsT7_gameAddr::MovesetInfo*>(payload.data() + blocks.movesetInfoBlock);
            const auto row = std::stoull(arguments[2], nullptr, 0);
            if (row >= info.table.moveCount) throw std::invalid_argument("Move row outside the moveset");
            const auto& move = *reinterpret_cast<const StructsT7_gameAddr::Move*>(payload.data() + blocks.movesetBlock + info.table.move + row * sizeof(StructsT7_gameAddr::Move));
            auto anims = payload.subspan(blocks.animationBlock, blocks.motaBlock - blocks.animationBlock);
            if (static_cast<uint64_t>(move.anim_addr) >= anims.size()) throw std::runtime_error("Move animation outside the animation block");
            auto panm = T7AnimToRetailT8::Convert(RetailPanm::DecodeT7(anims.subspan(static_cast<size_t>(move.anim_addr))), animationTemplate);
            if (!FileIO::WriteAtomically(arguments[4], panm)) throw std::runtime_error("Cannot write PANM");
            std::cout << json({{"ok", true}, {"bytes", panm.size()}}).dump() << '\n';
            return 0;
        }
        if (command == L"anim-info-t8" || command == L"poke-t8") {
            // Research diagnostics: resolve move animation handles to native resources / write one qword.
            GameProcess process;
            const bool poke = command == L"poke-t8";
            if (!process.Attach("Polaris-Win64-Shipping.exe", poke ? PROCESS_VM_WRITE | PROCESS_VM_OPERATION : 0))
                throw std::runtime_error("Cannot attach to running retail T8");
            if (poke) {
                if (arguments.size() != 3) throw std::invalid_argument("Expected ADDRESS QWORD");
                const auto address = std::stoull(arguments[1], nullptr, 0), value = std::stoull(arguments[2], nullptr, 0);
                const auto old = process.readUInt64(address);
                if (!process.writeUInt64(address, value)) throw std::runtime_error("Write failed");
                std::cout << json({{"ok", true}, {"address", address}, {"old", old}, {"new", process.readUInt64(address)}}).dump() << '\n';
                return 0;
            }
            if (arguments.size() < 3) throw std::invalid_argument("Expected PLAYER_SLOT MOVE...");
            GameAddressesFile addresses; addresses.Reload();
            GameData game; game.SetProcess(&process, &addresses);
            auto info = Games::GetGameInfoFromIdentifier(GameId_T8, GameVersions::T8::RETAIL); game.SetCurrentGame(info);
            namespace RT = RetailT8Runtime;
            if (arguments[1].size() != 1 || arguments[1][0] < L'0' || arguments[1][0] > L'1')
                throw std::invalid_argument("PLAYER_SLOT must be 0..1");
            auto state = RetailT8Preview::Read(process, game, *info, static_cast<uint8_t>(arguments[1][0] - L'0'));
            const auto container = game.ReadPtrPath("anim_container");
            auto q = [&](gameAddr a) { return RT::Read<uint64_t>(process, a); };
            json moves = json::array();
            for (size_t i = 2; i < arguments.size(); ++i) {
                const auto row = std::stoul(arguments[i], nullptr, 0);
                const auto move = state.moves + row * 0x448;
                const auto handle = q(move + 0x50);
                const uint32_t character = (handle >> 40) & 255, instance = static_cast<uint32_t>(handle);
                json entry{{"row", row}, {"handle", handle}, {"p40", q(move + 0x40)}, {"p48", q(move + 0x48)}, {"character", character}, {"instance", instance}};
                for (uint32_t domain : {uint32_t(0x81), character}) try {
                    auto node = RT::Lookup(process, q(container + 8), q(container + 0x18), q(container + 0x30), domain);
                    auto map = node ? q(q(node + 0x18)) : 0;
                    if (map && !entry.contains("resource") && !entry.contains("archive_panm_hit")) entry["map"] = map;
                    if (map) {
                        auto n = RT::Lookup(process, q(map + 0x10), q(map + 0x20), q(map + 0x38), instance);
                        entry["node"] = n;
                        if (n) { auto e = q(n + 0x18); entry["entry"] = e; if (e) { auto r = q(e + 8); entry["resource"] = r; if (r) entry["resource_panm"] = q(r + 0x1a0); } }
                        if (auto a = RT::ArchivePanm(process, map, instance, 0)) { entry["archive_panm_hit"] = a; entry["archive_domain"] = domain; }
                        if (n) entry["node_domain"] = domain;
                    }
                } catch (const std::exception& error) { entry["error"] = error.what(); }
                moves.push_back(entry);
            }
            std::cout << json({{"ok", true}, {"container", container}, {"moves", moves}}).dump() << '\n';
            return 0;
        }
        if (command == L"panm-skeleton-inc") {
            // Dev tool: native raw PANM -> C++ table of its skeleton (bone order, track options, frame-0 pose), the
            // built-in template of the no-donor T7 converter (T7AnimToRetailT8Skeleton.inc).
            if (arguments.size() != 3) throw std::invalid_argument("Expected INPUT.panm OUTPUT.inc");
            std::ifstream in{std::filesystem::path(arguments[1]), std::ios::binary};
            std::vector<Byte> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            const auto animation = RetailPanm::DecodeRaw(bytes);
            std::ostringstream out;
            out << "// Generated by `--headless panm-skeleton-inc` from a native retail T8 idle PANM (frame 0).\n"
                   "// {bone, {option field, value}..., scale xyz, rotation xyzw, translation xyz}\n";
            char line[512];
            for (const auto& track : animation.tracks) {
                const auto& s = track.samples.at(0);
                std::string options;
                for (const auto& o : track.options) options += "{" + std::to_string(o.field) + "," + std::to_string(o.value) + "},";
                snprintf(line, sizeof(line), "{\"%s\", {%s}, {%.9g,%.9g,%.9g}, {%.9g,%.9g,%.9g,%.9g}, {%.9g,%.9g,%.9g}},\n",
                    track.name.c_str(), options.c_str(), s.scale[0], s.scale[1], s.scale[2],
                    s.rotation[0], s.rotation[1], s.rotation[2], s.rotation[3], s.translation[0], s.translation[1], s.translation[2]);
                out << line;
            }
            if (!FileIO::WriteAtomically(std::filesystem::path(arguments[2]), out.str())) throw std::runtime_error("Cannot write output file");
            std::cout << json({{"ok", true}, {"bones", animation.tracks.size()}, {"frames", animation.frames}}).dump() << '\n';
            return 0;
        }
        if (command == L"t8-record-stats") {
            // Dev diagnostic: value statistics over native retail exports. Source data for the built-in tables of the
            // no-donor T7 converter (move field defaults, header constants, encrypted field slot pattern, skeleton).
            if (arguments.size() < 4) throw std::invalid_argument("Expected OUTPUT.json IDLE_DIRECTORY FILE...");
            namespace R = RetailT8Snapshot;
            auto u32 = [](std::span<const Byte> b, size_t at) { uint32_t v; memcpy(&v, b.data() + at, 4); return v; };
            auto u64 = [](std::span<const Byte> b, size_t at) { uint64_t v; memcpy(&v, b.data() + at, 8); return v; };
            constexpr size_t Cap = 512; // distinct values kept per field; more are counted as "other"
            struct Field { std::map<uint32_t, uint64_t> values; uint64_t other = 0; };
            auto count = [&](Field& f, uint32_t v) { auto it = f.values.find(v); if (it != f.values.end()) ++it->second; else if (f.values.size() < Cap) f.values[v] = 1; else ++f.other; };
            std::vector<Field> moveFields(0x448 / 4), headerFields(R::SourceHeaderSize / 4);
            std::map<std::string, uint64_t> encrypted; // "offset copy=slot filler=value" -> moves
            std::map<std::string, uint64_t> keys;      // "offset key" -> moves
            uint64_t files = 0, moves = 0;
            std::filesystem::create_directories(arguments[2]);
            for (size_t i = 3; i < arguments.size(); ++i) {
                MovesetFile file(arguments[i]); file.Validate();
                const auto& header = file.Header();
                if (header.gameId != GameId_T8 || header.minorVersion != GameVersions::T8::RETAIL ||
                    header.GetConversionOrigin() == GameVersions::T8::FROM_T7) continue;
                ++files;
                const auto payload = file.Payload();
                R::Header snapshot; memcpy(&snapshot, payload.data(), sizeof(snapshot));
                const auto source = payload.subspan(snapshot.sourceHeaderOffset, R::SourceHeaderSize);
                for (size_t k = 0; k < headerFields.size(); ++k) count(headerFields[k], u32(source, k * 4));
                const auto table = R::Table(payload, 12);
                for (size_t at = 0; at + 0x448 <= table.size(); at += 0x448, ++moves) {
                    const auto row = table.subspan(at, 0x448);
                    for (size_t k = 0; k < moveFields.size(); ++k) count(moveFields[k], u32(row, k * 4));
                    for (size_t off : {0x00, 0x20, 0x58, 0x78, 0xd0, 0xf0}) {
                        const auto value = EditorT8Retail::Decode(u64(row, off), u64(row, off + 8));
                        int copy = -1; uint32_t filler = 0;
                        for (int s = 0; s < 4; ++s) if (value && u32(row, off + 0x10 + s * 4) == *value && copy < 0) copy = s;
                        for (int s = 0; s < 4; ++s) if (s != copy) { filler = u32(row, off + 0x10 + s * 4); break; }
                        char text[96]; snprintf(text, sizeof(text), "%03zx copy=%d filler=%08x", off, copy, filler);
                        ++encrypted[text];
                        snprintf(text, sizeof(text), "%03zx key=%016llx", off, static_cast<unsigned long long>(u64(row, off + 8)));
                        ++keys[text];
                    }
                }
                // Idle body PANM (alias 0x8001), for the built-in skeleton template.
                const auto idleRow = static_cast<size_t>(*reinterpret_cast<const uint16_t*>(source.data() + 0xa8 + 2));
                if ((idleRow + 1) * 0x448 <= table.size()) {
                    const auto instance = static_cast<uint32_t>(u64(table, idleRow * 0x448 + 0x50));
                    for (const auto& asset : R::Assets(payload))
                        if (asset.type == 0 && asset.size && asset.instance == instance) {
                            const auto name = std::filesystem::path(arguments[i]).stem().wstring() + L"_idle.panm";
                            if (!FileIO::WriteAtomically((std::filesystem::path(arguments[2]) / name).wstring(), payload.subspan(asset.offset, asset.size)))
                                throw std::runtime_error("Cannot write idle PANM");
                            break;
                        }
                }
            }
            auto dump = [](const std::vector<Field>& fields, uint64_t total) {
                json out = json::array();
                for (size_t k = 0; k < fields.size(); ++k) {
                    std::vector<std::pair<uint64_t, uint32_t>> top;
                    for (auto [v, n] : fields[k].values) top.push_back({n, v});
                    std::sort(top.rbegin(), top.rend());
                    json values = json::array();
                    for (size_t t = 0; t < top.size() && t < 6; ++t) values.push_back({top[t].second, top[t].first});
                    out.push_back({{"offset", k * 4}, {"distinct", fields[k].values.size() + (fields[k].other ? 1 : 0)},
                        {"other", fields[k].other}, {"top", values}, {"top_share", total ? double(top.empty() ? 0 : top[0].first) / double(total) : 0.0}});
                }
                return out;
            };
            json result{{"files", files}, {"moves", moves}, {"move_fields", dump(moveFields, moves)},
                {"header_fields", dump(headerFields, files)}, {"encrypted", encrypted}};
            json topKeys = json::object();
            for (auto [k, n] : keys) if (n * 50 >= moves) topKeys[k] = n; // keys used by at least 2% of moves
            result["common_keys"] = topKeys;
            if (!FileIO::WriteAtomically(arguments[1], result.dump(1))) throw std::runtime_error("Cannot write output file");
            std::cout << json({{"ok", true}, {"files", files}, {"moves", moves}}).dump() << '\n';
            return 0;
        }
        if (command == L"convert-t8-full") {
            // T7 -> retail T8, self-contained (records and animations from the T7 file plus built-in tables only).
            if (arguments.size() < 3 || arguments.size() > 5)
                throw std::invalid_argument("Expected T7_INPUT OUTPUT [CODEC] [REPORT.json]");
            auto codec = arguments.size() >= 4 ? Codec(arguments[3]) : TKMovesetCompressionType_None;
            if (SameFile(arguments[1], arguments[2]))
                throw std::invalid_argument("Conversion output must differ from the input");
            MovesetFile source(arguments[1]); source.Validate();
            if (source.Header().gameId != GameId_T7) throw std::invalid_argument("Input must be a T7 moveset");
            StructsT7::TKMovesetHeaderBlocks blocks;
            memcpy(&blocks, source.Bytes().data() + source.Header().block_list, sizeof(blocks));
            T7ToRetailT8::ConversionReport report;
            auto converted = T7ToRetailT8::ConvertStandalone(source.Header(), blocks, source.Payload(), &report, true, ImportProfile::Read(source.Bytes()));
            uint64_t size = 0;
            std::unique_ptr<Byte[]> compressed(CompressionUtils::RAW::Moveset::Compress(converted.data(), converted.size(), codec, size));
            if (!compressed || !FileIO::WriteAtomically(arguments[2], std::span(compressed.get(), static_cast<size_t>(size))))
                throw std::runtime_error("Cannot write output file");
            auto result = json({{"ok", true}, {"output", Helpers::to_utf8(arguments[2])}, {"bytes", size},
                {"conversion", T7ToRetailT8::ConversionReportJson(report)}});
            if (arguments.size() == 5) {
                auto text = result.dump(1);
                if (!FileIO::WriteAtomically(arguments[4], text + "\n"))
                    throw std::runtime_error("Cannot write report file");
            }
            result["conversion"].erase("requirements"); result["conversion"].erase("properties"); result["conversion"]["animations"].erase("converted_moves");
            std::cout << result.dump() << '\n';
            return 0;
        }
        if (command == L"profile" || command == L"set-profile") {
            // Import profile (docs/import-profile.md). profile FILE: show it. set-profile FILE OUTPUT KEY=VALUE...: write a
            // copy with the given options (identity=target|CHARACTER_ID gender=target|male|female
            // story_flag=game|N in_heat=0|1 flags_clear=0|1); the payload is copied byte for byte.
            if (command == L"profile" && arguments.size() != 2) throw std::invalid_argument("Expected FILE");
            if (command == L"set-profile" && arguments.size() < 3) throw std::invalid_argument("Expected FILE OUTPUT [KEY=VALUE...]");
            std::ifstream file(arguments[1], std::ios::binary);
            if (!file) throw std::invalid_argument("Cannot open the moveset file");
            const std::vector<Byte> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            auto profile = ImportProfile::Read(bytes);
            const bool stored = !ImportProfile::ReadProperties(bytes).empty();
            auto describe = [](const ImportProfile::Profile& p) {
                return json({{"identity", p.identity == ImportProfile::Profile::Identity::Fixed ? json(p.character) : json("target")},
                    {"gender", p.gender == ImportProfile::Profile::Gender::Male ? "male" : p.gender == ImportProfile::Profile::Gender::Female ? "female" : "target"},
                    {"story_flag", p.storyFlag == ImportProfile::StoryFlagGameDecides ? json("game") : json(p.storyFlag)},
                    {"in_heat", p.inHeat}, {"flags_clear", p.flagsClear}, {"summary", ImportProfile::Describe(p)}});
            };
            if (command == L"profile") {
                std::cout << json({{"ok", true}, {"stored", stored}, {"profile", describe(profile)}}).dump() << '\n';
                return 0;
            }
            if (SameFile(arguments[1], arguments[2]))
                throw std::invalid_argument("Output must differ from the input");
            for (size_t i = 3; i < arguments.size(); ++i) {
                const auto item = Helpers::to_utf8(arguments[i]);
                const auto eq = item.find('=');
                if (eq == std::string::npos) throw std::invalid_argument("Expected KEY=VALUE: " + item);
                const auto key = item.substr(0, eq), value = item.substr(eq + 1);
                auto number = [&](int low, int high) {
                    size_t used = 0; int n = 0;
                    try { n = std::stoi(value, &used, 0); } catch (const std::exception&) { used = 0; }
                    if (!used || used != value.size() || n < low || n > high) throw std::invalid_argument("Bad value for " + key + ": " + value);
                    return n;
                };
                if (key == "identity") {
                    if (value == "target") profile.identity = ImportProfile::Profile::Identity::AdoptTarget;
                    else { profile.identity = ImportProfile::Profile::Identity::Fixed; profile.character = number(0, 0xff); }
                } else if (key == "gender") {
                    if (value == "target") profile.gender = ImportProfile::Profile::Gender::FromTarget;
                    else if (value == "male") profile.gender = ImportProfile::Profile::Gender::Male;
                    else if (value == "female") profile.gender = ImportProfile::Profile::Gender::Female;
                    else throw std::invalid_argument("gender is target, male or female");
                } else if (key == "story_flag") profile.storyFlag = value == "game" ? ImportProfile::StoryFlagGameDecides : number(0, 0xffff);
                else if (key == "in_heat") profile.inHeat = number(0, 1) != 0;
                else if (key == "flags_clear") profile.flagsClear = number(0, 1) != 0;
                else if (key == "story_mode") profile.storyMode = number(0, 1) ? ImportProfile::Profile::Toggle::Versus : ImportProfile::Profile::Toggle::Default;
                else if (key == "dlc_story") profile.dlcStory = number(0, 1) ? ImportProfile::Profile::Toggle::Versus : ImportProfile::Profile::Toggle::Default;
                else if (key == "special_modes") profile.specialModes = number(0, 1) ? ImportProfile::Profile::Toggle::Versus : ImportProfile::Profile::Toggle::Default;
                else if (key == "stage") profile.stage = number(0, 1) ? ImportProfile::Profile::Toggle::Versus : ImportProfile::Profile::Toggle::Default;
                else if (key == "wave_index") profile.waveIndex = value == "game" ? ImportProfile::ValueGameDecides : number(0, 0xffff);
                else if (key == "battle_script") profile.battleScript = value == "game" ? ImportProfile::ValueGameDecides : number(0, INT32_MAX);
                else if (key == "item") {
                    if (value == "game") profile.item = ImportProfile::Profile::Item::Default;
                    else if (value == "none") profile.item = ImportProfile::Profile::Item::None;
                    else if (value == "all") profile.item = ImportProfile::Profile::Item::All;
                    else throw std::invalid_argument("item is game, none or all");
                } else if (key == "body") {
                    if (value == "game") profile.bodyType = ImportProfile::Profile::Body::Default;
                    else if (value == "normal") profile.bodyType = ImportProfile::Profile::Body::Normal;
                    else if (value == "big") profile.bodyType = ImportProfile::Profile::Body::Big;
                    else if (value == "bear") profile.bodyType = ImportProfile::Profile::Body::Bear;
                    else throw std::invalid_argument("body is game, normal, big or bear");
                }
                else if (key == "story_effects") profile.storyEffects = number(0, 1) ? ImportProfile::Profile::Strip::Strip : ImportProfile::Profile::Strip::Keep;
                else if (key == "global_writes") profile.globalWrites = number(0, 1) ? ImportProfile::Profile::Strip::Strip : ImportProfile::Profile::Strip::Keep;
                else throw std::invalid_argument("Unknown profile option: " + key);
            }
            const auto output = ImportProfile::WithProfile(bytes, profile);
            if (!FileIO::WriteAtomically(arguments[2], output)) throw std::runtime_error("Cannot write output file");
            std::cout << json({{"ok", true}, {"output", Helpers::to_utf8(arguments[2])}, {"profile", describe(ImportProfile::Read(output))}}).dump() << '\n';
            return 0;
        }
        if (command == L"check-rules") {
            // Validates a t7_t8_conversion_rules.ini as the updater and the converters would (no file: report the rules in use).
            if (arguments.size() > 2) throw std::invalid_argument("Expected [game_addresses.ini | t7_t8_conversion_rules.ini]");
            auto counts = [](const ConversionRules::Rules& r) {
                size_t lines = 0; for (const auto& [name, map] : r.maps) lines += map.lines.size();
                return json({{"version", r.version}, {"format", r.format}, {"values", r.values.size()}, {"map_lines", lines},
                    {"lists", r.lists.size()}, {"tables", r.tables.size()}, {"transforms", r.transforms.size()},
                    {"t8_move_fields", r.t8MoveDefaults.size()}});
            };
            if (arguments.size() == 2) {
                std::ifstream file(arguments[1], std::ios::binary);
                if (!file) throw std::invalid_argument("Cannot open the rules file");
                std::stringstream text; text << file.rdbuf();
                const auto rules = ConversionRules::Parse(ConversionRules::Section(text.str()));
                auto result = counts(rules);
                result["ok"] = true; result["built_in_version"] = ConversionRules::Embedded().version;
                result["would_replace_built_in"] = rules.version > ConversionRules::Embedded().version;
                std::cout << result.dump() << '\n';
                return 0;
            }
            const auto status = ConversionRules::CurrentStatus();
            auto result = counts(ConversionRules::Current());
            result["ok"] = true; result["source"] = status.source; result["built_in_version"] = status.embeddedVersion;
            if (!status.error.empty()) result["ignored_file_error"] = status.error;
            std::cout << result.dump() << '\n';
            return 0;
        }
        if (command == L"convert-t7") {
            // Retail T8 -> T7, self-contained (records and animations from the T8 file plus built-in tables only).
            if (arguments.size() < 3 || arguments.size() > 5)
                throw std::invalid_argument("Expected T8_INPUT OUTPUT [CODEC] [REPORT.json]");
            auto codec = arguments.size() >= 4 ? Codec(arguments[3]) : TKMovesetCompressionType_None;
            if (std::filesystem::absolute(arguments[1]).lexically_normal() == std::filesystem::absolute(arguments[2]).lexically_normal() ||
                (std::filesystem::exists(arguments[2]) && std::filesystem::equivalent(arguments[1], arguments[2])))
                throw std::invalid_argument("Conversion output must differ from the input");
            MovesetFile source(arguments[1]);
            if (source.Header().gameId != GameId_T8 || source.Header().minorVersion != GameVersions::T8::RETAIL)
                throw std::invalid_argument("Input must be a retail T8 moveset");
            source.Validate();
            T8ToT7::Report report;
            auto converted = T8ToT7::Convert(source.Header(), source.Payload(), &report, true, ImportProfile::Read(source.Bytes()));
            uint64_t size = 0;
            std::unique_ptr<Byte[]> compressed(CompressionUtils::RAW::Moveset::Compress(converted.data(), converted.size(), codec, size));
            if (!compressed || !FileIO::WriteAtomically(arguments[2], std::span(compressed.get(), static_cast<size_t>(size))))
                throw std::runtime_error("Cannot write output file");
            auto result = json({{"ok", true}, {"output", Helpers::to_utf8(arguments[2])}, {"bytes", size},
                {"conversion", T8ToT7::ReportJson(report)}});
            if (arguments.size() == 5 && !FileIO::WriteAtomically(arguments[4], result.dump(1) + "\n"))
                throw std::runtime_error("Cannot write report file");
            result["conversion"]["requirements"].erase("unresolved_ids"); result["conversion"]["properties"].erase("dropped_ids");
            std::cout << result.dump() << '\n';
            return 0;
        }
        if (command == L"repair-handles-t8") {
            // Converted T7 documents written before handles were kept: copy native body handles from the donor
            // export (matched by animation hash) so PANM import/export works on them.
            if (arguments.size() < 4 || arguments.size() > 5) throw std::invalid_argument("Expected CONVERTED DONOR OUTPUT [CODEC]");
            for (size_t i : {size_t(1), size_t(2)})
                if (SameFile(arguments[i], arguments[3]))
                    throw std::invalid_argument("Output must differ from the inputs");
            MovesetFile converted(arguments[1]), donor(arguments[2]); converted.Validate(); donor.Validate();
            for (const auto* file : {&converted, &donor})
                if (file->Header().gameId != GameId_T8 || file->Header().minorVersion != GameVersions::T8::RETAIL)
                    throw std::invalid_argument("Both files must be retail T8 documents");
            auto* info = Games::GetGameInfoFromIdentifier(GameId_T8, GameVersions::T8::RETAIL);
            GameProcess offline; GameData offlineData;
            EditorT8Retail editor(offline, offlineData, info);
            auto* copy = static_cast<Byte*>(malloc(converted.Bytes().size())); if (!copy) throw std::bad_alloc();
            memcpy(copy, converted.Bytes().data(), converted.Bytes().size());
            if (!editor.LoadMoveset(copy, converted.Bytes().size())) throw std::runtime_error("Editor rejected the converted document");
            const auto updated = editor.AdoptAnimationHandles(RetailT8Snapshot::Table(donor.Payload(), 12));
            size_t unresolved = 0;
            for (uint32_t row = 0; row < editor.Count(12); ++row) if (!editor.AnimationInstance(row)) ++unresolved;
            auto snapshot = editor.BuildSaveSnapshot();
            const auto codec = arguments.size() == 5 ? Codec(arguments[4]) : TKMovesetCompressionType_None;
            uint64_t size = 0;
            std::unique_ptr<Byte[]> compressed(CompressionUtils::RAW::Moveset::Compress(snapshot.data(), snapshot.size(), codec, size));
            if (!compressed || !FileIO::WriteAtomically(arguments[3], std::span(compressed.get(), static_cast<size_t>(size))))
                throw std::runtime_error("Cannot write output file");
            std::cout << json({{"ok", true}, {"moves_updated", updated}, {"moves_unresolved", unresolved}, {"bytes", size}}).dump() << '\n';
            return 0;
        }
        if (command == L"check-panm-t8") {
            // Whether each PANM can be imported as a custom retail T8 animation (EditorT8Retail::ValidatePanm).
            if (arguments.size() < 2) throw std::invalid_argument("Expected PANM_FILE...");
            json reports = json::array(); bool valid = true;
            for (auto& argument : arguments.subspan(1)) {
                json report{{"file", Helpers::to_utf8(argument)}};
                try { auto bytes = EditorT8Retail::ReadPanmFile(argument); EditorT8Retail::ValidatePanm(bytes); report.update({{"ok", true}, {"bytes", bytes.size()}}); }
                catch (const std::exception& error) { report.update({{"ok", false}, {"error", error.what()}}); valid = false; }
                reports.push_back(std::move(report));
            }
            std::cout << reports.dump() << '\n';
            return valid ? 0 : 1;
        }
        if (command == L"export-panm-t8" || command == L"export-panms-t8" || command == L"import-panm-t8") {
            // Standalone PANM export/import on a retail T8 document (EditorT8Retail::MoveAnimation/ExportAnimations/ImportPanm).
            //   export-panm-t8  DOC MOVE OUTPUT.panm
            //   export-panms-t8 DOC NEW_DIRECTORY
            //   import-panm-t8  DOC MOVE FILE.panm OUTPUT [NAME|-] [CODEC]
            // MOVE is a row or a generic alias (e.g. 0x8001).
            const bool importing = command == L"import-panm-t8", all = command == L"export-panms-t8";
            if (all ? arguments.size() != 3 : importing ? (arguments.size() < 5 || arguments.size() > 7) : arguments.size() != 4)
                throw std::invalid_argument(all ? "Expected DOC NEW_DIRECTORY" : importing ?
                    "Expected DOC MOVE FILE.panm OUTPUT [NAME|-] [CODEC]" : "Expected DOC MOVE OUTPUT.panm");
            MovesetFile file(arguments[1]); file.Validate();
            if (file.Header().gameId != GameId_T8 || file.Header().minorVersion != GameVersions::T8::RETAIL)
                throw std::invalid_argument("DOC must be a retail T8 document");
            auto* info = Games::GetGameInfoFromIdentifier(GameId_T8, GameVersions::T8::RETAIL);
            GameProcess offline; GameData offlineData;
            EditorT8Retail editor(offline, offlineData, info);
            auto* copy = static_cast<Byte*>(malloc(file.Bytes().size())); if (!copy) throw std::bad_alloc();
            memcpy(copy, file.Bytes().data(), file.Bytes().size());
            if (!editor.LoadMoveset(copy, file.Bytes().size())) throw std::runtime_error("Editor rejected the document");
            auto move = [&](const std::wstring& text) { return editor.ResolveMove(static_cast<uint32_t>(std::stoul(text, nullptr, 0))); };
            auto names = GameStaticData::LoadOptional("t8-retail");
            if (all) {
                auto count = editor.ExportAnimations(arguments[2], [&](uint32_t hash) { if (auto* custom = editor.CustomName(hash)) return *custom; auto* known = names.FindName(hash); return known ? *known : std::string(); });
                std::cout << json({{"ok", true}, {"animations", count}, {"directory", Helpers::to_utf8(arguments[2])}}).dump() << '\n';
                return 0;
            }
            const auto row = move(arguments[2]);
            if (!importing) {
                if (SameFile(arguments[1], arguments[3])) throw std::invalid_argument("Output must differ from the document");
                auto bytes = editor.MoveAnimation(row);
                if (!FileIO::WriteAtomically(arguments[3], bytes)) throw std::runtime_error("Cannot write PANM");
                std::cout << json({{"ok", true}, {"move", row}, {"instance", *editor.AnimationInstance(row)}, {"bytes", bytes.size()}}).dump() << '\n';
                return 0;
            }
            if (SameFile(arguments[1], arguments[4]) || SameFile(arguments[3], arguments[4]))
                throw std::invalid_argument("Output must differ from the document and the PANM");
            // The new animation's name: NAME, or the PANM's file name.
            const auto animationName = Helpers::to_utf8(arguments.size() >= 6 && arguments[5] != L"-" ? arguments[5] :
                std::filesystem::path(arguments[3]).stem().wstring());
            auto result = editor.ImportPanm(row, EditorT8Retail::ReadPanmFile(arguments[3]), animationName);
            auto snapshot = editor.BuildSaveSnapshot();
            const auto codec = arguments.size() == 7 ? Codec(arguments[6]) : TKMovesetCompressionType_None;
            uint64_t size = 0;
            std::unique_ptr<Byte[]> compressed(CompressionUtils::RAW::Moveset::Compress(snapshot.data(), snapshot.size(), codec, size));
            if (!compressed || !FileIO::WriteAtomically(arguments[4], std::span(compressed.get(), static_cast<size_t>(size))))
                throw std::runtime_error("Cannot write output file");
            std::cout << json({{"ok", true}, {"move", row}, {"instance", result.instance}, {"custom", result.custom},
                {"replaced", result.replaced}, {"animation_name", result.name}, {"rewritten", result.rewritten}, {"bytes", size}}).dump() << '\n';
            return 0;
        }
        if (command != L"convert" && command != L"convert-t8" && command != L"recompress")
            throw std::invalid_argument("Unknown headless command");
        if (arguments.size() < 3 || arguments.size() > 4)
            throw std::invalid_argument("Expected INPUT OUTPUT and optional codec");
        auto codec = arguments.size() == 4 ? Codec(arguments[3]) : TKMovesetCompressionType_None;
        MovesetFile source(arguments[1]);
        std::vector<Byte> converted;
        auto data = source.Bytes();
        if (command == L"convert" || command == L"convert-t8") {
            if (std::filesystem::absolute(arguments[1]).lexically_normal() == std::filesystem::absolute(arguments[2]).lexically_normal() ||
                (std::filesystem::exists(arguments[2]) && std::filesystem::equivalent(arguments[1],arguments[2])))
                throw std::invalid_argument("Conversion output must differ from the source");
            converted = command == L"convert-t8" ? source.ConvertToT8() : source.ConvertToT7();
            data = converted;
        } else {
            source.Validate();
        }
        uint64_t size = 0;
        std::unique_ptr<Byte[]> compressed(CompressionUtils::RAW::Moveset::Compress(data.data(), data.size(), codec, size));
        if (!compressed || !FileIO::WriteAtomically(arguments[2], std::span(compressed.get(), static_cast<size_t>(size))))
            throw std::runtime_error("Cannot write output file");
        std::cout << json({{"ok", true}, {"output", Helpers::to_utf8(arguments[2])}, {"bytes", size}}).dump() << '\n';
        return 0;
    } catch (const std::invalid_argument& error) {
        std::cout << json({{"ok", false}, {"error", error.what()}}).dump(-1, ' ', false, json::error_handler_t::replace) << '\n';
        return 2;
    } catch (const std::exception& error) {
        std::cout << json({{"ok", false}, {"error", error.what()}}).dump(-1, ' ', false, json::error_handler_t::replace) << '\n';
        return 1;
    }
}
