void RunGameStaticDataTests();
void RunAddressScannerTests();
void RunT7ToRetailT8Tests();
void RunT8ToT7Tests();
void RunMovesetListSortTests();
void RunRetailPanmTests();
void RunEditorMutationTests();
void RunSpecializedEditorTests();
void RunSemanticValidationTests();
void RunEditorWorkflowTests();
void RunFileJobTests();
void RunProcessWriteTests();
void RunAnimationTests();
#include "MovesetFile.hpp"
std::vector<Byte> T7RetailConvertibleFixture();
#include "RetailT8Records.hpp"
#include "Compression.hpp"
#include "GameImport.hpp"
#include "Editor_t7/Editor_t7.hpp"
#include "MovesetLoader_t7.hpp"
#include "MovesetConverters/MovesetConverters.hpp"
#include "../TKMovesets2/Updater/Http.hpp"
#include "Importer_t7/Importer_t7_Utils.hpp"
#include "Settings.hpp"
#include "AttachRetryBackoff.hpp"
#include <future>
#include <functional>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

namespace fs = std::filesystem;
namespace raw = CompressionUtils::RAW;

static void check(bool condition, const char* description)
{
	if (!condition) throw std::runtime_error(description);
}

static std::vector<Byte> fixture()
{
	TKMovesetHeader header;
	header.header_size = sizeof(header);
	header.block_list = sizeof(header);
	header.block_list_size = 1;
	header.moveset_data_start = header.block_list + sizeof(uint64_t);
	std::vector<Byte> file(header.moveset_data_start + 1, 0);
	memcpy(file.data(), &header, sizeof(header));
	file.back() = 42;
	return file;
}

static void codecs()
{
	std::array<Byte, 80> guard;
	guard.fill(0xa5);
	Byte literal = 42;
	check(!raw::LZMA::CompressToBuffer(&literal, 1, guard.data(), 1), "XZ accepted a one-byte output");
	check(std::all_of(guard.begin() + 1, guard.end(), [](Byte b) { return b == 0xa5; }), "XZ overflow");
	const Byte block[] = {0x10, literal};
	check(!raw::LZ4::DecompressToBuffer(block, sizeof(block), guard.data(), 8), "short LZ4 output accepted");

	auto file = fixture();
	for (auto algorithm : {TKMovesetCompressionType_LZ4, TKMovesetCompressionType_LZMA, TKMovesetCompressionType_CompactZstd}) {
		uint64_t size = 0;
		std::unique_ptr<Byte[]> encoded(raw::Moveset::Compress(file.data(), file.size(), algorithm, size));
		check(encoded != nullptr, "tiny payload did not compress");
		auto copy = static_cast<Byte*>(malloc(size));
		check(copy != nullptr, "allocation failed");
		memcpy(copy, encoded.get(), size);
		check(raw::Moveset::ExpandFileBuffer(copy, size), "round trip failed");
		check(size == file.size() && memcmp(copy, file.data(), size) == 0, "round trip changed prefix or payload");
		free(copy);
	}
	{
		// A legacy file at the 256 MiB stored limit with incompressible data: LZ4 framing makes it larger, and the
		// loader would refuse it, so the encoder must refuse it too instead of producing a file that cannot reopen.
		auto big = fixture();
		big.resize(256ULL << 20);
		uint64_t state = 0x9e3779b97f4a7c15ULL;
		const auto start = reinterpret_cast<const TKMovesetHeader*>(big.data())->moveset_data_start;
		for (size_t at = start; at + 8 <= big.size(); at += 8) {
			state ^= state << 13; state ^= state >> 7; state ^= state << 17;
			memcpy(big.data() + at, &state, 8);
		}
		uint64_t size = 0;
		std::unique_ptr<Byte[]> encoded(raw::Moveset::Compress(big.data(), big.size(), TKMovesetCompressionType_LZ4, size));
		check(encoded == nullptr, "LZ4 produced a file above the loader's stored-size limit");
	}
	auto* header = reinterpret_cast<TKMovesetHeader*>(file.data());
	header->moveset_data_start = UINT32_MAX;
	check(!header->ValidateHeader(file.size()), "accepted offset beyond EOF");
	header->moveset_data_start = sizeof(*header) + 8;
	header->block_list_size = UINT32_MAX;
	check(!header->ValidateHeader(file.size()), "accepted overflowing block list");
}

