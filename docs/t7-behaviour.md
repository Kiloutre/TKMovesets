# Tekken 7 behaviour

This page describes how the tool works with Tekken 7: how it finds players and movesets, extraction from memory and
from the game files, import, live editing, the training move list, online and persistent play, and the import of
movesets from older games. Record layouts are in [t7-structures.md](t7-structures.md).

Supported build: the latest Steam version of Tekken 7 (`TekkenGame-Win64-Shipping.exe`, Steam app 389730).

## Game registration

`TKMovesets2/GameSpecific/Games.cpp`, entry `Tekken 7`:

| Field | Value |
| --- | --- |
| process | `TekkenGame-Win64-Shipping.exe` |
| file name prefix | `T7_` |
| `gameId` / `minorVersion` | `GameId_T7` / `GameVersions::T7::NONE` |
| players | 2, pointer size 8, little-endian |
| flags | `GameFlag_MovesetLiveEditable` |
| address key | `t7` |
| importable files | T7, TTT2, TREV, T6, T5 (incl. T5 DR), retail T8 |
| online-importable files | T7, TTT2, TREV, T6 |
| classes | `ExtractorT7`, `ImporterT7`, `EditorT7`, `EditorVisuals_T7`, `OnlineT7` |

## Addresses (`game_addresses.ini`, `t7` section)

File: `TKMovesets2/Resources/game_addresses.ini`. Read through `GameData` (`GameInteractions/GameProcess/GameData.cpp`);
the code asks for a name without the `t7_` prefix (`m_game.GetValue("motbin_offset")`, `ReadPtrPath("p1_addr")`).

Line kinds:

- `val:NAME = X`: a plain value (`GameData::GetValue`).
- `str:NAME = ...`: a string. For T7 these are byte patterns searched by the injected DLL (`Sig::find`).
- `NAME = A, B, ..., Z`: a pointer path (`GameData::ReadPtrPath`). `+A` is relative to the main module base. With more
  than one element: read a pointer at `A`, then for each middle element read a pointer at `previous + element`, then
  add the last element. A null pointer on the way returns null.

| Key | Value | Use |
| --- | --- | --- |
| `t7_p1_addr` | `+0x34DF630, 0` | player 1 structure (pointer at base + 0x34DF630) |
| `t7_pid_addr` | `+0x34D55A0, 0, 0, 0x78` | local player side; the DLL reads an int there, 0 = local player is P1 |
| `val:t7_playerstruct_size` | 0x3670 | player 2 = player 1 + this |
| `val:t7_motbin_offset` | 0x1520 | moveset pointers in the player structure (5 qwords, 0x1520 .. 0x1540) |
| `val:t7_chara_id_offset` | 0xD8 | character id (int16 read) |
| `val:t7_currmove` | 0x220 | pointer to the current `Move` |
| `val:t7_playerid_offset` | 0xD4 | player index (0 or 1) |
| `t7_movelist_manager_addr` | `+0x34477e0, 0x40` | array of two `MvlManager` (training move list); null outside training |
| `val:t7_static_camera_offset` | 0x14A0 | two MOTA pointers (camera slots 8 and 9) in the player structure |
| `val:t7_currmove_timer` | 0x1D4 | current move frame |
| `val:t7_currmove_id` | 0x350 | current move id (uint16) |
| `val:t7_nextmove_addr` | 0xDA0 | pointer to the next move to play |
| `val:t7_character_id_condition` | 217 | requirement condition "character id is" |
| `val:t7_req_end` | 881 | requirement list terminator |
| `t7_steamLobbyId_addr` | `+0x34CEEF8` | Steam lobby id (DLL, online) |
| `str:t7_f_ApplyNewMoveset` | bytes | function that installs a moveset on a player after a loading screen (DLL hook) |
| `str:t7_f_GetPlayerFromID` | bytes | function holding the player list (DLL) |
| `str:t7_f_ExecuteExtraprop` | bytes | function that runs one property (DLL) |
| `str:t7_f_NetInterCS::MatchedAsClient` / `MatchedAsHost` | bytes | online match start (DLL hooks) |
| `str:t7_f_NetManager::GetSyncBattleStart` | bytes | online battle start gate (DLL hook) |
| `str:t7_f_NetManager::f_DiarkisErrString` | bytes | same bytes as `GetSyncBattleStart`; not read by the T7 loader |
| `t7_f_Log_addr`, `str:t7_f_Log` | `+0x556170`, bytes | game log function, hooked in debug builds only |

