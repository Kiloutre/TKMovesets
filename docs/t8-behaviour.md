# Retail Tekken 8 support: behaviour

This page describes what TKMovesets2 does with retail Tekken 8 (Steam build 26155785): how it finds players, what
an export captures, how it reads the game's own files, how an import is written into the game, how live editing and
move preview work, and what the retail editor allows. Data layouts are in [t8-structures.md](t8-structures.md).

The game profile is registered in `TKMovesets2/GameSpecific/Games.cpp` (first entry, `"Tekken 8"`): process
`Polaris-Win64-Shipping.exe`, data string `t8-retail`, extractor `ExtractorT8`, importer `ImporterT8`, editor
`EditorT8Retail` with `EditorVisualsT8Retail`, live editable, imports from T7 files, no online handler.

All game access is reading and writing process memory. No DLL is injected and no game function is called for
export, import, live edit or preview.

## game_addresses.ini (t8-retail)

`TKMovesets2/Resources/game_addresses.ini`, section `t8-retail`. Values with `val:` are plain numbers; the others are
pointer paths (`+` = relative to the main module).

| Key | Value | Use |
|---|---|---|
| `t8-retail_base` | 0x0 | required by the base-address loader |
| `t8-retail_registry_rva` | 0x959A320 | round registry pointer (player lookup) |
| `t8-retail_round_key_rva` | 0x959A088 | u32 key of the current round (player lookup) |
| `t8-retail_motbin_offset` | 0x3B50 | moveset root in the player struct |
| `t8-retail_chara_id_offset` | 0x168 | character id in the player struct (secondary identity at +4) |
| `t8-retail_anim_container` | +0x9BD9DE8, 0, 0x10, 0 | animation container (domain -> instance maps); export and import |
| `t8-retail_anim_manager` | +0x98D0C40, 0x8, 0 | animation manager (category index); export only |
| `t8-retail_currmove` | 0x3E0 | current move pointer |
| `t8-retail_currmove_timer` | 0x390 | current move frame |
| `t8-retail_nextmove_addr` | 0x28F0 | queued next move pointer |
| `t8-retail_nextmove_id` | 0x290C | queued next move id |

The same section holds byte patterns (`str:t8-retail_sig_*`) that Tools > Addresses searches for in the game's exe file
to update these values after a patch (`GameSpecific/AddressScanner/ExeSignatures.cpp`, `AddressUpdate.cpp`), and the
values of the separate Tag mode (`t8-retail_tag_*`), which is not covered here.

### PE timestamp

The section comment records the supported build: Steam 26155785, PE timestamp 0x6ADB1AD5. The code does not compare
the running game's PE timestamp before reading or writing. The PE timestamp is only read by the address scanner, which
reports it (`ScanReport::peTimestamp`, shown in Tools > Addresses), and written as metadata in the T7 conversion report
(`target_pe_timestamp` in `T7ToRetailT8.cpp`). Build compatibility of moveset data is checked through the source
header's compile date (+0x4) instead, see [Import](#import).

## Player lookup

`RetailT8Runtime::Players` (`Extractor_t8/RetailRuntime.hpp`), used by `ExtractorT8::GetCharacterAddress`,
`ImporterT8::GetCharacterAddress` and `RetailT8Preview::Read`:

1. Read the registry pointer at module + `registry_rva`, then the round map at registry +0x10.
2. Look up the u32 key at module + `round_key_rva` in the round map (head +0x100, buckets +0x110, mask +0x128; the key
   is not hashed). The node's +0x18 is the round object.
3. The round's first qword is the player list; its +0x08/+0x10 are the begin/end of entry pointers (at most 64).
4. For each entry, player = entry + 0x90 and slot = u32 at player + 4. Slots 0..3 are kept; a duplicate slot is an
   error.

Only slots 0 and 1 (P1, P2) are exposed for export, import and preview. A failed lookup returns 0 (no player). Then:

| Read | Offset |
|---|---|
| moveset root | player + `motbin_offset` (0x3B50) |
| context roots | player + 0x3B58, +0x3B60, +0x3B68, +0x3B70 |
| character id | player + `chara_id_offset` (0x168) |
| secondary identity | player + 0x16C |
| current move / timer | player + `currmove` / `currmove_timer` |
| next move pointer / id | player + `nextmove_addr` / `nextmove_id` |

