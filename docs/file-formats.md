# Moveset file format (.tkmvst)

This page describes the on-disk layout of a TKMovesets2 moveset file: the TKM2 outer container, the compression
codecs, the Tekken 7 payload and the retail Tekken 8 `T8RS` payload.

All integers are little-endian. Offsets are in bytes. "Aligned" means rounded up to a multiple of 8.

## File overview

A `.tkmvst` file is laid out in this order:

| Part | Starts at | Size | Notes |
| --- | --- | --- | --- |
| `TKMovesetHeader` | 0 | 256 bytes (current) | Older files can have a shorter header (see `header_size`) |
| Header extension sections | 256 | up to `header_size` | Retail T8 only. Tagged sections (`T8ED`, `T8NT`, `T8NM`) |
| Custom property list | aligned `header_size` | up to `block_list` | `TKMovesetProperty` entries ending with id 0. Holds the [import profile](import-profile.md) |
| Block list | `block_list` | `block_list_size` x 8 | u64 offsets into the payload |
| Payload (moveset data) | `moveset_data_start` | rest of file | Raw, or compressed with the codec named in the header |

Only the payload is compressed. The header, its extension sections, the property list and the block list are always
stored raw, so they can be read without decompressing.

Sources: `TKMovesets2/LocalStorage/MovesetStructs.h` (`TKMovesetHeader`),
`TKMovesets2/GameSpecific/Extractors/Extractor_t7/T7ExportSnapshot.cpp` (`T7Export::Serialize`),
`TKMovesets2/GameSpecific/Extractors/Extractor_t8/RetailSnapshot.cpp` (`RetailT8Snapshot::Serialize`).

## TKM2 header (`TKMovesetHeader`)

Defined in `TKMovesets2/LocalStorage/MovesetStructs.h`. Offsets assume the MSVC x64 default packing used by the
build; the struct is copied to and from disk as raw bytes. `sizeof(TKMovesetHeader)` is 256.