The file carries `val:global_addr_version` (11) and also the T7 <-> T8 conversion rules section; the rules are used by
the import profile and the T8 converters, see `conversion-rules.md`.

### Locating players and movesets

- Player `n`: `ReadPtrPath("p1_addr") + n * playerstruct_size` (`ExtractorT7::GetCharacterAddress`,
  `ImporterT7::GetCharacterAddress`).
- Moveset of a player: the qword at `player + motbin_offset` (`ImporterT7::GetMovesetAddress`). The player holds five
  copies at `+0x1520` .. `+0x1540`. The first is the moveset the player returns to for alias moves; the others follow
  the move currently playing (which can come from the opponent's moveset).
- Training move list of a player: `ReadPtrPath("movelist_manager_addr") + playerId * 0x80`, field `mvlHead` (+0x20).
- `ExtractorT7::CanExtract`: P1 exists, its moveset has `isInitialized == 1`, its current move's animation (`Move +
  0x10`) starts with type byte 0x64 or 0xC8. `ImporterT7::CanImport` does the current-move check on both players.

## Extraction from memory

Entry: `ExtractorT7::Extract` (`GameSpecific/Extractors/Extractor_t7/Extractor_t7.cpp`), run on the extraction thread
by `GameInteractions/GameExtract.cpp`. The graph reader is `T7Export::ReadSnapshot` and the file writer
`T7Export::Serialize` (`T7ExportSnapshot.cpp`).

Steps:

1. Read the moveset pointer at `player + motbin_offset`. With `ExtractSettings_DisplayableMovelist`, also read the
   player's `MvlManager::mvlHead` (none when the manager pointer is null).
2. Read the first 0x2E0 bytes of `MovesetInfo` (12 MOTA slots; the 13th is never read).
3. Copy each of the 19 tables independently from its own pointer and count. Tables may be anywhere in memory; each
   one must be contiguous. They are written in table order, each 8-aligned.
4. Turn every reference into a row index (`Index()`); a pointer that is not a row start of its target table aborts.
5. Copy names and metadata strings, deduplicated by address. The stored character name gets the `Tkm:` prefix.
6. Copy each distinct move animation once. Its size comes from the bounded animation layout parser
   (`TAnimUtils::ReadAnimationLayoutFrom` through a cached probe). Big-endian animations are byte-swapped.
7. Copy the MOTA slots selected by `ExtractSettings` bits 0..11. A slot that is not a `MOTA` is skipped (with
   `ExtractSettings_UnknownMotas` the export fails instead, because its length cannot be known). An archive with an
   invalid member animation is dropped and listed in `discardedMotas`. Big-endian archives are byte-swapped.
8. Copy the training move list (`ReadTraining`), converting its offsets (see
   [t7-structures.md](t7-structures.md#training-move-list-mvlt)).
9. Run `T7ImportUtils::ValidateT7Semantics` on the result, then re-read the header and check that no pointer, count or
   string pointer changed during the read.
10. `Serialize` builds the file (header, import profile property list, block list, payload) and the CRC.
11. Check the player still points to the same moveset, compress (`ExtractorUtils::GetCompressionAlgorithm`: compact
    zstd, LZMA, LZ4 or none) and write with `FileIO::WriteAtomically`.

Limits: the whole export must fit 256 MiB (`Limit` in `T7ExportSnapshot.cpp`). Any unreadable region aborts.

File name: `Extractor::GetFilepath`, `T7_<display name>.tkmvst` in the extraction directory. The display name is the
character name without `Tkm:` and brackets, title-cased, at most 31 characters (`DisplayName` in `Extractor_t7.cpp`).
An existing file gets a numbered suffix unless `ExtractSettings_OVERWRITE_SAME_FILENAME` is set. The header's
`characterId` is the int16 at `player + chara_id_offset`.

## Extraction from the game files

Code: `GameSpecific/Extractors/Extractor_t7/T7DiskMoveset.cpp` (namespace `T7Disk`). GUI: the extraction menu entry
for building every T7 moveset from the game files (`Layouts/Submenus/T8DiskExtraction.cpp`, shared with T8). Headless:
`build-disk-movesets-t7 TKDATA_BIN DIRECTORY [CODE,...|all] [CODEC]`, default codec LZ4 (`main/headless.cpp`). No
game process is needed.

Archive: `TekkenGame/Content/Binary/tkdata.bin` (`T7Disk::TkdataPath`; the installed executable is found with
`ExeSignatures::FindInstalledT7Executable`).

- Magic `_TEKKEN7CSFILES_` at offset 0.
- File list: at `size - 0x68` two uint64 (offset, stored size). The list is AES-256-ECB encrypted (key in
  `T7DiskMoveset.cpp`) and zlib-compressed when it starts with 0x78. It starts with `tsLniBNB`, then count (+8), info
  offset (+16) and names offset (+24). Entries are 64 bytes: +8 flags (bit 0 = zlib), +24 offset (data at
  `offset + 0x10`), +32 stored size, +40 size. Names are NUL-separated, in clear.

Files used per character code (`T7Disk::Characters` lists `mothead\bin\<code>mot.bin`, without `test`):

| File | Contents |
| --- | --- |
| `mothead\bin\<code>mot.bin` | the moveset (`HID` at offset 4), `MovesetInfo` layout, offsets relative to +0x2E8 |
| `mothead\mask\<code>rotsm.arc` | `{ uint32 count; count x { uint32 offset; uint32 size; } }`, up to 6 MOTAs = slots 1, 3, 5, 7, 9, 11 |
| `mothead\mask\common.arc` | one MOTA of shared animations, read once per batch |
| `mothead\movelist\<code>.mvl` | training move list; absent for `bs7`, `ja4`, `ykz`, `zak` |

`T7Disk::BuildFile` relocates the motbin as the game holds it in memory, then runs the memory exporter on it through
an in-memory process (`ImageProcess`), with every MOTA slot and the move list selected:

- `MovesetInfo` strings, the 19 table pointers and the even MOTA slots become absolute; odd slots point into
  `rotsm.arc`; slot 12 is cleared; `isInitialized` is set to 1.
- Every row index becomes a pointer; `-1` becomes null. A move's secondary cancel lists are null when their gate
  (`+0x30`, `+0x40`, `+0x50`) is below 1. A projectile reference of 0 is null.
- Move `anim_addr` is a key: bit 15 = entry `key & 0x3FFF` of `common.arc`, bit 14 = entry of MOTA slot 1, no flag =
  entry of MOTA slot 0. `anim_len` is set to that animation's frame count.
- Cancel option bits are computed as listed in [t7-structures.md](t7-structures.md#cancels-and-grouped-cancels).
- Projectile `+0x78` and `+0x7C` are filled from `+0x30 * 2` and `+0x34` when 0.
- Move list offsets are converted to the in-memory form, and the low 16 bits of `MvlHead::_unk0x4` go from 5 to 1000.
- `characterId` is the most common `moveId_val2` of the moves.

`T7Disk::WriteMovesetFile` writes `T7_<display name>.tkmvst`. If that file exists and holds another character (other
`gameId` or `characterId`), it writes `T7_<display name> (<code>).tkmvst` instead. If the chosen file exists, it is
left alone (`Report::existed`).

## Import

Entry: `GameInteractions/GameImport.cpp` queues requests; `Importer::Import` (`GameSpecific/Importers/Importer.cpp`)
reads the file; `ImporterT7::ImportMovesetData` dispatches by the file's `gameId`; T7 files go to
`ImporterT7::_Import_FromT7` (`Importer_t7/Importer_t7.cpp`).

`ImportSettings` (`Importer.hpp`):

| Flag | Effect |
| --- | --- |
| `EnforceCurrentAliasesAsDefault` | copy `current_aliases` over `orig_aliases` (default is the reverse) |
| `ApplyInstantly` | end the current move and play alias 0x8001 at once |
| `FreeUnusedMovesets` | run `CleanupUnusedMovesets` after the import |
| `BasicLoadOnly` | load the moveset only: no player pointer, no character fixes, no MOTA fill, no move list |
| `ImportOriginalData` | also copy the whole original file into the game (online sending) |
| `UseMotaMask` | bits 16..27 select which file MOTAs to use |

Default: `ApplyInstantly | FreeUnusedMovesets`.

`_Import_FromT7` steps:

1. Check the block list and run `ValidateT7Semantics`.
2. Allocate the payload size in the game (`GameProcess::allocateMem`, `VirtualAllocEx`, read-write).
3. Relocate in the local copy: metadata strings (`CorrectMovesetInfoValues`), every index to a pointer
   (`ConvertMovesetIndexes`).
4. Unless basic load, `ApplyCharacterIDFixes`:
   - import profile requirement overrides (`ImportProfile::RequirementOverride`): a decided requirement becomes
     condition 0 (always true) or `t8_to_t7_req_never_true` (901); stripped properties get `starting_frame = 0x7FFF`;
   - requirements with condition 217, and start/end properties with property 217: a parameter equal to the moveset's
     character id becomes the target player's character id; any other becomes target id + 10. The moveset's id is the
     header `characterId`, or the profile's fixed character.
5. Turn the table offsets into pointers (`ConvertMovesetTableOffsets`).
6. MOTA list (`ConvertMotaListOffsets`): a selected slot holding a valid `MOTA` points into the allocation. Other slots
   take the player's current MOTA pointer, or stay `MOVESET_ADDR_MISSING` in basic load.
7. If the file has an `MVLT` block: convert its offsets (`ConvertDisplayableMovelistOffsets`) and, unless basic load,
   write `mvlHead`, `displayableEntriesCount` (4 bytes) and `mvlDisplayableBlock` to the player's `MvlManager`
   (`ImportMovelist`; skipped when the manager pointer is null).
8. Aliases (see flags above).
9. Write the whole payload to the allocation.
10. Unless basic load: write the moveset address to `player + motbin_offset`; write MOTA slots 8 and 9 to `player +
    static_camera_offset` (`WriteCameraMotasToPlayer`); with `ApplyInstantly`, `ForcePlayerMove(player, moveset,
    32769)`: write the moveset to the four other moveset qwords, resolve the alias, set `currmove_timer` to 99999,
    `nextmove_addr` to the move and `currmove_id` to its id.
11. Record `lastLoaded` (crc32, character id, address, size).

### Write batch and rollback

`Importer::ImportChecked` wraps the T7 import in `GameProcess::WriteBatch` (`GameProcess.hpp`,
`GameProcess/RemoteWriteBatch.cpp`).

- While a batch is active, writes from the planning thread are staged (`WriteBatch::Stage`); reads see the staged
  bytes (`Overlay`). Allocations are real and recorded as new allocations of the batch.
- `Commit`: check expected bytes (`ExpectBytes`); read the current bytes of every shared target (outside new
  allocations); write and read back every private upload (inside new allocations), whatever their place in the plan;
  then write the shared targets (training manager, player structure) in plan order.
- If a shared write fails, the earlier shared writes are restored in reverse order. Result: `GameWriteError`. If a
  restore also fails, the new allocations are pinned (never freed) and the result is `GameRollbackError`.
- A batch not committed, with complete rollback, releases its new allocations. A batch from an older attachment
  (`m_generation`) writes nothing.
- On failure `lastLoaded` is restored. An `ImportOriginalData` copy is freed on failure.

### Allocation cleanup ("free unused movesets")

`ImporterT7::CleanupUnusedMovesets`. Called after an import with `FreeUnusedMovesets`, before process detach
(`GameImport::PreProcessDetach`) and when the import thread stops.

- Stops without freeing anything if the game is not attached, `CanImport` is false, or any needed read fails.
- Never considers the most recent allocation.
- An allocation is kept if it contains: a MOTA pointer of either player's current moveset; either `MvlManager`'s
  `mvlHead`, `sequenceStart` or `sequenceEnd`; or any qword of either player structure (the whole 0x3670 bytes are
  scanned in 8-byte steps).
- Others are freed with `GameProcess::freeMem`: removed from `allocatedMemory` and released after
  `GAME_FREEING_DELAY_SEC` (10 s) by `FreeOldGameMemory`. Pinned allocations and buffers still used by a pending
  remote thread are not released.
- Detach forgets all allocation records of the old process.

## Live editing

Code: `GameSpecific/Editors/Editor_t7/Editor_t7_LiveEdition.cpp`, `Editor_t7_LiveEdition_Displayable.cpp`;
UI in `GameSpecific/EditorVisuals/EditorVisuals_t7/EditorVisuals_t7_Rendering.cpp`.

- The editor imports its moveset through its own import helper. The address of that import is the live target
  (`EditorLogic::live_loadedMoveset`), used while the "live edition" checkbox is on.
- `EditorT7::Live_OnFieldEdit` runs when a valid field is applied. It dispatches by window type (move, cancel,
  grouped cancel, voice clip, cancel extradata, extra / start / end property, requirement, hit condition, reactions,
  pushback, pushback extradata, input sequence, input, projectile, camera data, throw camera, move list displayable /
  playable / input) and writes that one field into the game copy, at `live_loadedMoveset + block offset + table offset
  + row * stride + field offset`. References are written as addresses. Move name edits are not written live.
- All writes of one edit go through one `WriteBatch`. On failure the binding is cleared and an error is shown (with a
  note when rollback was incomplete).
- The binding is cleared, so a new import is needed, when: the local moveset is reallocated (any list size change,
  `LoadMovesetPtr`), another game is selected, the player's moveset pointer no longer equals the imported address
  (`EditorVisuals_T7::MovesetStillLoaded`), or the game can no longer import.
- "Play move" (`EditorT7::SetCurrentMove`) writes the moveset to the player's moveset qwords 1..4 (not the first),
  resolves an alias through the game copy's `current_aliases`, then sets the timer, next move and current move id, in
  one batch.

## Training move list

- Extraction: `ExtractSettings_DisplayableMovelist` reads the player's `MvlManager::mvlHead` and stores the `MVLT`
  copy in the move list block. Without it, the block is 8 zero bytes.
- Disk extraction: from `mothead\movelist\<code>.mvl`.
- Import: `ImporterT7::ImportMovelist` (see step 7 above). Only when the training manager exists (training mode) and
  not in basic load.
- Editing: `Editor_t7_Forms_DisplayableMovelist.cpp`, `Editor_t7_Creations_DisplayableMovelist.cpp`; live edits of
  rows, playables and inputs in `Editor_t7_LiveEdition_Displayable.cpp`.

## Online and persistent play

Tool side: `GameInteractions/GameSharedMem.cpp`, `GameSpecific/Online/Online.cpp`, `Online_t7.cpp`; menus
`Layouts/Submenus/Submenu_OnlinePlay.cpp` and `Submenu_PersistentPlay.cpp`. Game side (the injected DLL):
`Injected/MovesetLoader.cpp`, `Injected/MovesetLoaders/MovesetLoader_t7*.cpp`, shared memory layout in
`Injected/SharedMemory/SharedMemory.h` and `SharedMemory_t7.h`.

### Injection

- The DLL `TKMovesetLoader.dll` is embedded in the program (LZMA). In release builds `ExtractMovesetLoaderIfNeeded`
  writes it to the working directory, replacing a file older than the executable or of another size. The tool also
  loads it in its own process to learn the export addresses.
- `Online::InjectDllAndWaitEnd` loads it in the game from the executable's directory (`GameProcess::InjectDll`), then
  calls the export `MovesetLoaderInit` in a remote thread (`CallMovesetLoaderFunction`). Result 1 = call
  `MovesetLoaderRun`; 2 = already running; 0 = failed.
- The DLL picks `MovesetLoaderT7` from the process name, finds its hook targets with the `str:t7_f_*` byte patterns
  inside the module, and hooks `ApplyNewMoveset`, `NetInterCS::MatchedAsClient`, `NetInterCS::MatchedAsHost` and
  `NetManager::GetSyncBattleStart`.
- Shared memory `Local\TKMovesets2T7Mem`, 4096 bytes. The DLL writes its version; the tool refuses to call into a DLL
  of another version (`versionMismatch`).

### Shared memory (`SharedMemT7`)

| Field | Meaning |
| --- | --- |
| `moveset_loader_version` | DLL version string |
| `locked_in` | custom movesets are applied only when true |
| `moveset_sync_status` | online sync state (`MovesetSyncStatus_`) |
| `moveset_loader_mode` | `OnlineMode` or `PersistentPlay` |
| `program_path` | the tool's directory |
| `players[2]` | per side: `crc32`, `custom_moveset_addr`, `custom_moveset_size`, `custom_moveset_original_data_addr` / `_size`, `missingMotas`, `is_initialized`, `moveset_character_id`, `previous_character_id` |
| `propToPlay` | player, id and value for the exported `ExecuteExtraprop` |

`players[0]` is the local user and `players[1]` the opponent, whatever side the user plays on.

### Applying a moveset (both modes)

The tool imports the selected file with `BasicLoadOnly` (online: `| ImportOriginalData`) and player address 0, then
`OnlineT7::OnMovesetImport` writes the address, size, crc32, character id and original-data address to the shared
memory and clears `is_initialized`.

The DLL's `ApplyNewMoveset` hook runs after every loading screen for each player. After the game's own function, when
locked in (and, online, when the sync is ready):

1. Map the player to a shared-memory slot (player list from `GetPlayerFromID`, corrected with `pid_addr`).
2. On first use, mark MOTA slots that are `MOVESET_ADDR_MISSING` in `missingMotas`; copy those slots from the
   game's new moveset.
3. Rewrite the character id checks with `StructsT7::RetargetCharacterIdChecks`, the same rule as the importer's
   `ApplyCharacterIDFixes`: condition-217 requirements and move start/end properties equal to the previous character
   id become the new player's character id (16 bits at `chara_id_offset`), any other becomes new id + 10.
4. Write the custom moveset to all five moveset qwords and MOTA slots 8 and 9 to the static camera offset.

Persistent play is this alone: the selected movesets stay applied across matches and loading screens.

### Online sync (Steam)

`MovesetLoader_t7_Networking.cpp`. Steam P2P through `ISteamNetworking` (`SendP2PPacket` / `ReadP2PPacket`), channel
`MOVESET_LOADER_P2P_CHANNEL` (48697987). The opponent is the other member of the lobby read at `steamLobbyId_addr`.

1. `MatchedAsClient` / `MatchedAsHost` start the sync in online mode (`InitMovesetSyncing`). Each side sends
   `RequestSync` with the size and crc32 of its original file (size 0 = no custom moveset).
2. The receiver answers `AnswerSync`. It requests a download unless it already holds a moveset with the same crc32 in
   `players[1]`. A size of `ONLINE_MOVESET_MAX_SIZE_BYTES` (30,000,000 bytes) or more is refused and the sync stops.
3. The sender sends its original file (header and compression included) in 1,000,000-byte packets.
4. The receiver installs it in-process (`MovesetLoaderT7::ImportForOnline`: header validation, decompression, T7,
   TTT2, TREV or T6 conversion and relocation) in `players[1]`, then sends `Ready`.
5. Both `Ready` -> `MovesetSyncStatus_Ready`. `GetSyncBattleStart` holds the battle start until then, for at most 60 s
   after the opponent's request was received, or 15 s if none came; on timeout the sync is abandoned and the match
   starts without custom movesets.

Packets are `PacketT7` with the magic `TKM2PACKET`; other packets are moveset bytes.

## Older games to T7 conversion

Code: `GameSpecific/MovesetConverters/` (`MovesetConverters.hpp`), called by `Importer_t7_OtherGames.cpp`.

| File `gameId` | Converter | Alias tables |
| --- | --- | --- |
| `GameId_TTT2` | `MovesetConverter::TTT2ToT7` (`Converter_ttt2_to_t7.cpp`) | `Converter_ttt2_to_t7_Aliases.cpp` |
| `GameId_TREV` | `TREVToT7`, subclass of `TTT2ToT7` with other aliases | `Converter_trev_to_t7_Aliases.cpp` |
| `GameId_T6` | `T6ToT7` (`Converter_t6_to_t7.cpp`) | `Converter_t6_to_t7_Aliases.cpp` |
| `GameId_T5`, `minorVersion == DR_ONLINE` | `T5DRToT7` (`Converter_t5dr_to_t7.cpp`) | `Converter_t5dr_to_t7_Aliases.cpp` |
| `GameId_T5`, other | `T5ToT7` (`Converter_t5_to_t7.cpp`) | `Converter_t5_to_t7_Aliases.cpp` |
| `GameId_T8` (retail only) | `T8ToT7::Convert`, then the normal T7 import | see `conversion.md` |

Each older converter:

1. Checks the source layout (`ValidateLegacyLayout` in `LegacyLayout.hpp`) and move name offsets.
2. Allocates a T7 payload: `MovesetInfo`, table and MOTA list, then the source name block, the T7 moveset block (in
   T7 table order), the source animation block and MOTA block. There is no move list block.
3. Copies `MovesetInfo` metadata and aliases, and record counts.
4. Converts each record field by field into the T7 layout. Indexes are kept; `0xFFFFFFFF` becomes
   `MOVESET_ADDR_MISSING`.
5. Translates requirement conditions and property ids with the game's alias dictionary (`ApplyBasicPropertyAlias`,
   `Aliases::ApplyPropertyAlias`, `Aliases::BuildAliasDictionary`).

The import then runs the same steps as a T7 file (validation, allocation, relocation, character fixes, MOTAs,
aliases, write, player pointer).

Per game:

- **TTT2 / TREV**: hitbox and `_0x98` values are converted (`ApplyHitboxAlias`, `ConvertMove0x98`). Cancels to the
  move named `Co_DA_Ground` are disabled (`command = 0xFFFFFFFFFFFFFFFF`). Projectiles are kept as zeroed records
  (the source format is not decoded). Fields with no source get 0.
- **T6**: moves have no names; the converter creates `move_<n>` and `anim_<n>` (shared animations share a name).
  The three secondary cancel lists are missing, the projectile table is empty, only the first 56 aliases are copied.
- **T5 / T5 DR**: 16-bit fields are widened. Cancel commands are rebuilt (`T5_T7_Aliases::ConvertCommand`): grouped
  cancel `0x8005` -> `0x800B`, its end `0x8006` -> `0x800C`, input sequences `0x8013 .. 0x81FF` + 6. The 36 aliases
  fill the first 36 T7 aliases; the rest take the first original alias. No projectiles, no secondary cancel lists.
- A MOTA pointer of `0xFFFFFFFF` becomes missing; missing MOTAs are taken from the player's current moveset at
  import, as for T7 files.

Online, the DLL accepts T7, TTT2, TREV and T6 files (`ImportForOnline_From*` in `MovesetLoader_t7_Importer.cpp`).