All map walks are bounded and refuse invalid pointers, masks and cycles (`RetailT8Runtime::Lookup`).

## Animation lookup

`RetailT8Runtime` functions, used by export and import:

- `DomainMap(container, domain)`: looks up `domain` in the container map and returns vector[0] of its node (the
  instance map readers use). `DomainMapSlot` returns the address of that vector slot, which import guards.
- `ResolveAnimation` / `RetailT8Import::ReadyAnimation`: for an instance id, try domain 0x81 first, then the character
  domain. `ReadyAnimation` accepts an entry only when its AnimResource has a compiled object with the game's vtable,
  a control block, and a load request with status 2. This is the rule the game's resolver follows.
- `Panm(container, domain, instance, category)`: returns the PANM bytes the game holds, preferring the retained ANMBIN
  archive (`ArchivePanm`) and then AnimResource +0x1A0. `ArchivePanm` checks that pool and PANM lie inside the archive
  buffer.
- `HasCharacterAnimations`: true when the domain has a non-empty instance map.
- `AnimationCharacter(moves)`: the animation domain read from the move table's body handles.

## Extraction from memory

`ExtractorT8::Extract` (`Extractor_t8/Extractor_t8.cpp`) with `RetailT8Snapshot::CaptureComplete`
(`Extractor_t8/RetailSnapshot.cpp`).

### What is captured

1. The 0x318-byte source header at the player's moveset root. The header must have a compile date in
   20240101..20991231 and at least one move.
2. All 20 tables through their own pointer/count pairs, byte for byte (encrypted fields, aliases and unknown bytes
   included). Pointers stay as game addresses; the file records each table's original address.
3. The header is read again after the tables; a change fails the capture.
4. Native data (`CaptureNative`): for every move, the strings at +0x40 (name) and +0x48 (animation), and every external
   cancel list from +0x98, +0xA0, +0xB0, +0xC0 (non-null, gate > 0 for the last three, outside the cancel table). A
   string or list that cannot be read is recorded as not captured.
5. Animations in the selected categories (default body, hands, camera; set in the export settings), see below.

Not captured: the header strings (+0x10..+0x28), +0x170, the runtime slots (+0x2B0..+0x310) and the training-menu
movelist.

### Animations

- The animation domain is read from the move table's body handles, not from the player's character id, so a moveset
  that was imported onto another character still exports its own animation set. Mixed domains are refused.
- The character's slots come from the animation manager's category index (domain node, then its inner list). Each
  inner node gives a category (single bit in key bits 20..25) and a handle; identities are deduplicated by
  (category, instance). The node count must match the map's count.
- For each slot, the shared map (0x81) and then the character map are searched. A valid AnimResource +0x1A0 PANM is
  preferred (this keeps animations edited in memory); otherwise the PANM in the retained ANMBIN archive is used.
- A slot the game has not loaded is read from the game's `tkdata.bin` (see [Reading tkdata.bin](#reading-the-games-tkdatabin)),
  `com.anmbin` first, then the character's archive. Such entries get the marker address 0x7FFFFFFE0000
  (`ArchiveAssetAddress`). An id that the archives declare without data becomes a size-0 entry. An id declared by no
  archive stops the export; the error lists up to 20 of them.
- With the setting "read animations the game has not loaded from tkdata.bin" turned off
  (`ExtractSettings_T8_NO_ARCHIVE`, headless `no-archive`), unloaded slots become size-0 entries and the file gets a
  notes section saying how many per category.
- PANM size comes from its FlatBuffers metadata (`PanmSize`), reading a prefix of 128 bytes growing up to 1 MiB. Slot
  handles and the map head/count are re-read after copying; changes fail the capture.

### Budgets

| Item | Limit |
|---|---|
| All tables together | 256 MiB |
| One PANM | 32 MiB |
| Whole snapshot | 1 GiB |
| String | 1024 characters |
| External cancel list | 4096 entries |
| Animation slots per character | 100000 |

### File written

The payload is a `T8RS` snapshot (`RetailT8Snapshot`), always written as version 4:

| Part | Content |
|---|---|
| Header (40 bytes) | `"T8RS"`, version, source address, source header offset (40), directory offset (0x340), total size |
| Source header | the 0x318 captured bytes |
| Directory | 20 x {address, count, payload offset, stride, reserved} (32 bytes each) |
| Tables | 8-aligned, in table order |
| `T8AN` directory (v2+) | count, entries offset, data offset; entries {address, offset, size, instance, category} (32 bytes) |
| PANM data | 8-aligned blobs; size 0 with address 0 = no data (v3+) |
| `T8RX` extension (v4) | move strings and external cancel lists, CRC32-checked |

A tables-only selection still produces version 4, with an empty `T8AN` directory. Versions 1 to 3 are still read.

The outer TKMovesets container (`RetailT8Snapshot::Serialize`) has game id T8, minor version RETAIL,
`game_specific_flags` = 1 << 33 (1 << 32 only for v1 tables-only files), and tagged sections in the header area:

| Tag | Content |
|---|---|
| `T8NM` | names: hash -> string for every move and animation hash, from captured strings (when their hash matches), else the shipped `t8-retail` name table |
| `T8NT` | notes (UTF-8), written only when something is worth saying (for example animations not carried) |
| `T8ED` | editor provenance (written by the editor, see below) |

Unknown tags or bad checksums reject the file. The header's character id is the player's character id. The file is
named `T8_<character name>.tkmvst`, compressed with the selected codec, and written atomically. The moveset root and
the character id are re-read at the end; a change fails the export.

## Reading the game's tkdata.bin

`RetailT8GameData` (`Extractor_t8/RetailGameData.cpp`). The path is
`<exe dir>/../../../Content/Binary/pak/tkdata.bin` (`TkdataPath`), taken from the attached game's executable path.

Steps (`ReadTkdataFile`):

1. Check the 16-byte signature `__TEKKEN8FILES__`.
2. Read the last 128 bytes, XOR them with 16 u32 keys; they must start with `kaPniBNB`. Footer +0x11 = TOC seed,
   +0x18 = TOC offset (i32), +0x20 = TOC size (i32).
3. Read the TOC; if the seed is nonzero, decrypt it; decompress it with zstd. TOC +0x08 = entry count, +0x10 = first
   entry offset. Entries are 64 bytes.
4. Find the entry whose u64 at +16 equals the Kamui hash of the VFS path (`RetailNames::Hash`). Entry +8 = flag (u8),
   +9 = seed (u8), +24 = offset (from byte 16 of the file), +32 = stored size, +40 = original size.
5. Decrypt the stored bytes; if they start with the zstd magic, decompress (when flag is 0 and the original size is
   smaller, only that many bytes are the frame); otherwise truncate to the original size.

Decryption is AES-256-ECB through Windows CNG with a 32-byte key taken from a fixed 1800-character pool at offset
`(44983 * seed) % (1800 - 32)`. PKCS7 padding is removed when present.

VFS paths used: `mothead/bin/<code>.motbin`, `mothead/bin/<code>.anmbin`, `mothead/bin/com.anmbin`. Character codes
come from `TKMInterface/t8-retail_names.json` (`GameStaticData`); without it the character archive cannot be found.

`RetailT8GameData::Source::Panm(domain, category, instance)` looks in `com.anmbin` and then `<code>.anmbin`. It
returns the PANM, or nothing when the archives declare the id without data, and throws when no archive declares it.
An archive that holds two different PANMs under one identity is refused.

## Building movesets from the game's files

`RetailT8Disk` (`Extractor_t8/RetailDiskMoveset.cpp`). The UI entry is "Extract all T8 movesets from game files" in
the extraction menu (`Layouts/Submenus/T8DiskExtraction.cpp`); the headless command is
`build-disk-movesets-t8 TKDATA_BIN DIRECTORY [CODE,...|all] [CATEGORIES] [CODEC]`. Nothing is read from or written to
the game process.

`Build` loads `<code>.motbin` as an image at a synthetic address and applies `Relocate`, a port of the game's load step:

- header strings point at one empty placeholder string; +0x170 and the runtime slots are cleared; table offsets
  become absolute pointers (`root + 0x318 + offset`); +0x2 is set to 1;
- for character 0x74 only, the identity fields +0x160/+0x164 are rewritten;
- record indices become pointers (`base + index * stride`); -1 becomes null for move voiceclip and property lists;
  move cancel banks +0xA0/+0xB0/+0xC0 become null when their gate is below 1;
- move +0x50: the disk index (masked 0xFFFF3FFF) is looked up in the character archive's list 0 and replaced by the
  handle `0xEF << 56 | domain << 40 | id`; move +0x120 is set from the PANM's frame count (0xFFFFFFFF when no data);