static void files(const fs::path& directory)
{
	auto bytes = fixture();
	auto source = directory / "source.tkmvst";
	auto destination = directory / "saved.tkmvst";
	std::ofstream(source, std::ios::binary).write(reinterpret_cast<char*>(bytes.data()), bytes.size());
	std::ofstream(destination, std::ios::binary) << "original";
	// Deny delete sharing to reproduce a failed final replacement on Windows.
	auto handle = CreateFileW(destination.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
	check(handle != INVALID_HANDLE_VALUE, "cannot lock destination");
	auto installed = CompressionUtils::FILE::Moveset::Compress(destination, source, TKMovesetCompressionType_LZ4);
	CloseHandle(handle);
	check(!installed && fs::exists(source), "failed save consumed the source");
	std::ifstream previous(destination, std::ios::binary);
	std::string value{std::istreambuf_iterator<char>(previous), {}};
	check(value == "original", "failed save replaced the previous version");
	previous.close();
	check(CompressionUtils::FILE::Moveset::Compress(destination, source, TKMovesetCompressionType_LZ4), "save failed");
	check(!fs::exists(source) && fs::file_size(destination) > 0, "successful save was not installed");
	fs::remove(destination);
}

static void layout()
{
	using namespace StructsT7;
	TKMovesetHeaderBlocks offsets{};
	offsets.tableBlock = offsetof(MovesetInfo, table);
	offsets.motalistsBlock = offsets.tableBlock + sizeof(MovesetTable);
	offsets.nameBlock = offsets.motalistsBlock + sizeof(MotaList);
	offsets.movesetBlock = offsets.animationBlock = offsets.motaBlock = offsets.movelistBlock = offsets.nameBlock;
	std::vector<Byte> payload(offsets.movelistBlock, 0);
	memset(payload.data() + offsets.motalistsBlock, 0xff, sizeof(MotaList));
	check(T7ImportUtils::ValidateT7Layout(offsets, payload.data(), payload.size()), "valid empty layout rejected");
	auto table = reinterpret_cast<StructsT7_gameAddr::MovesetTable*>(payload.data() + offsets.tableBlock);
	table->moveCount = UINT64_MAX;
	check(!T7ImportUtils::ValidateT7Layout(offsets, payload.data(), payload.size()), "unbounded move table accepted");
}

static std::vector<Byte> nativeFixture()
{
	using namespace StructsT7;
	TKMovesetHeader header;
	strcpy_s(header.version_string, "1.0");
	strcpy_s(header.origin, "T7");
	strcpy_s(header.target_character, "TEST");
	header.header_size = header.block_list = sizeof(header);
	header.block_list_size = 8;
	header.moveset_data_start = sizeof(header) + sizeof(TKMovesetHeaderBlocks);
	header.gameId = GameId_T7;
	header.minorVersion = GameVersions::T7::NONE;
	TKMovesetHeaderBlocks offsets{};
	offsets.tableBlock = offsetof(MovesetInfo, table);
	offsets.motalistsBlock = offsetof(MovesetInfo, motas);
	offsets.nameBlock = offsets.movesetBlock = offsets.animationBlock = offsets.motaBlock = offsets.movelistBlock = sizeof(MovesetInfo);
	std::vector<Byte> file(header.moveset_data_start + sizeof(MovesetInfo));
	memcpy(file.data(), &header, sizeof(header));
	memcpy(file.data() + header.block_list, &offsets, sizeof(offsets));
	memset(file.data() + header.moveset_data_start + offsets.motalistsBlock, 0xff, sizeof(MotaList));
	return file;
}

static void onlineImport()
{
	using namespace StructsT7;
	auto file = nativeFixture();
	SharedMemT7_Player player{};
	for (auto algorithm : {TKMovesetCompressionType_None, TKMovesetCompressionType_LZ4, TKMovesetCompressionType_LZMA, TKMovesetCompressionType_CompactZstd}) {
		uint64_t size = file.size();
		Byte* packet;
		if (algorithm == TKMovesetCompressionType_None) {
			packet = new Byte[size];
			memcpy(packet, file.data(), size);
		} else {
			packet = raw::Moveset::Compress(file.data(), file.size(), algorithm, size);
		}
		check(packet != nullptr, "online fixture compression");
		std::unique_ptr<Byte[]> installed(MovesetLoaderT7::ImportForOnline(player, packet, size));
		check(installed != nullptr, "valid online payload rejected");
	}
	auto packet = new Byte[file.size()];
	memcpy(packet, file.data(), file.size());
	reinterpret_cast<TKMovesetHeader*>(packet)->gameId = UINT16_MAX;
	check(!MovesetLoaderT7::ImportForOnline(player, packet, file.size()), "unsupported online game accepted");
}

using Unary = int(*)(int);
static uint64_t trampoline = 0;
static int intercepted(int input) { return reinterpret_cast<Unary>(trampoline)(input) + 7; }

static void hook()
{
	auto code = static_cast<Byte*>(VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	check(code != nullptr, "hook fixture allocation");
	// mov eax, ecx / add eax, 1 / padding / ret
	memset(code, 0x90, 64);
	const Byte instructions[] = {0x89, 0xc8, 0x83, 0xc0, 0x01};
	memcpy(code, instructions, sizeof(instructions));
	code[32] = 0xc3;
	DWORD previous;
	check(VirtualProtect(code, 64, PAGE_EXECUTE_READ, &previous), "hook fixture protection");
	FlushInstructionCache(GetCurrentProcess(), code, 64);
	check(reinterpret_cast<uintptr_t>(Sig::find(code, 64, "89 C8 83 C0 01")) == reinterpret_cast<uintptr_t>(code), "signature lookup");
	auto function = reinterpret_cast<Unary>(code);
	check(function(2) == 3, "original fixture");
	{
		PLH::x64Detour detour(reinterpret_cast<uint64_t>(code), reinterpret_cast<uint64_t>(&intercepted), &trampoline);
		check(detour.hook() && function(2) == 10, "detour/trampoline");
		check(detour.unHook() && function(2) == 3, "detour removal");
	}
	VirtualFree(code, 0, MEM_RELEASE);
}

static void lastAnimation()
{
	using namespace StructsT7;
	GameData game;
	GameProcess process;
	GameInfo info{};
	info.dataString = info.minorDataString = "t7";
	EditorT7 editor(process, game, &info);
	TKMovesetHeader header;
	header.header_size = header.block_list = sizeof(header);
	header.block_list_size = 8;
	header.moveset_data_start = sizeof(header) + sizeof(TKMovesetHeaderBlocks);
	TKMovesetHeaderBlocks blocks{};
	blocks.tableBlock = offsetof(MovesetInfo, table);
	blocks.motalistsBlock = offsetof(MovesetInfo, motas);
	blocks.nameBlock = blocks.movesetBlock = blocks.animationBlock = sizeof(MovesetInfo);
	blocks.motaBlock = blocks.animationBlock + 16;
	blocks.movelistBlock = blocks.motaBlock + 8;
	auto size = header.moveset_data_start + blocks.movelistBlock;
	auto file = static_cast<Byte*>(calloc(1, size));
	check(file != nullptr, "editor fixture allocation");
	memcpy(file, &header, sizeof(header));
	memcpy(file + header.block_list, &blocks, sizeof(blocks));
	memcpy(file + size - 8, "MOTAdata", 8);
	editor.LoadMovesetPtr(file, size);
	editor.DeleteAnimation(8);
	uint64_t newSize;
	auto saved = editor.GetMoveset(newSize);
	check(newSize == size - 8 && memcmp(saved + newSize - 8, "MOTAdata", 8) == 0, "last animation damaged MOTA");

}

// Exercise the real queue and importer dispatch using two synthetic player addresses.
class TestImporter : public Importer
{
public:
	using Importer::Importer;
	std::vector<gameAddr> calls;
	int cleanups = 0;
    std::function<void()> beforeImport;
	bool CanImport() override { return true; }
	void CleanupUnusedMovesets() override { ++cleanups; }
	gameAddr GetCharacterAddress(uint8_t id) override { return 100 + id; }
	gameAddr GetMovesetAddress(uint8_t) override { return 0; }

	ImportationErrcode_ ImportMovesetData(const TKMovesetHeader*, Byte* bytes, uint64_t size,
		gameAddr player, ImportSettings, uint8_t&) override
	{
        if (beforeImport) beforeImport();
		check(calls.size() < 2, "queue repeated a completed request");
		check(size == 1 && *bytes == 42, "queued payload changed after submission");
		calls.push_back(player);
		lastLoaded.address = player;
		return ImportationErrcode_Successful;
	}
};

class TestWorker : public GameImport
{
public:
	using GameImport::OnProcessAttach;
	using GameImport::RunningUpdate;
	TestImporter& importer() { return *static_cast<TestImporter*>(m_importer.get()); }
};

static void operationDoesNotBlockRendering()
{
    GameFactory<TestImporter> factory;
    GameInfo info{}; info.characterCount = 2; info.importer = &factory; info.dataString = info.minorDataString = "test";
    TestWorker worker; worker.currentGame = &info; worker.OnProcessAttach();
    const auto file = fixture();
    gameAddr output = 0;
    worker.QueueCharacterImportation(file.data(), file.size(), 0, &output);
    std::promise<void> started, finish;
    auto entered = started.get_future(); auto release = finish.get_future().share();
    worker.importer().beforeImport = [&] { started.set_value(); release.wait(); };
    std::exception_ptr failure;
    std::thread operation([&] { try { worker.RunningUpdate(); } catch (...) { failure = std::current_exception(); } });
    const bool began = entered.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    bool rendererHasState = false, reportsBusy = false;
    if (began) {
        auto renderState = worker.TryLockState();
        rendererHasState = renderState.owns_lock();
        if (rendererHasState) reportsBusy = worker.IsBusy();
    }
    finish.set_value(); operation.join();
    if (failure) std::rethrow_exception(failure);
    check(began && rendererHasState && reportsBusy, "slow import held the rendering lock");
    check(output == 100 && !worker.IsBusy(), "slow import result was not published");
    AttachRetryBackoff retry;
    using Clock = std::chrono::steady_clock;
    auto now = Clock::time_point{};
    check(retry.Ready(now), "initial process attach delayed");
    for (const auto delay : {1, 2, 4, 8, 10, 10}) {
        retry.Failed(now);
        check(!retry.Ready(now + std::chrono::milliseconds(delay * 1000 - 1)), "process retry scans too frequently");
        now += std::chrono::seconds(delay);
        check(retry.Ready(now), "process retry deadline missed");
    }
    retry.Reset();
    check(retry.Ready(now), "explicit target change did not reset retry backoff");
}

static void queue(ImportSettings settings)
{
	GameFactory<TestImporter> factory;
	GameInfo info{};
	info.importer = &factory;
	info.dataString = info.minorDataString = "test";
	TestWorker worker;
	worker.currentGame = &info;
	worker.OnProcessAttach();
	auto file = fixture();
	gameAddr first = 0, second = 0;
	worker.QueueCharacterImportationOnBothPlayers(file.data(), file.size(), settings, &first, &second);
	file.back() = 99;
	worker.RunningUpdate();
	check(!worker.IsBusy() && first == 100 && second == 101, "queue lost player results");
	check(worker.importer().calls.size() == 2, "queue lost request");
	check(worker.importer().cleanups == (settings ? 2 : 0), "cleanup flag changed request count");
	Byte shortFile[8] = {'T', 'K', 'M', '2'};
	uint8_t progress = 0;
	check(worker.importer().Import(shortFile, sizeof(shortFile), 100, 0, progress) == ImportationErrcode_MovesetDataError,
		"pointer-sized header accepted");
	worker.StopThreadAndCleanup();
}

static void processMemory()
{
	GameProcess process;
	check(process.AttachById(GetCurrentProcessId(), PROCESS_VM_WRITE | PROCESS_VM_OPERATION), "attach to test process");
	uint32_t value = 12;
	auto address = reinterpret_cast<gameAddr>(&value);
	check(process.read<uint32_t>(address) == 12 && value == 12, "typed read modified memory");
	process.write<uint32_t>(address, 23);
	check(value == 23, "typed write failed");
	check(process.ResolveRemoteFunction(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW")) != 0,
		"forwarded export was not resolved in target");
	process.Detach();
	check(!process.readBytes(address, &value, sizeof(value)), "detached read reported success");
	check(!process.writeBytes(address, &value, sizeof(value)), "detached write reported success");
}

// Optional local fixtures can be supplied without redistributing game assets.
void RunHistoricalAnimationTests(const std::vector<Byte>& file);
void RunHistoricalEditorTests(const std::vector<Byte>& file);

static void historicalFile(const fs::path& path)
{
    MovesetFile source(path);
    source.Validate();
    auto firstConversion = source.ConvertToT7();
    check(firstConversion == source.ConvertToT7(), "conversion output was nondeterministic");
    RunHistoricalAnimationTests(firstConversion);
    RunHistoricalEditorTests(firstConversion);
	uint64_t size;
	auto bytes = Helpers::ReadMovesetFile(path.wstring(), size);
	check(raw::Moveset::ExpandFileBuffer(bytes, size), "historical file decompression");
	std::unique_ptr<Byte, decltype(&free)> owner(bytes, free);
	auto header = reinterpret_cast<const TKMovesetHeader*>(bytes);
	auto payload = bytes + header->moveset_data_start;
	auto payloadSize = size - header->moveset_data_start;
	if (header->gameId == GameId_T7) {
		auto packet = new Byte[size];
		memcpy(packet, bytes, size);
		SharedMemT7_Player player{};
		std::unique_ptr<Byte[]> installed(MovesetLoaderT7::ImportForOnline(player, packet, size));
		check(installed != nullptr, "historical T7 import");
	} else if (header->gameId == GameId_T5) {
		StructsT7::TKMovesetHeaderBlocks offsets{};
		check(MovesetConverter::T5ToT7().Convert(header, payload, payloadSize, offsets), "historical T5 conversion");
		std::unique_ptr<Byte[]> converted(payload);
		check(T7ImportUtils::ValidateT7Layout(offsets, payload, payloadSize), "converted T5 layout");
		auto table = reinterpret_cast<StructsT7_gameAddr::MovesetTable*>(payload + offsets.tableBlock);
		check(T7ImportUtils::ConvertMovesetIndexes(payload, reinterpret_cast<gameAddr>(payload), table, &offsets),
			"historical T5 relocation");
	} else {
		throw std::runtime_error("fixture game unsupported by this test");
	}
	std::cout << path.filename().string() << " passed\n";
}

class LifecycleWorker : public ThreadedClass
{
public:
    std::atomic_uint ticks{};
    ~LifecycleWorker() override { StopThreadAndCleanup(); }
    void Update() override {
        while (m_threadStarted) {
            ++ticks;
            std::this_thread::yield();
        }
    }
};

static void retailRecordConversion()
{
    StructsT7::Requirement always{}, end{}, distance{};
    end.condition = 881;
    distance.condition = 35; distance.param_unsigned = 0xffffffff;
    std::array requirements{always, distance, end};
    StructsT7::ExtraMoveProperty camera{}, terminator{};
    camera.starting_frame = 17; camera.id = 0x8001;
    std::array properties{camera, terminator};
    auto before = requirements;
    auto result = RetailT8::ConvertRecords(requirements, properties);
    check(result.Ok() && result.requirements.size() == 3 && result.properties.size() == 2, "retail record conversion failed");
    check(result.requirements[2].id == 1100 && result.requirements[1].parameters[0] == 0xffffffff,
        "retail IDs/parameter bits lost");
    check(result.requirements[1].parameters[1] == 0 && result.requirements[1].parameters[3] == 0 &&
        result.properties[0].parameters[4] == 0 && result.properties[0].frame == 17, "retail expansion lost defaults/frame");
    check(!memcmp(requirements.data(), before.data(), sizeof(requirements)), "retail conversion changed source");
    requirements[1].condition = 217;
    result = RetailT8::ConvertRecords(requirements, properties);
    check(!result.Ok() && result.issues[0].index == 1 && result.issues[0].id == 217 &&
        result.requirements.empty() && result.properties.empty(), "unsupported retail mapping yielded partial arrays");
    requirements = before;
    properties[0].value_unsigned = 1;
    result = RetailT8::ConvertRecords(requirements, properties);
    check(!result.Ok() && result.issues[0].property, "unsupported camera parameter accepted");
    // Property id 0 ends a T7 list whatever the frame: {frame 0, id 0x8001} is an ordinary property...
    properties[0] = camera; properties[1].id = 0x8001;
    result = RetailT8::ConvertRecords(requirements, properties);
    check(result.Ok() && result.properties.size() == 2 && result.properties[1].id != 0, "frame-0 property with an id was treated as a terminator");
    // ...and {frame 5, id 0} is an end, written as the retail {0, 0} end row.
    properties[1] = terminator; properties[1].starting_frame = 5;
    result = RetailT8::ConvertRecords(requirements, properties);
    check(result.Ok() && result.properties[1].id == 0 && result.properties[1].frame == 0, "id-0 terminator kept its frame");
    std::vector<StructsT7::Requirement> unsupported(300);
    for (auto& record : unsupported) record.condition = 217;
    auto bounded = RetailT8::ConvertRecords(unsupported, {});
    check(!bounded.Ok() && bounded.issueCount == 300 && bounded.issues.size() == 256,
        "retail conversion diagnostics are not bounded");
    for (const auto& rule : RetailT8::Rules) {
        if (rule.property) {
            auto value = camera; value.id = rule.source; value.value_unsigned = rule.minimum;
            if (!rule.source) value.starting_frame = 0;
            auto converted = RetailT8::ConvertRecords({}, {&value, 1});
            check(converted.Ok() && converted.properties[0].id == rule.target, "property rule failed");
        } else {
            auto value = always; value.condition = rule.source; value.param_unsigned = rule.minimum;
            auto converted = RetailT8::ConvertRecords({&value, 1}, {});
            check(converted.Ok() && converted.requirements[0].id == rule.target, "requirement rule failed");
        }
    }
}

static void lifecycle()
{
    LifecycleWorker worker;
    worker.StopThreadAndCleanup();
    for (int repeat = 0; repeat < 3; ++repeat) {
        worker.ticks = 0;
        worker.StartThread();
        worker.StartThread();
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!worker.ticks && std::chrono::steady_clock::now() < deadline)
            std::this_thread::yield();
        worker.StopThreadAndCleanup();
        worker.StopThreadAndCleanup();
        check(worker.ticks > 0, "worker did not restart");
    }
    GameImport detached;
    detached.QueueCharacterImportation(L"missing.tkmvst");
    detached.QueueCharacterImportation(nullptr, 0);
    check(!detached.IsBusy(), "detached importer queued an invalid request");
    check(EditorUtils::MustShiftId(14, -2, 10, 15), "reference to deleted list tail was not shifted");
    check(!EditorUtils::MustShiftId(11, -2, 10, 15), "retained list reference moved");
    check(EditorUtils::MustShiftId(16, -2, 10, 15), "following list reference was not shifted");
    check(!EditorUtils::MustShiftId(UINT64_MAX, -2, 10, 15), "null reference moved");
}

static void settings(const fs::path& directory)
{
    const auto previous = fs::current_path();
    fs::current_path(directory);
    Settings::SetFilePath(SETTINGS_FILE); // in the test folder, never the user's AppData settings
    try {
        {
            std::ofstream output(SETTINGS_FILE);
            output << "  spaced_key = 17\n# ignored\n = invalid\nplain = hello\n";
        }
        check(Settings::LoadFile(), "settings load failed");
        check(Settings::Get("spaced_key", 0) == 17, "indented settings key was corrupted");
        check(std::string(Settings::Get("plain", "")) == "hello", "settings value lost");
        Settings::Set("roundtrip", 42);
        check(Settings::LoadFile() && Settings::Get("roundtrip", 0) == 42, "settings save round trip failed");
        fs::remove(SETTINGS_FILE);
    } catch (...) {
        fs::current_path(previous);
        Settings::SetFilePath({});
        throw;
    }
    fs::current_path(previous);
    Settings::SetFilePath({});
}

std::vector<Byte> SemanticFixture();

void BenchmarkEditor(const fs::path& path);
void RunExportGraphTests();
void RunRetailSnapshotTests();
void RunRetailImportTests();
void ExportLiveRetail(const fs::path& directory);
void CheckLegacyExportImage(const fs::path& manifest);

int main(int argc, char** argv)
{
	try {
        if (argc == 2 && std::string(argv[1]) == "--test-file-jobs") { RunFileJobTests(); return 0; }
        if (argc == 2 && std::string(argv[1]) == "--test-offline-editor") {
            RunEditorMutationTests(); RunSpecializedEditorTests(); RunEditorWorkflowTests();
            std::cout << "Offline editor tests passed\n"; return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--test-game-static-data") { RunGameStaticDataTests(); return 0; }
        if (argc == 2 && std::string(argv[1]) == "--test-address-scanner") { RunAddressScannerTests(); std::cout << "Address scanner tests passed\n"; return 0; }
        if (argc == 2 && std::string(argv[1]) == "--test-retail-export") { RunRetailSnapshotTests(); std::cout << "Retail export tests passed\n"; return 0; }
        if (argc == 2 && std::string(argv[1]) == "--test-semantic") { RunSemanticValidationTests(); std::cout << "Semantic validation tests passed\n"; return 0; }
        if (argc == 2 && std::string(argv[1]) == "--test-retail-import") { RunRetailImportTests(); std::cout << "Retail import tests passed\n"; return 0; }
        if (argc == 3 && std::string(argv[1]) == "--export-retail-live") { ExportLiveRetail(argv[2]); return 0; }
        if (argc == 3 && std::string(argv[1]) == "--benchmark-editor") {
            BenchmarkEditor(argv[2]); return 0;
        }
        if (argc == 3 && (std::string(argv[1]) == "--write-semantic-fixture" || std::string(argv[1]) == "--write-t7-retail-fixture")) {
            auto bytes = std::string(argv[1]) == "--write-t7-retail-fixture" ? T7RetailConvertibleFixture() : SemanticFixture();
            std::ofstream output(argv[2], std::ios::binary);
            output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            return output ? 0 : 1;
        }
        if (argc == 3 && std::string(argv[1]) == "--write-fixture") {
            auto bytes = nativeFixture();
            std::ofstream output(argv[2], std::ios::binary);
            output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            output.close();
            check(static_cast<bool>(output), "cannot write fixture");
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "--check-legacy-export") {
            CheckLegacyExportImage(argv[2]); return 0;
        }
        retailRecordConversion();
        RunT7ToRetailT8Tests();
        RunT8ToT7Tests();
        RunMovesetListSortTests();
        RunAddressScannerTests();
        RunRetailPanmTests();
        RunRetailSnapshotTests();
        RunRetailImportTests();
        RunExportGraphTests();
        RunProcessWriteTests();
        RunFileJobTests();
        RunEditorWorkflowTests();
        RunSemanticValidationTests();
        RunSpecializedEditorTests();
        for (int i = 1; i < argc; ++i) historicalFile(argv[i]);
		RunEditorMutationTests();
		RunAnimationTests();
		lifecycle();
		codecs();
		layout();
		lastAnimation();
		onlineImport();
		hook();
		check(Http::Sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "release asset SHA-256");
        operationDoesNotBlockRendering();
		queue(0);
		queue(ImportSettings_FreeUnusedMovesets);
		processMemory();
		auto directory = fs::temp_directory_path() / ("TKMovesets-test-" + std::to_string(GetCurrentProcessId()));
		check(fs::create_directory(directory), "cannot create test directory");
		files(directory);
		settings(directory);
		fs::remove(directory);
		return 0;
	} catch (const std::exception& e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
