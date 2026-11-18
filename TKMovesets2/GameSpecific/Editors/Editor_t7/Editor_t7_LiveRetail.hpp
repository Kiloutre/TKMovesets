#pragma once
#include "GameProcess.hpp"
#include "GameData.hpp"
#include "Structs_t7.h"
#include "T7ToRetailT8.hpp"
#include "GameInfo.hpp"
#include <set>
#include <string>
#include <utility>
#include <vector>

// Live edition of a T7 moveset imported into retail T8, without converting the whole moveset again on every edit.
//
// The T7 editor's live writers (Editor_t7_LiveEdition*.cpp) target T7 memory layout. For a T8 target they write into
// T7LiveShadow instead: a copy of the T7 moveset as it was imported, kept in file layout (row indices). Each edited
// T7 row is converted alone (T7ToRetailT8::LiveConversion::UpdateRow) and the changed retail records are written into
// the moveset the game holds (RetailLiveEdit::Sync), with the importer's identity, profile and animation fixups.
// Edits a single row cannot carry (a property with no retail equivalent, a moved list end, a new list, a move name or
// animation) stop live edition with the reason: they need a new import.

// A T7 moveset (payload: MovesetInfo onwards) at a fixed virtual address. Writes land in the copy; a pointer field
// written as an address is stored back as the row index the T7 file holds.
class T7LiveShadow : public GameProcess {
public:
    static constexpr gameAddr Base = 0x7e0000000000ULL;
    T7LiveShadow(const StructsT7::TKMovesetHeaderBlocks& blocks, std::vector<Byte> payload);
    gameAddr Address() const { return Base; }
    // File-layout bytes of a T7 row.
    std::span<const Byte> Row(size_t table, uint64_t row) const;
    // Rows written since the last call, in table order; a reason when a write could not be read back as T7 data.
    std::set<std::pair<size_t, uint64_t>> TakeDirty();
    const std::string& Invalid() const { return m_invalid; }
    // Takes the rows of `t7File` (a complete T7 moveset file, as the editor holds it after Apply) that differ from the
    // copy, as if the live writers had written them. A table that changed size makes the copy invalid.
    void SyncFrom(std::span<const Byte> t7File);
protected:
    bool ReadMemoryRaw(gameAddr address, void* data, size_t size) const override;
    bool WriteMemoryRaw(gameAddr address, const void* data, size_t size) override;
    gameAddr AllocateMemoryRaw(size_t, bool) override { return 0; }
    bool ReleaseMemoryRaw(gameAddr) override { return false; }
    bool SuspendProcessRaw() override { return true; }
    bool ResumeProcessRaw() override { return true; }
private:
    StructsT7::TKMovesetHeaderBlocks m_blocks;
    std::vector<Byte> m_payload;
    std::array<uint64_t, 19> m_offset{}, m_count{};
    std::set<std::pair<size_t, uint64_t>> m_dirty;
    std::string m_invalid;
    void StoreIndex(size_t table, uint64_t row, uint32_t field);
};

class T7RetailLive {
public:
    // `t7File`: the complete T7 moveset file as it is imported (header included). Converts it once, fully.
    explicit T7RetailLive(std::span<const Byte> t7File);
    // The converted container to import; the game must receive these bytes for Bind to be valid.
    const std::vector<Byte>& Container() const { return m_conversion.File(); }
    // The moveset the import of Container() published on `slot` of the game `game` (retail T8).
    void Bind(gameAddr root, uint8_t slot, const GameInfo* game);
    bool Bound() const { return m_root != 0; }
    gameAddr Root() const { return m_root; }
    T7LiveShadow& Shadow() { return m_shadow; }
    // Converts the rows the shadow received and writes their records into the game. Throws (with the reason) when
    // the edit needs a new import; the binding is then unusable.
    void Publish(GameProcess& process, GameData& game);
    // Records written by the last Publish.
    size_t LastRecords() const { return m_lastRecords; }
private:
    T7ToRetailT8::LiveConversion m_conversion;
    T7LiveShadow m_shadow;
    std::vector<Byte> m_published; // the container as the game holds it (tables only are kept current)
    gameAddr m_root = 0;
    uint8_t m_slot = 0;
    const GameInfo* m_game = nullptr;
    size_t m_lastRecords = 0;
};