- cancel and group cancel option bits (+0x26) are computed (see [t8-structures.md](t8-structures.md#cancels-table-6-and-group-cancels-table-7-0x28));
- projectile +0xAC and +0xB0 get their defaults;
- the six encrypted move fields are decoded from the disk form and re-encoded in the memory form.

The image is then passed to the same capture code as a memory export through an in-memory process stand-in
(`ImageProcess`), with every identity the character archive declares in the selected categories as animations (bytes
from the character archive, else `com.anmbin`; size 0 when neither has data). For these lookups `Build` and `Relocate`
try the character archive before `com.anmbin`, which is the opposite order from `Source::Panm`.

`WriteMovesetFile` writes `T8_<character name>.tkmvst` (the same name as a memory export; an existing file is
skipped by the UI and refused by the function), with a names section and no notes section. The header character id is
the character's runtime id when known, else its asset id; the animation domain is the asset id. Characters without a
known id are refused.

## Import

`ImporterT8::ImportMovesetData` (`Importer_t8/Importer_t8.cpp`), wrapped by `Importer::ImportChecked`
(`GameSpecific/Importers/Importer.cpp`).

### Inputs and checks

- Accepted files: retail T8 (`minorVersion` RETAIL) and T7 files. A T7 file is first converted with
  `T7ToRetailT8::ConvertStandalone` (self-contained: no other moveset, no game data; the same output for every
  target). Other T8 builds are refused (`UnsupportedGameVersion`).
- `ImportSettings_BasicLoadOnly` and `ImportSettings_ImportOriginalData` are refused (no online or basic loading).
- The target player must still be P1 or P2.
- No identity check: any valid moveset is imported onto any target. The target's two identity fields (+0x168 and
  +0x16C) may differ; each kind of self-character check is rewritten to its own field (see below).
- The payload is validated (`RetailT8Snapshot::Validate`), then the editor provenance is read when present
  (`RetailEditorData::Read`).

### Two routes

**Memory-only route.** Used when all of these hold:

- the file is a converted T7 file, or a v4 native snapshot (it has the `T8RX` extension);
- the file's idle row (header +0xAA) is inside its move table, and (for converted files) it embeds body PANMs;
- for native snapshots, the file's header compile date equals the target's (same game build);
- the target's animation map for its domain exists and is populated (map +0x48 == 1);
- for native snapshots, every external cancel list the move table points at was captured in the file.

On this route the target's own moveset lends header strings and runtime slots, and its idle row lends move strings.
Body animations are imported from the file (see below). No loaded source moveset is needed.

**Binding route.** Otherwise, a loaded moveset must provide what the file does not carry. Candidates are the target's
current root, the other player's root, and (for native files) the original capture address. A candidate must have the
same compile date (native files) and the export's original table counts (from editor metadata when present). Among
compatible candidates, `RetailT8Import::BindAnimations` scores each by missing animations and identity mismatches and
the best one wins; mismatches are counted, not fatal. Each move row is bound to a loaded row that plays the same
animation hash (same index first), else to the loaded idle row. The bound moveset's animation character must be
loaded (`HasCharacterAnimations`).

**Target fallback.** When neither route above applies (no compatible loaded moveset, or its character's animations are
not loaded; this includes converted T7 files whose target animation map is not ready), the target's own moveset is the
binding (`ImporterT8::TargetFallback`). It is loaded by definition. Each move plays the target's animation of the same
hash when it has one, else the target's idle (`BindingFallbackMoves`; the GUI shows the idle warning). External cancel
lists come from the file's capture when it has one, else become an empty list: one terminator copied from the file's
own cancel table (`RetailT8Import::TerminatorCancel`, counted in `FallbackCancelLists`). Nothing is written into game
resources on this route. The image audit checks these lists against `ImporterT8::FallbackCancels`.

A valid moveset is always imported. The import fails only when the file is invalid or the game cannot be written:

| Situation | Result |
|---|---|
| invalid table pointer graph | refused before any write |
| the target player has no readable moveset | refused before any write |
| round, player root or character changed during import | refused (stale target) |

### Building the image

`RetailT8Import::Prepare` (`Importer_t8/RetailImport.cpp`) builds the allocation offline:

1. Copy the source header and the 20 tables, each 8-aligned, into one buffer.
2. Clear and record runtime dependencies: header strings, +0x170, runtime slots, move strings +0x40/+0x48, body
   handles +0x50.
3. Convert every field of the typed pointer graph into a relocation to the new table row. Inactive cancel banks
   (gate <= 0) are cleared. Null pointers stay null. A pointer outside its target table is an issue, except move
   cancel pointers, which are treated as external lists.
4. External cancel lists are appended to the buffer and their requirement/extra pointers relocated. They come from
   the file's `T8RX` capture (memory-only native) or from the bound live moveset. With editor metadata, ids are
   remapped through the saved remaps; a cancel that refers to something the edit deleted is dropped (the list
   terminator is kept, with the dependency cleared).
5. Encrypted values, aliases, commands, handles and parameters are kept as bytes.

Then, on the plan buffer:

- requirement identity adaptation (`AdaptCharacterRequirements`): in conditions 220/221/224/225/499/500/503/504 the
  file's identity (or the import profile's fixed identity) and the target's identity are swapped, using +0x168 for the
  primary checks (220/221/499/500) and +0x16C for the secondary checks (224/225/503/504); primary legacy
  selector codes 0x81..0x8F are resolved first; an identity a field cannot hold is left as is;
