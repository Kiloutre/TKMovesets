#include "GameProcess.hpp"
#include "Importer.hpp"
#include "Importer_t7/Importer_t7.hpp"
#include "Editor_t7/Editor_t7.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <thread>

std::vector<Byte> SemanticFixture();
namespace {
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
class Memory : public GameProcess {
public:
    std::map<gameAddr, std::vector<Byte>> regions;
    int writes = 0, reads = 0, failWrite = 0, failRead = 0, detachWrite = 0;
    bool failRollback = false;
    std::vector<gameAddr> order;
    gameAddr next = 0x100000;
    static constexpr gameAddr target = 0x1000;
    Memory() { regions[target].assign(128, 0xa5); }
    auto Find(gameAddr address, size_t size) {
        auto it = regions.upper_bound(address);
        if (it == regions.begin()) return regions.end();
        --it;
        return address - it->first <= it->second.size() && size <= it->second.size() - (address - it->first) ? it : regions.end();
    }
protected:
    bool ReadMemoryRaw(gameAddr address, void* data, size_t size) const override {
        auto& self = const_cast<Memory&>(*this);
        if (++self.reads == failRead) return false;
        auto it = self.Find(address, size); if (it == self.regions.end()) return false;
        memcpy(data, it->second.data() + address - it->first, size); return true;
    }
    bool WriteMemoryRaw(gameAddr address, const void* data, size_t size) override {
        ++writes; order.push_back(address);
        if (writes == detachWrite) { Detach(); return false; }
        auto it = Find(address, size); if (it == regions.end()) return false;
        if (writes == failWrite || (failRollback && writes > failWrite)) {
            if (writes == failWrite && size) memcpy(it->second.data() + address - it->first, data, (size + 1) / 2);
            SetLastError(ERROR_PARTIAL_COPY); return false;
        }
        memcpy(it->second.data() + address - it->first, data, size); return true;
    }
    gameAddr AllocateMemoryRaw(size_t size, bool) override {
        auto address = next; next += 0x10000;
        regions[address].resize(size); return address;
    }
    bool ReleaseMemoryRaw(gameAddr address) override { return regions.erase(address) != 0; }
};
class Upload : public Importer {
public:
    using Importer::Importer;
    bool failAfterPlanning = false;
    bool CanImport() override { return true; }
    void CleanupUnusedMovesets() override {}
    gameAddr GetCharacterAddress(uint8_t) override { return Memory::target; }
    gameAddr GetMovesetAddress(uint8_t) override { return 0; }
protected:
    ImportationErrcode_ ImportMovesetData(const TKMovesetHeader*, Byte*, uint64_t, gameAddr player,
        ImportSettings, uint8_t& progress) override {
        auto memory = m_process.allocateMem(32);
        if (!memory) return ImportationErrcode_GameAllocationErr;
        // Deliberately plan pointer publication before upload: commit must reorder these.
        m_process.writeUInt64(player, memory);
        const uint32_t payload = 0x12345678;
        m_process.writeBytes(memory, &payload, sizeof(payload));
        Check(m_process.readUInt32(memory) == payload, "write batch lost read-your-writes");
        m_process.writeUInt32(player + 16, payload);
        lastLoaded.address = memory; lastLoaded.size = 32; progress = 100;
        return failAfterPlanning ? ImportationErrcode_MovesetDataError : ImportationErrcode_Successful;
    }
};
DWORD WINAPI DelayedRead(void* argument) {
    Sleep(40);
    return *static_cast<const uint32_t*>(argument);
}
}
void RunProcessWriteTests() {
    {
        Memory memory;
        Check(memory.writeUInt16(Memory::target + 4, 0x1234), "checked scalar write failed");
        Check(memory.regions.at(Memory::target)[3] == 0xa5 && memory.regions.at(Memory::target)[6] == 0xa5, "narrow write damaged adjacent bytes");
        Check(!memory.writeUInt32(0xdead, 42), "invalid scalar write reported success");
        GameProcess::WriteBatch batch(memory);
        Check(memory.writeUInt32(Memory::target, 1), "staging write failed");
        memory.writeUInt16(Memory::target + 1, 2);
        const uint32_t expected = 0x00000201;
        Check(memory.readUInt32(Memory::target) == expected, "overlapping write overlay is incorrect");
        Check(batch.Commit().ok && memory.readUInt32(Memory::target) == expected, "overlapping write commit failed");
    }
    {
        Memory memory; const auto before = memory.regions.at(Memory::target);
        {
            GameProcess::WriteBatch batch(memory);
            memory.writeUInt64(Memory::target, 100);
            memory.allocateMem(12);
        }
        Check(memory.regions.at(Memory::target) == before && memory.allocatedMemory.empty(), "abandoned plan changed memory or leaked allocation");
    }
    auto file = SemanticFixture();
    for (int failure = 0; failure <= 3; ++failure) {
        Memory memory; const auto before = memory.regions.at(Memory::target);
        memory.failWrite = failure;
        GameData game; GameInfo info{}; info.gameId = 0; info.dataString = info.minorDataString = "t7";
        Upload importer(memory, game, &info);
        importer.lastLoaded.address = 0x777;
        uint8_t progress = 0;
        auto result = importer.Import(file.data(), file.size(), Memory::target, 0, progress);
        if (!failure) {
            Check(result == ImportationErrcode_Successful && importer.lastLoaded.address != 0x777 && progress == 100, "successful upload failed");
            Check(memory.order.front() == importer.lastLoaded.address, "pointer was published before payload upload");
            Check(memory.readUInt64(Memory::target) == importer.lastLoaded.address, "successful pointer installation failed");
        } else {
            Check(result == ImportationErrcode_GameWriteError && importer.lastLoaded.address == 0x777, "failed import reported success or replaced lastLoaded");
            Check(memory.regions.at(Memory::target) == before && memory.allocatedMemory.empty() && memory.regions.size() == 1,
                "partial write was not restored or private allocation leaked");
        }
    }
    {
        Memory memory; const auto before = memory.regions.at(Memory::target);
        GameData game; GameInfo info{}; info.gameId = 0; info.dataString = info.minorDataString = "t7";
        Upload importer(memory, game, &info); importer.failAfterPlanning = true;
        uint8_t progress = 0;
        Check(importer.Import(file.data(), file.size(), Memory::target, 0, progress) == ImportationErrcode_MovesetDataError,
            "planning failure returned wrong error");
        Check(memory.regions.at(Memory::target) == before && memory.order.empty() && memory.allocatedMemory.empty(),
            "failed planning performed remote writes");
    }
    for (const bool fail : {false, true}) {
        Memory memory; memory.failWrite = fail ? 1 : 0;
        const auto before = memory.regions.at(Memory::target);
        GameData game; GameInfo info{}; info.gameId = 0; info.dataString = info.minorDataString = "t7";
        ImporterT7 importer(memory, game, &info);
        uint8_t progress = 0;
        auto result = importer.Import(file.data(), file.size(), Memory::target, ImportSettings_BasicLoadOnly, progress);
        Check(result == (fail ? ImportationErrcode_GameWriteError : ImportationErrcode_Successful),
            "native T7 basic upload returned the wrong result");
        Check(memory.regions.at(Memory::target) == before, "basic upload published player or movelist pointers");
        Check(fail ? memory.allocatedMemory.empty() : memory.allocatedMemory.size() == 1, "native T7 upload allocation lifetime failed");
    }
    {
        Memory memory; memory.failRead = 1;
        GameProcess::WriteBatch batch(memory);
        memory.writeUInt32(Memory::target, 11);
        Check(!batch.Commit().ok && memory.order.empty(), "failed preflight read still wrote memory");
    }
    {
        Memory memory; const auto before = memory.regions.at(Memory::target);
        GameProcess::WriteBatch batch(memory);
        memory.writeUInt32(Memory::target, 11);
        memory.readUInt32(0xdead);
        Check(!batch.Commit().ok && memory.regions.at(Memory::target) == before, "failed planning read was ignored");
    }
    {
        Memory memory; memory.failWrite = 2; memory.failRollback = true;
        GameData game; GameInfo info{}; info.gameId = 0; info.dataString = info.minorDataString = "t7";
        Upload importer(memory, game, &info); importer.lastLoaded.address = 0x777;
        uint8_t progress = 0;
        Check(importer.Import(file.data(), file.size(), Memory::target, 0, progress) == ImportationErrcode_GameRollbackError,
            "incomplete rollback was hidden");
        Check(memory.allocatedMemory.size() == 1 && importer.lastLoaded.address == 0x777, "uncertain allocation was freed");
        auto address = memory.allocatedMemory.front().first;
        memory.freeMem(address); memory.FreeOldGameMemory(true);
        Check(memory.regions.contains(address), "possibly published allocation was released");
    }
    {
        Memory memory; memory.detachWrite = 1;
        GameProcess::WriteBatch batch(memory);
        auto allocation = memory.allocateMem(8);
        memory.writeUInt64(allocation, 3); memory.writeUInt64(Memory::target, allocation);
        Check(!batch.Commit().ok && memory.order.size() == 1, "detach during upload caused writes into another attachment");
    }
    {
        Memory memory;
        GameProcess::WriteBatch batch(memory);
        memory.allocateMem(8); memory.writeUInt32(Memory::target, 1);
        memory.Detach();
        Check(!batch.Commit().ok && memory.order.empty() && memory.allocatedMemory.empty(), "stale plan survived detach");
    }
    {
        Memory memory; GameData game; GameAddressesFile addresses; game.SetProcess(&memory, &addresses);
        GameInfo info{}; info.gameId = 0; info.dataString = info.minorDataString = "t7";
        EditorT7 editor(memory, game, &info);
        auto copy = static_cast<Byte*>(malloc(file.size())); Check(copy != nullptr, "live fixture allocation failed");
        memcpy(copy, file.data(), file.size());
        Check(editor.LoadMoveset(copy, file.size()), "live edit fixture load failed");
        editor.live_loadedMoveset = Memory::target;
        char buffer[] = "100";
        EditorInput field; field.name = "condition"; field.flags = EditorInput_U32; field.buffer = buffer; field.bufsize = sizeof(buffer);
        bool rejected = false;
        try { editor.Live_OnFieldEdit(TEditorWindowType_Requirement, 0, &field); }
        catch (const std::exception&) { rejected = true; }
        Check(rejected && !editor.live_loadedMoveset && memory.order.empty(), "failed live edit retained its binding or wrote invalid memory");
        Check(editor.GetCurrentMoveID(0) == 0, "failed current-move read escaped bounds handling");
    }
    {
        GameProcess process;
        Check(process.AttachById(GetCurrentProcessId(), PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD),
            "cannot attach to native test process");
        auto memory = process.allocateMem(2 * sizeof(uint32_t));
        Check(memory && process.writeUInt32(memory + 4, 42), "native thread argument preparation failed");
        auto result = process.createRemoteThread(reinterpret_cast<gameAddr>(&DelayedRead), memory + 4, true, nullptr, 1);
        Check(result == GameProcessThreadCreation_WaitTimeout && process.PendingRemoteThreadCount() == 1, "timed-out thread was not retained");
        process.freeMem(memory);
        Check(!process.allocatedMemory.empty(), "pending thread argument was released early");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (process.PendingRemoteThreadCount() && std::chrono::steady_clock::now() < deadline) {
            Sleep(5); process.FreeOldGameMemory(true);
        }
        Check(!process.PendingRemoteThreadCount() && process.allocatedMemory.empty(), "completed remote thread/argument was not reclaimed");
        process.Detach();
    }
    {
        // A batch planned by one thread (worker) must not capture writes of another thread (DLL injection)
        Memory memory;
        GameProcess::WriteBatch batch(memory);
        bool written = false;
        std::thread other([&] { written = memory.writeUInt32(Memory::target + 8, 0x55); });
        other.join();
        Check(written && memory.order.size() == 1 && memory.regions.at(Memory::target)[8] == 0x55,
            "another thread's write was staged into this thread's batch");
        Check(batch.Commit().ok, "another thread's write failed this thread's batch");
    }
    {
        // Pointer paths: any null pointer (the last one included) yields null, never the final offset; baseAddr applies once per hop
        const auto iniPath = std::filesystem::temp_directory_path() / "TKMovesets2_ptrpath_test.ini";
        {
            std::ofstream ini(iniPath, std::ios::binary);
            ini << "ptrtest_onehop = 0x1000, 0x40\n"
                << "ptrtest_twohop = 0x1000, 0x8, 0x10\n"
                << "ptrtest_based = 0xF00, 0x8, 0x4\n";
        }
        Memory memory; GameAddressesFile addresses; addresses.ReloadFrom(iniPath);
        std::error_code removeError; std::filesystem::remove(iniPath, removeError);
        GameData game; game.SetProcess(&memory, &addresses);
        game.gameKey = game.minorGameKey = "ptrtest";
        auto& region = memory.regions.at(Memory::target);
        std::fill(region.begin(), region.end(), Byte(0));
        Check(game.ReadPtrPath("onehop") == GAME_ADDR_NULL, "null last pointer returned the final offset");
        Check(game.ReadPtrPath("twohop") == GAME_ADDR_NULL, "null first pointer of a multi-hop path was not null");
        Check(memory.writeUInt64(Memory::target, Memory::target) && game.ReadPtrPath("twohop") == GAME_ADDR_NULL,
            "null last pointer of a multi-hop path returned the final offset");
        Check(game.ReadPtrPath("onehop") == Memory::target + 0x40, "valid one-hop pointer path resolved incorrectly");
        Check(memory.writeUInt64(Memory::target + 0x8, 0x5000) && game.ReadPtrPath("twohop") == 0x5010,
            "valid two-hop pointer path resolved incorrectly");
        // Emulated memory: game address X lives at host address baseAddr + X
        game.baseAddr = 0x100;
        Check(memory.writeUInt64(Memory::target, 0xF10) && memory.writeUInt64(Memory::target + 0x18, 0x2000) &&
            game.ReadPtrPath("based") == 0x2004, "base address was applied more than once on a pointer path");
    }
}