| Offset | Size | Type | Field | Meaning |
| --- | --- | --- | --- | --- |
| 0x00 | 4 | char[4] | `_signature` | Always `TKM2` |
| 0x04 | 28 | char[28] | `version_string` | Tool version that wrote the file, NUL-terminated (currently `"1.0"`, `MOVESET_VERSION_STRING` in `Utils/constants.h`). Debug builds append `d` |
| 0x20 | 32 | u8[32] | `gameVersionHash` | Meant to identify the game build. All current writers store zeros |
| 0x40 | 2 | u16 | `gameId` | Game the file imports into (`GameId_` in `GameSpecific/GameIDs.hpp`) |
| 0x42 | 2 | u16 | `minorVersion` | Game variant (`GameVersions::<game>` in `GameIDs.hpp`) |
| 0x44 | 4 | u32 | `header_size` | Size of the header, including any extension sections. Aligned to 8 by writers |
| 0x48 | 4 | u32 | `block_list` | Absolute offset of the block list |
| 0x4C | 4 | u32 | `block_list_size` | Number of u64 entries in the block list |
| 0x50 | 4 | u32 | `moveset_data_start` | Absolute offset of the payload |
| 0x54 | 4 | - | padding | |
| 0x58 | 8 | u64 | `moveset_data_size` | Uncompressed payload size when compressed; 0 when not compressed |
| 0x60 | 1 | u8 | `compressionType` | Codec of the payload (see [Compression](#compression)) |
| 0x61 | 3 | - | padding | |
| 0x64 | 4 | u32 | `crc32` | Checksum of the moveset data (see [Checksums](#checksums)) |
| 0x68 | 4 | u32 | `orig_crc32` | `crc32` as it was at extraction time. Kept unchanged by editors |
| 0x6C | 4 | - | padding | |
| 0x70 | 8 | u64 | `date` | Last modification time, Unix seconds |
| 0x78 | 8 | u64 | `extraction_date` | Time of the first extraction, Unix seconds |
| 0x80 | 4 | u32 | `characterId` | Character id in the source game's numbering |
| 0x84 | 4 | - | padding | |
| 0x88 | 8 | u64 | `flags` | `MovesetFlags_` bits (below) |
| 0x90 | 32 | char[32] | `origin` | Display name of the game the file imports into (e.g. `"Tekken 7"`, `"Tekken 8"`) |
| 0xB0 | 32 | char[32] | `target_character` | Character name shown in the moveset lists |
| 0xD0 | 32 | char[32] | `orig_character_name` | Original character name |
| 0xF0 | 8 | u64 | `game_specific_flags` | Game-specific bits. Retail T8 uses it for the container profile (below); T7 writers store 0 |
| 0xF8 | 2 | u16 | `conversion_origin` | Source game of a converted file, 0 if not converted |
| 0xFA | 6 | - | padding | Zeroed by the T8 to T7 converter so output is deterministic |

Dates are written with `Helpers::getCurrentTimestamp()` (seconds since 1970, UTC) and shown in local time by
`Helpers::formatDateTime`.

### `flags` bits

| Bit | Name | Set by |
| --- | --- | --- |
| 0 | `MovesetFlags_MovesetModified` | The T7 and T8 CNT editors on the first edit. The list shows modified files in another color (`LocalStorage.cpp`, `getMovesetColor`) |
| 1 | `MovesetFlags_Debug` | Extractors in debug builds |

Other bits are unused. Retail T8 export writes 0.

### `gameId` and `minorVersion`

From `GameSpecific/GameIDs.hpp`. Values are serialized and must not change.

| `gameId` | Game | `minorVersion` values |
| --- | --- | --- |
| 0 | Tekken 7 (`GameId_T7`) | 0 (`NONE`) |
| 1 | Tekken 8 (`GameId_T8`) | 0 closed network test, 1 closed beta test, 2 retail |
| 2 | Tekken Tag Tournament 2 | 0 RPCS3 BLES01702 v01.00, 1 RPCS3 BLES01702 v01.03 |
| 3 | Tekken Revolution | (none defined) |
| 4 | Tekken 6 | 0 RPCS3 BLES00635, 1 PPSSPP ULUS10466, 2 RPCS3 BLUS30369 |
| 5 | Tekken 5 | 0 default, 1 Dark Resurrection online |

### `conversion_origin`

The meaning depends on the file's `gameId`.

| File `gameId` | Value | Meaning |
| --- | --- | --- |
| T7 | 0 | Not converted |
| T7 | 1 | From TTT2 |
| T7 | 2 | From Tekken Revolution |
| T7 | 3 | From Tekken 6 |
| T7 | 4 | From Tekken 5 |
| T7 | 5 | From Tekken 5 DR |
| T7 | 6 | From retail Tekken 8 (`T8ToT7`) |
| T8 | 1 | From Tekken 7 (`GameVersions::T8::FROM_T7`) |

Converted files keep their source's `characterId` meaning: T8 to T7 maps it to the T7 id; T7 to T8 writes the
placeholder 124 (`T7ToRetailT8::ConvertedCharacter`), which the importer replaces with the target's id.

### Older headers and version-gated fields

`header_size` tells how much of the struct the file actually has. Bytes past `header_size` belong to the property list
or block list, not to the header. Readers use these getters (`MovesetStructs.h`):

| Getter | Returns the field when | Otherwise |
| --- | --- | --- |
| `GetOrigCharacterName()` | `version_string` > `"0.5"` and `header_size` >= 0xF0 | `target_character` |
| `GetGameSpecificFlags()` | `version_string` > `"0.5"` and `header_size` >= 0xF8 | 0 |
| `GetConversionOrigin()` | `version_string` > `"0.6"` and `header_size` >= 0xFA | 0 |

The smallest accepted `header_size` is 0xD0 (208, up to `target_character`). Converters always write a full current
header (`header_size` = 256) and fill the gated fields through the getters.

### Header validation (`TKMovesetHeader::ValidateHeader`)

A file is rejected when any of these fails:

- signature is `TKM2`;
- `version_string`, `origin` and `target_character` contain a NUL; `orig_character_name` too when
  `header_size` >= 0xF0;
- `header_size` >= 0xD0;
- `block_list_size` != 0;
- `block_list` >= `header_size`;
- `moveset_data_start` > `block_list` and < file size;
- the block list fits between `block_list` and `moveset_data_start`;
- `compressionType` <= 3;
- when compressed: `moveset_data_size` is not 0 and <= `INT32_MAX`.

The list scanner (`LocalStorage.cpp`, `fetchMovesetInformations`) also rejects empty or unterminated
`version_string`, `origin` and `target_character` (`Helpers::isHeaderStringMalformated`).

## Header extension sections (retail T8)

Retail T8 files may have tagged sections between the end of the struct (offset 256) and `header_size`.
Source: `RetailT8Snapshot::ReadOuterSections` and `Serialize` in `Extractor_t8/RetailSnapshot.cpp`.

Each section is a 16-byte header followed by its data; the next section starts at the next aligned offset.

| Offset | Size | Type | Field |
| --- | --- | --- | --- |
| 0 | 4 | char[4] | tag |
| 4 | 4 | u32 | version |
| 8 | 4 | u32 | data size |
| 12 | 4 | u32 | CRC-32 of the data |
| 16 | size | bytes | data |

| Tag | Version | Content | Code |
| --- | --- | --- | --- |
| `T8ED` | 1 | Editor provenance: CBOR object with `source_address`, `source` (the 0x318-byte source header), `move_names`, `origins`, `remaps`, `names` | `RetailEditorData::Read` / `Serialize` (`Editors/Editor_t8Retail/RetailEditorData.cpp`) |
| `T8NT` | 1 | Notes: UTF-8 text about the file (e.g. animations it does not carry). Written only when there is something to say. Max 1 MiB | `NotesSection`, `Notes` |
| `T8NM` | 1 | Names: `u32 count`, then `count` x `{u32 hash, u32 length, length bytes}` sorted by hash. Move and animation names for the Kamui hashes stored in move records. Max 1M names, 4096 bytes per name | `NamesSection`, `Names`, `CollectNames` |

Rules: an unknown tag, a size past `header_size` or a bad CRC rejects the file. All sections together are limited to
64 MiB. `T8ED` may appear at most once.

## Custom property list

Starts at aligned `header_size` and runs to `block_list`. Each entry is 8 bytes:

| Offset | Size | Type | Field |
| --- | --- | --- | --- |
| 0 | 4 | i32 | `id` |
| 4 | 4 | i32 | `value` |

An entry with `id` 0 (`TKMovesetProperty_END`) ends the list. The list area is padded to 8 bytes. Readers stop at
4096 entries. Current writers store the import profile here; older files hold only the END entry. See
[import-profile.md](import-profile.md).

## Block list and payload location

The block list is `block_list_size` u64 values at `block_list`. What they mean depends on the game:

| Format | `block_list_size` | Block list content |
| --- | --- | --- |
| Tekken 7 | 8 | Offsets of the 8 T7 blocks, relative to `moveset_data_start` |
| Retail Tekken 8 | 1 | A single 0. The payload is a self-describing `T8RS` container |
| T5, T5DR, T6, TTT2, Revolution, T8 CNT/CBT | game-specific | See `TKMovesetHeaderBlocks` in the matching `GameSpecific/Structures/Structs_*.h`. These files are read only for conversion to T7 |

The payload always starts at `moveset_data_start`. The first payload byte is the first byte of block 0 (T7) or of
the `T8RS` header (retail T8).

## Compression

`compressionType` selects the codec (`TKMovesetCompressionType_` in `MovesetStructs.h`). The codec is detected from
this byte only; LZ4 and LZMA data have no extra magic check. Code: `TKMovesets2/Utils/Compression.cpp`
(`CompressionUtils::RAW::Moveset::Compress`, `Decompress`, `DecompressToBuffer`, `ExpandFileBuffer`) and
`TKMovesets2/Utils/CompactZstd.cpp`.

| Value | Codec | Stored payload | Notes |
| --- | --- | --- | --- |
| 0 | None | Raw payload | `moveset_data_size` = 0 |
| 1 | LZ4 | One raw LZ4 block (`LZ4_compress_default`), no LZ4 frame header | Input and output limited to `INT_MAX` |
| 2 | LZMA | One `.xz` stream (`lzma_easy_buffer_encode`, preset 0, CRC64 check) | Decoder accepts concatenated streams, 256 MiB memory limit, must consume all input and produce exactly `moveset_data_size` bytes |
| 3 | Compact + Zstd | A `TCZ1` frame (below) | Lossless. Older tool versions reject it |

When compressed:

- `moveset_data_size` is the uncompressed payload size;
- the compressed bytes run from `moveset_data_start` to the end of the file;
- everything before `moveset_data_start` is identical to the uncompressed file.

On load, `ExpandFileBuffer` rebuilds the uncompressed file and sets `compressionType` = 0 and
`moveset_data_size` = 0 in memory. Saving reverses this. A file is never compressed twice
(`Compress` refuses an already compressed input).

Default codecs: LZ4 in release builds and none in debug builds (`CompressionUtils::GetDefaultCompression`).
Retail T8 extraction and the retail editor default to Compact + Zstd (`Submenu_Extract.cpp`,
`EditorVisuals_t8Retail.cpp`). Headless codec names: `none`, `lz4`, `lzma`, `compact-zstd`.

### Compact + Zstd: TCZ1 frame

`CompactZstd::Compress` / `CompactZstd::Decompress`.

| Offset | Size | Type | Meaning |
| --- | --- | --- | --- |
| 0 | 4 | char[4] | `TCZ1` |
| 4 | 4 | u32 | Header length, always 40 |
| 8 | 8 | u64 | Original payload length. Must equal `moveset_data_size` |
| 16 | 8 | u64 | Length of the expanded `T8AS` archive |
| 24 | 8 | u64 | Length of the Zstd frame. Must equal the stored size minus 40 |
| 32 | 4 | u32 | CRC-32 (zlib) of the original payload |
| 36 | 4 | u32 | CRC-32 (zlib) of bytes 0..35 |
| 40 | variable | | Exactly one Zstd frame |

Zstd frame rules: content checksum on, no dictionary, known content size equal to the archive length, window at most
32 MiB (the encoder uses `windowLog` 23), no trailing bytes or extra frames. The fast profile (used by
`Compress(payload)`) uses Zstd level 3.

### Compact + Zstd: T8AS archive

The Zstd frame expands to:

| Offset | Size | Type | Meaning |
| --- | --- | --- | --- |
| 0 | 4 | char[4] | `T8AS` |
| 4 | 4 | u32 | Version, 1 |
| 8 | 8 | u64 | Original payload length |
| 16 | | | Records until the original length is covered |

Each record:

| Offset | Size | Type | Meaning |
| --- | --- | --- | --- |
| 0 | 1 | u8 | Mode: 0 opaque bytes, 1 compact PANM (`PCF1`) |
| 1 | 4 | u32 | Original length (never 0) |
| 5 | 4 | u32 | Stored length |
| 9 | stored | | Data |

Mode 0 requires stored = original. Mode 1 holds one animation encoded by `CompactPanm::Encode`
(`Utils/CompactPanm.cpp`); it starts with a 24-byte `PCF1` header and may be at most
`24 + 32 * 16384` bytes larger than the original PANM. Record lengths must sum to the original payload length and
every archive byte must be consumed. The final payload must match the CRC at TCZ1 offset 32.

The encoder walks a `T8RS` v2..v4 payload by its directories (`AnimationRanges` in `CompactZstd.cpp`) and encodes each
PANM blob as mode 1 when it can. Everything else (tables, directories, padding, the `T8RX` extension, any non-T8RS
payload, PANMs it cannot encode) is stored as mode 0. The `PCF1` encoding itself is described in
`.docs/compact-zstd-format.md` and implemented in `Utils/CompactPanm.cpp`.

## Size limits

Source: `TKMovesets2/Utils/MovesetLimits.hpp`, checked in `Helpers::ReadMovesetFile` and `ExpandFileBuffer` before
any large allocation.

| Profile | Applies to | Expanded file limit | Stored file limit |
| --- | --- | --- | --- |
| Legacy | Every file except the one below | 256 MiB | 256 MiB |
| Retail runtime | `gameId` T8, `minorVersion` retail, `header_size` >= 0xF8, `game_specific_flags` = `1 << 33`, `block_list_size` = 1 | 1 GiB | 1 GiB + 1 GiB / 255 + 64 KiB |

Other limits:

| Item | Limit | Code |
| --- | --- | --- |
| T7 export payload | 256 MiB | `T7ExportSnapshot.cpp` (`Limit`) |
| `T8RS` payload | 1 GiB | `RetailSnapshot.cpp` (`Limit`) |
| `T8RS` header + directory + 20 tables | 256 MiB | `RetailSnapshot.cpp` (`TableLimit`, `CheckSource`) |
| One PANM | 32 MiB | `RetailSnapshot.cpp` (`PanmLimit`), `CompactPanm::MaximumSize` |
| Animations per `T8AN` directory | 100000 | `RetailT8Snapshot::Validate` |
| Compact payload | 1 GiB | `CompactZstd::MaximumPayload` |
| T8AS records | 200001 | `CompactZstd.cpp` (`MaximumRecords`) |
| Header extension sections | 64 MiB total | `RetailSnapshot.cpp` (`OuterSectionsLimit`) |
| Online play | files under 30 MB | `ONLINE_MOVESET_MAX_SIZE_BYTES` in `Utils/constants.h` |

A compressed output larger than the stored limit is refused rather than written
(`CompressionUtils::RAW::Moveset::Compress`).

## Checksums

All checksums use the standard CRC-32 (reflected, polynomial 0xEDB88320, initial and final XOR 0xFFFFFFFF):
`Helpers::CalculateCrc32` in `Utils/Helpers.cpp`. Compact + Zstd uses zlib's `crc32`, which is the same algorithm.

| File type | `crc32` covers | Checked |
| --- | --- | --- |
| Tekken 7 | One 8-byte END property `{0, 0}`, then T7 blocks 0, 1, 2, 4, 5, 6 in order (moveset info, table, motalists, moveset, animation, mota). The name block (3) and movelist block (7) are not covered. The real property list is not covered either, so the import profile does not change the checksum | Not checked on load. Shown as the hash in the moveset lists |
| Retail Tekken 8 | The whole `T8RS` payload (`moveset_data_start` to end, uncompressed) | On save (`RunFileJob`) and when the retail editor loads a file |

Writers: `T7Export::Serialize`, `MovesetFile::ConvertToT7`, `EditorT7::CalculateCRC32`,
`RetailT8Snapshot::Serialize`, `EditorT8Retail::CalculateCRC32`. Editors update `crc32` and `date` on each save
(`EditorLogic::BuildSaveSnapshot`) and keep `orig_crc32`.

## Tekken 7 payload

Block table: `StructsT7::TKMovesetHeaderBlocks` in `TKMovesets2/GameSpecific/Structures/Structs_t7.h`.
Writer: `T7Export::ReadSnapshot` / `T7Export::Serialize` in `Extractor_t7/T7ExportSnapshot.cpp`.

The block list holds 8 u64 offsets, relative to `moveset_data_start`. A block ends where the next one starts; the last
block ends at the end of the payload.

| Index | Field | Content |
| --- | --- | --- |
| 0 | `movesetInfoBlock` | `StructsT7::MovesetInfo` header (aliases, name/date string offsets). Offset 0 |
| 1 | `tableBlock` | The `MovesetTable` inside `MovesetInfo` (offset and count of each record list). `offsetof(MovesetInfo, table)` |
| 2 | `motalistsBlock` | The `MotaList` inside `MovesetInfo` (offsets of the MOTA files). `offsetof(MovesetInfo, motas)` |
| 3 | `nameBlock` | Move and animation name strings, character name, creator and dates. Starts at `sizeof(MovesetInfo)` |
| 4 | `movesetBlock` | All record lists (moves, cancels, requirements, properties...) |
| 5 | `animationBlock` | The animations used by moves |
| 6 | `motaBlock` | The extracted MOTA files |
| 7 | `movelistBlock` | The training-mode movelist (`.mvl`), may be empty |

Blocks 3 to 7 start at aligned offsets. The serialized payload is padded to 8 bytes. Pointers inside records are
stored as offsets into their block; `T7ImportUtils::ValidateT7Semantics` (`Importers/Importer_t7/Importer_t7_Utils.cpp`)
defines what a valid T7 payload is (aliases, references, list terminators, input ranges, animation and MOTA extents,
movelist targets).

T7 header values written by the exporter and converters: `header_size` = 256, `block_list_size` = 8,
`game_specific_flags` = 0, `gameVersionHash` zeros.

## Retail Tekken 8 payload: `T8RS`

A retail T8 file has `gameId` = 1, `minorVersion` = 2, `block_list_size` = 1 with a single 0 entry, and
`moveset_data_start` = `block_list` + 8. Code: `TKMovesets2/GameSpecific/Extractors/Extractor_t8/RetailSnapshot.hpp`
and `.cpp` (`RetailT8Snapshot`).

### Versions and `game_specific_flags`

| `T8RS` version | Contents after the 20 tables | `game_specific_flags` |
| --- | --- | --- |
| 1 | Nothing (tables only) | `1 << 32` (`TablesOnlyFlag`) |
| 2 | `T8AN` animation directory and PANM blobs. Every entry must have data | `1 << 33` (`RuntimeAssetsFlag`) |
| 3 | Same as 2. Entries may be size-0 markers (animation declared but not carried) | `1 << 33` |
| 4 | Same as 3, then a `T8RX` extension. The directory may be empty (tables-only export) | `1 << 33` |

The flag must match the version: version 1 uses `TablesOnlyFlag`, version 2 and later use `RuntimeAssetsFlag`. Checked
in `MovesetFile::Validate` and `RunFileJob`. Current exports and editor saves write version 4.

### Layout

| Part | Offset | Size |
| --- | --- | --- |
| `T8RS` header | 0 | 40 |
| Source header (copy of the game's moveset header) | 40 | 0x318 (792) |
| Table directory (20 entries) | 832 | 20 x 32 = 640 |
| Tables, in directory order | 1472, then each aligned | count x stride each |
| `T8AN` directory (v2+) | aligned end of last table | 32 |
| `T8AN` entries | directory + 32 | count x 32 |
| PANM blobs | aligned end of entries, each aligned | per entry |
| `T8RX` extension (v4) | aligned end of last blob | its `size` |

There are no gaps besides alignment padding, and the payload must end exactly where the last part ends.

### `T8RS` header (`RetailT8Snapshot::Header`, 40 bytes)

| Offset | Size | Type | Field | Meaning |
| --- | --- | --- | --- | --- |
| 0 | 4 | char[4] | `signature` | `T8RS` |
| 4 | 4 | u32 | `version` | 1 to 4 |
| 8 | 8 | u64 | `sourceAddress` | Game address of the moveset when captured (a synthetic address for movesets built from game files). Provenance only. Must not be 0 or all ones |
| 16 | 8 | u64 | `sourceHeaderOffset` | Always 40 |
| 24 | 8 | u64 | `directoryOffset` | Always 832 (aligned 40 + 0x318) |
| 32 | 8 | u64 | `size` | Total payload size. Must equal the payload length |

### Source header

A verbatim copy of the game's 0x318-byte moveset header. Checks in `CheckSource`: the u32 at +4 (header compilation
date) is between 20240101 and 20991231, and the move count at +0x238 is not 0. The 20 table pointer/count pairs it
contains must match the table directory.

### Table directory (`RetailT8Snapshot::Entry`, 32 bytes each)

| Offset | Size | Type | Field | Meaning |
| --- | --- | --- | --- | --- |
| 0 | 8 | u64 | `address` | Table address in the game (provenance). Equals the pointer in the source header |
| 8 | 8 | u64 | `count` | Number of records. Equals the count in the source header |
| 16 | 8 | u64 | `offset` | Payload offset of the table data |
| 24 | 4 | u32 | `stride` | Record size |
| 28 | 4 | u32 | `reserved` | 0 |

The 20 tables, in order (`RetailT8Snapshot::Tables`; source header pointer and count offsets, record stride):

| Index | Name | Pointer | Count | Stride |
| --- | --- | --- | --- | --- |
| 0 | reactions | 0x168 | 0x178 | 0x70 |
| 1 | requirements | 0x180 | 0x188 | 0x14 |
| 2 | hit_conditions | 0x190 | 0x198 | 0x18 |
| 3 | projectiles | 0x1A0 | 0x1A8 | 0xE0 |
| 4 | pushbacks | 0x1B0 | 0x1B8 | 0x10 |
| 5 | pushback_extra | 0x1C0 | 0x1C8 | 0x02 |
| 6 | cancels | 0x1D0 | 0x1D8 | 0x28 |
| 7 | group_cancels | 0x1E0 | 0x1E8 | 0x28 |
| 8 | cancel_extra | 0x1F0 | 0x1F8 | 0x04 |
| 9 | extra_properties | 0x200 | 0x208 | 0x28 |
| 10 | start_properties | 0x210 | 0x218 | 0x20 |
| 11 | end_properties | 0x220 | 0x228 | 0x20 |
| 12 | moves | 0x230 | 0x238 | 0x448 |
| 13 | voiceclips | 0x240 | 0x248 | 0x0C |
| 14 | input_sequences | 0x250 | 0x258 | 0x10 |
| 15 | inputs | 0x260 | 0x268 | 0x08 |
| 16 | parry | 0x270 | 0x278 | 0x04 |
| 17 | cameras | 0x280 | 0x288 | 0x0C |
| 18 | throws | 0x290 | 0x298 | 0x10 |
| 19 | dialogues | 0x2A0 | 0x2A8 | 0x18 |

Records are the game's original bytes, including encrypted move fields. Pointers inside records stay capture-time game
addresses; the directory's `address` values are used to resolve them.

### `T8AN` animation directory (`RetailT8Snapshot::AssetDirectory`, 32 bytes)

| Offset | Size | Type | Field | Meaning |
| --- | --- | --- | --- | --- |
| 0 | 4 | char[4] | `signature` | `T8AN` |
| 4 | 4 | u32 | `count` | Number of entries, at most 100000. 0 is allowed only in v4 |
| 8 | 8 | u64 | `entriesOffset` | Directory offset + 32 |
| 16 | 8 | u64 | `dataOffset` | Aligned (`entriesOffset` + count x 32) |
| 24 | 8 | u64 | `reserved` | 0 |

### `T8AN` entry (`RetailT8Snapshot::AssetEntry`, 32 bytes)

| Offset | Size | Type | Field | Meaning |
| --- | --- | --- | --- | --- |
| 0 | 8 | u64 | `address` | Where the data came from (below) |
| 8 | 8 | u64 | `offset` | Payload offset of the PANM blob |
| 16 | 8 | u64 | `size` | Blob size, at most 32 MiB. 0 = marker without data (v3+) |
| 24 | 4 | u32 | `instance` | Animation instance id |
| 28 | 4 | u32 | `type` | Category: 0 body, 1 hands, 2 face, 3 swing, 4 camera, 5 extra |

`address` values:

| Value | Meaning |
| --- | --- |
| A game pointer | PANM captured from game memory |
| `0x00007FFFFFFE0000` (`ArchiveAssetAddress`) | Read from the game's `tkdata.bin` archive (not loaded at export, or built from game files) |
| `0x00007FFFFFFF0000` (`T7AnimToRetailT8::CustomAssetAddress`) | Converted from a T7 animation |
| 0 | Size-0 marker: the animation is declared but no data is carried |

Entry rules: `(type, instance)` pairs are unique; blobs follow in entry order, each at the aligned cursor;
`offset` equals that cursor; a non-empty blob has `PANM` at byte 4 and its size equals the extent computed from its
FlatBuffers metadata (`RetailT8Snapshot::PanmSize`). A size-0 entry needs version 3 or later and `address` 0.

### `T8RX` extension (v4, `RetailT8Snapshot::ExtensionHeader`, 64-byte header)

Holds the move name strings and external cancel lists that a memory-only import would otherwise read from the live
game. All offsets are relative to the start of the extension. Code: `BuildExtension`, `ParseExtension`,
`ReadExtension`, `AppendExtension`.

| Offset | Size | Type | Field | Meaning |
| --- | --- | --- | --- | --- |
| 0 | 4 | char[4] | `signature` | `T8RX` |
| 4 | 4 | u32 | `version` | 1 |
| 8 | 8 | u64 | `size` | Total extension size, multiple of 8 |
| 16 | 8 | u64 | `moveCount` | Rows in the string reference table, at most 0x100000 |
| 24 | 8 | u64 | `stringsOffset` | Aligned (64 + moveCount x 8) |
| 32 | 8 | u64 | `stringsSize` | Size of the string blob |
| 40 | 8 | u64 | `cancelsOffset` | Aligned (stringsOffset + stringsSize) |
| 48 | 8 | u64 | `cancelCount` | External cancel lists, at most 0x100000 |
| 56 | 4 | u32 | `crc32` | CRC-32 of every byte after this 64-byte header |
| 60 | 4 | u32 | `reserved` | 0 |

After the header:

1. `moveCount` x `{u32 name, u32 animation}`: offsets into the string blob for each captured move row (move fields
   +0x40 and +0x48). `0xFFFFFFFF` = not captured.
2. The string blob: pooled NUL-terminated strings, at most 1024 characters each.
3. `cancelCount` x `ExternalCancelEntry` (24 bytes): `u32 row`, `u32 field` (move field 0x98, 0xA0, 0xB0 or
   0xC0), `u64 offset`, `u64 size`. `(row, field)` pairs are unique.
4. The cancel lists, each at an aligned offset: 40-byte cancel entries copied verbatim, at most 4096 per list, the
   last one with command 0x8000.

The extension must end exactly at `size`, which must end exactly at the end of the payload.

### `T8RS` validation (`RetailT8Snapshot::Validate`)

- payload <= 1 GiB; signature `T8RS`; version 1..4;
- `size` = payload length, fixed offsets 40 and 832, `sourceAddress` set;
- source header checks (`CheckSource`);
- each directory entry matches the source header and the fixed stride, `reserved` = 0, tables in order at aligned
  offsets;
- v2+: `T8AN` directory and entry rules above;
- v4: `T8RX` rules above;
- no trailing bytes.

`MovesetFile::Validate` (`LocalStorage/MovesetFile.cpp`) adds: flags match the version, block list is one 0, and when
`header_size` > 256 the editor provenance (`RetailEditorData::Read`) is valid.

## Validation on save

`RunFileJob` (`LocalStorage/FileJob.cpp`) validates every snapshot before writing it. The snapshot must be
uncompressed and pass `ValidateHeader`, then:

- retail T8: `header_size` >= 0xF8, block list is one 0, `T8RS` validates, `game_specific_flags` matches the version,
  and `crc32` equals the payload CRC;
- otherwise: `gameId` must be T7, `block_list_size` >= 8, and `ValidateT7Semantics` must pass.

Only after this is the snapshot compressed with the chosen codec and written.

## Atomic writes

`FileIO::WriteAtomically` (`TKMovesets2/Utils/FileIO.cpp`):

1. Create a temporary file `tkm*.tmp` in the destination's directory (`GetTempFileNameW`).
2. Write the content in chunks of at most 1 GiB.
3. Flush it to disk (`FlushFileBuffers`) and close it.
4. Replace the destination with `MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`.
5. On any failure, delete the temporary file. The destination is left as it was.

Exports, editor saves, conversions, recompression (`CompressionUtils::FILE::Moveset::Compress`), import-profile edits
and backups all go through this function.

## Backups

Editor saves and auto-saves run through `RunFileJob` with backup options set by `EditorVisuals::Save` and
`EditorVisuals::SaveBackup` (`Layouts/EditorVisuals/EditorVisuals.cpp`).

Backups go to `editor_auto_backups/` next to the moveset (`MOVESET_AUTO_BACKUPDIRECTORY`). Names are
`<moveset name><tag><dd-mm-yy_HHhMM>.tkmvst`, with ` (2)`, ` (3)`... added if the name exists.

| Kind | Tag | When | Content |
| --- | --- | --- | --- |
| Original | `_[ORIGINAL]_` | On a normal editor save, only when no original backup exists yet | The file on disk before the first save, byte for byte (or the new snapshot in LZ4 if no file exists) |
| Auto-save | `_[BAK]_` | Periodic editor backups (`backupOnly`) | The current editor snapshot, LZ4 compressed. The moveset file itself is not touched |

At most `maxBackups` (5 in the editor) files are kept per tag. Older ones are deleted, oldest first by modification
time, only after the new backup is written. Only files whose names match the generated pattern exactly are counted
or deleted (`IsGeneratedBackup`). The backup is written before the moveset file is replaced.