- import profile rules (`ApplyProfileConditions`, `ApplyProfileProperties`): gender/story requirements forced to
  always or never true; stripped extra properties moved to a frame no move reaches;
- converted T7 files: the header compile date is copied from the target, and `ApplyConvertedIdentity` writes the
  target's identity codes into header +0x160/+0x164 and every move's encrypted +0xD0 (with its plain-copy slot);
- runtime dependencies are filled from the bound or target moveset (header fields from its header, move strings from
  the bound rows; native move names from their original row);
- custom move names from editor metadata, and captured move strings (memory-only native), are stored in the
  allocation and pointed at.

### Animations

Memory-only route (body animations, plus the hand, face, wing and camera animations of converted T7 files):

- Every move's body animation comes from the file (converted animations first, then embedded natives) or, when the
  file lacks it, from the game's `tkdata.bin` (`Source::Panm`).
- For native snapshots, an animation that is byte-identical to the copy the game would resolve in the target domain
  (`ReadyAnimation`) is bound to that copy; nothing is uploaded.
- Every other distinct PANM gets an id from `ChooseAnimationId`. If that id already resolves to an entry the tool
  built earlier with the same bytes, it is reused; otherwise `BuildRegisteredAnimation` builds a complete entry
  locally (PANM copy, compiled object, control block that never reaches zero, finished request, AnimResource with the
  game's vtable, ResourceRef, map node). All new entries go in one allocation.
- `PublishAnimations` links the nodes into the target's instance map through bucket `hi`/`prev` links only. The game's
  readers find them; its list walks (teardown, rehash) never reach them, so the game never frees them. One 8-byte
  store per bucket publishes.
- Move handles are set to `0xEF << 56 | targetDomain << 40 | id`. An id the archives declare without data keeps its
  id. An animation found in neither the file nor the archives makes its move play the target's idle animation
  (counted in `MemoryOnlyFallbackMoves`; the first archive error is kept in `ArchiveError`).
- Converted T7 hand, face, wing and camera animations (`T8AN` categories 1..4) get an entry the same way, then a node in the
  target domain's category index: the anim manager's map `+0x2088` (domain -> inner map at node `+0x18`), key
  `index | 1 << (20 + category)`,
  value the entry's handle ([retail-t8-nonbody-anim-re.md](../.docs/retail-t8-nonbody-anim-re.md)). The file's index
  (`0x2000` up) is used when the key is free or already holds this same handle (an earlier import); a key holding
  another animation is never shadowed, the next free index is taken instead. `PublishCategoryIndex` links the nodes
  bucket-only, after the entries, like `PublishAnimations`.
- `ApplyHandIndices` then rewrites the hand, face, wing and camera rows (`0x8610` / `0x8611`, `0x877A`, `0x8683`, and the camera ids of
  `T7CameraToRetailT8::CameraProperties`, with a value in `0x2000..0x3FFF`) to the published index of their category. A row without one (another route, no category index for the domain, no free key) gets the
  never-reached frame `ImportProfile::StrippedFrame`, so it never plays a missing or foreign animation. The image audit
  applies the same rewrite. Report: `hand_animations.carried`, `published_indices`, `mapped`, `silenced_rows`.

Binding route:

- Moves play the bound moveset's loaded animations. A file PANM whose bytes differ from the game's copy is counted
  (`EditedAnimationsKept`); the game's copy plays.
- Converted animations with the custom marker address (older converted files) are attached to a loaded carrier
  resource when the process allows it: the compiled object is built locally, uploaded, and AnimResource +0x190 is
  repointed (`AttachCustomAnimation`). The original object is recorded per game process in
  `%TEMP%/TKMovesets2/t8-carriers-<pid>-<start>.bin` and restored for that player on a later import
  (`RestoreCarriers`). Moves whose carrier is missing play the idle animation (`FallBackMissingCarriers`).

Non-body categories are never uploaded or registered by import.

### Publishing

- `Importer::ImportChecked` wraps the import in a `GameProcess::WriteBatch`: every write is staged and guarded with
  expected bytes (round registry, round key, round node, player root and identity, bound root and header, move
  records, map slot, map buckets, external lists, current move and contexts).
- One allocation receives the relocated buffer (`Plan::RelocateTables`).
- The player's moveset root (+0x3B50) is set to the allocation.
- With `ImportSettings_ApplyInstantly` (the default), the four context roots are also set, the next move id is set to
  0x8001 and the next move pointer to the idle row, and the timer is set to the current move's length (+0x120), so the
  game transitions to the new idle on its own.
- With `ImportSettings_StageOnly`, everything is written except the player pointers.
- Commit runs with the game process suspended (`GameProcess::PauseGuard`). A failed commit rolls back; an incomplete
  rollback is reported as `GameRollbackError`.
- Allocations are never freed (`CleanupUnusedMovesets` does nothing), because game caches can keep references.
  Repeated imports use more memory.

### Shadow check

`RetailImportShadow` (`Importer_t8/RetailImportShadow.hpp`) is a process stand-in that reads live memory but keeps
every write and allocation in local buffers, and refuses resource preparation. The headless `check-import-t8 FILE SLOT`
runs the production importer against it with a process handle that has no write rights, then
`RetailT8Import::AuditImage` checks the produced image independently: header, every table, external cancel lists,
strings, identity and profile adaptations, and (memory-only) that every body handle resolves to a ready resource,
except ids declared without data. The game is not written or paused.

## Live editing and play move

### RetailLiveEdit

`RetailLiveEdit::Apply` (`Importer_t8/RetailLiveEdit.cpp`) is used by the editor's Import button and by every
live-editing revision. With a binding (the root and document bytes published earlier on that slot), it calls `Sync`;
otherwise, or when `Sync` reports a structural change, it imports the document again with `ApplyInstantly`.

`Sync` writes changed records in place. It reports a structural change (full import needed) when any of these differ
between the published and the current document: character id, conversion origin, table counts/strides/addresses,
source header, editor metadata (names, provenance, remaps), animation assets. It also falls back when the published
allocation is no longer the player's root or its table counts differ.

For each changed record it:

- relocates pointer fields to the published table bases; keeps an external list pointer when the document reference is
  unchanged; refuses an input sequence without inputs or past the input table;
- keeps the published move name pointer (+0x40); keeps +0x48/+0x50 when the animation hash is unchanged, else copies
  them from a published row that plays the new animation (no such row: structural);
- applies requirement identity adaptation and profile rules (tables 1 and 9), and converted-T7 identity codes (moves);
- skips records whose final bytes equal the published ones.

All writes go in one `WriteBatch` guarded by the player's root, committed with the process paused.

### Play move (preview)

`RetailT8Preview` (`Importer_t8/RetailPreview.cpp`):

- `Read` returns the player, root, move table, count, character and the current move row (when the current move
  pointer is inside the table).
- `Queue` requires the player's root to be the one published by the editor. It resolves aliases 0x8000..0x803B through
  header +0xA8, guards the round and player state, sets the four context roots to the root, writes the requested id to
  the next move id and the row to the next move pointer, sets the timer to the current move's length, and commits with
  the process paused.

## Retail editor

### Editor_t8Retail

`EditorT8Retail` (`Editors/Editor_t8Retail/`) edits `T8RS` files offline. It never uses CNT structures and never
writes game memory (`Live_OnFieldEdit` only clears the live binding).

- Loading requires a retail container with a valid CRC and matching flags (`LoadMovesetPtr`, `LoadMoveset`).
- Fields come from a per-table schema (`BuildSchema`) based on `Structs_t8.h`. Kinds: u16, s16, u32, s32, u64, f32,
  reference (shown as a row index, -1 = null) and encoded u32.
- Encrypted move fields are shown decoded. `name_hash` and `animation_hash` are read-only; vulnerability, hit level,
  character id and move key can be edited, are re-encoded with the existing key, and their plain-copy slot is updated.
- External references (pointers outside their table) and non-finite float bit patterns are read-only.
- `SaveRecord` validates all fields first, then writes. It keeps existing list terminators, checks input sequence
  bounds, checks move ids against the move count and aliases (0x8000..0x803B, 0xFFFF), checks group cancel ids, and
  refuses to activate an optional cancel bank whose pointer does not resolve.
- `RenameMove` stores the Kamui hash in +0x00 (re-encoded, plain copy updated) and the string in the editor metadata.
- `SetAnimation` copies the donor's animation key, animation name pointer and handle (+0x48..+0x57) and length (+0x120).
- `ImportPanm` binds a PANM identical to an embedded one; otherwise stores it as a custom asset under a new instance
  (from 0x10000) with its own animation name, or replaces in place a custom animation used only by this move.
  T7 animation files are converted onto the retail skeleton first. Only body animations are handled.

### Table rebuilder

Create, duplicate, delete and list resize/reorder go through `EditorT8Retail::Rebuild`
(`RetailEditorOperations.cpp`):

- The changed table is rebuilt and every table gets a file-local address `0x600000000000 + (table << 32)`.
- All typed pointers are re-pointed; ranges (input sequence inputs, pushback extras) are remapped with their counts;
  move ids in transitions, cancels, reactions and header aliases, input sequence commands, group cancel ids and the
  projectile/throw-camera property parameters are remapped.
- Animation assets and the `T8RX` extension are carried unchanged.
- Editor provenance (`RetailEditorData`, outer section `T8ED`, CBOR): source address, original source header, original
  move name hashes, per-table row origins and remaps, custom names. Import uses it to map edited rows back to captured
  rows. On save, the names section is rebuilt for every hash in use (custom names, then the file's names, then the
  shipped table); notes are kept.
- A move table must stay below 0x8000 rows.
- `CreateNew` for a move copies the idle move (alias 0x8001), gives it its own one-row cancel list (0x8000 to 0x8001),
  clears the extra cancel banks, voiceclip and property lists, sets transition 0x8001 and names it `tkm_<id>`. New
  lists of other tables are created with their terminator. `DuplicateStructure` rewrites the check slots of copied
  moves for their new rows.

### Protected stock moves

`CanDeleteRecord`: in the move table, a row can be deleted only when no original move maps to that row or a later one.
In practice only moves appended after all stock moves can be deleted, so stock move ids stay stable. Rows of other
tables can be deleted.

### EditorVisuals_t8Retail

`EditorVisualsT8Retail` (`EditorVisuals/EditorVisuals_t8Retail/`) is the window. Game-related controls
(`EditorVisuals_t8Retail_Browser.cpp`):

- A process-list scan every 2 s shows whether the game runs; no handle is opened for it.
- Current (1P/2P) reads the player's current move every 250 ms and selects it, only when the loaded move table has
  the same move name hashes as the document (`MatchesMoveNames`).
- Import (button, or F1/F2/F3 for P1/P2/both) builds a save snapshot and calls `RetailLiveEdit::Apply` on a worker
  thread. A successful import binds the document revision to that slot.
- Live editing (enabled after an import on the selected slot) publishes each new applied revision automatically; it
  stops when the process or the player's root changes.
- Play queues the selected move with `RetailT8Preview::Queue`.

## Not supported

- Online play and persistent loading: the retail profile has no online handler and no supported online imports.
- Basic loading and "import original data" settings.
- The training-menu movelist: not captured, not imported, not shown in the editor.
- Importing or registering non-body animations (hands, face, swing, camera, extra). They are exported and kept in
  files, but import binds only body animations.
- More than two players (slots 2 and 3 are read but not exposed).
- Other game builds: no automatic check; addresses must be updated with Tools > Addresses, and native files from
  another build fail the compile-date checks.
