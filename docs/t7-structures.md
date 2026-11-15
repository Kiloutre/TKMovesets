# Tekken 7 moveset structures

This page lists the Tekken 7 moveset records as the tool defines them: strides, field offsets, types, terminators and how
records reference each other. Offsets are hexadecimal. Field names are the ones used in the code; fields named `_0x..`
or `_unk..` have no known meaning. The older games' structures are summarised at the end.

Main sources:

- `TKMovesets2/GameSpecific/Structures/Structs_t7.h`: `StructsT7` (pointer types, used for the local copy and the
  injected DLL) and `StructsT7_gameAddr` (the same records with `gameAddr` fields, used when the value is an index or a
  remote address).
- `TKMovesets2/LocalStorage/MovesetStructs.h`: the `.tkmvst` file header.
- `TKMovesets2/GameSpecific/Extractors/Extractor_t7/T7ExportSnapshot.cpp` (`T7Export::ReadSnapshot`, `Serialize`):
  memory -> file conversion of every reference.
- `TKMovesets2/GameSpecific/Importers/Importer_t7/Importer_t7_Utils.cpp` (`ConvertMovesetIndexes`,
  `ValidateT7Layout`, `ValidateT7Semantics`): file -> memory conversion and the validation rules.

All T7 records are little-endian. Pointers are 8 bytes.

## Three representations

The same records exist in three forms. The record layout (stride and field offsets) is the same in all three; only the
meaning of reference fields changes.

| Form | Where | Reference fields hold |
| --- | --- | --- |
| Game memory | the running game, or the tool's import allocation | absolute game addresses; `0` = none |
| `.tkmvst` file | `T7Export::Serialize` output, editor buffer | row index into the target table; `0xFFFFFFFFFFFFFFFF` (`MOVESET_ADDR_MISSING`) = none |
| Game files (`mothead\bin\<code>mot.bin`) | `T7DiskMoveset.cpp` | row index; `-1` = none (projectile references: `0` = none); offsets relative to `+0x2E8` |

Conversion between the first two forms:

- memory -> file: `Index()` in `T7ExportSnapshot.cpp` computes `(pointer - table start) / stride`. A pointer that does
  not land on a row start of its own target table aborts the export. `0` and `UINT64_MAX` become `MOVESET_ADDR_MISSING`.
- file -> memory: `RelocateIndex()` in `Importer_t7_Utils.cpp` computes `allocation + movesetBlock + table offset +
  index * stride`. An index outside its table makes the import fail. `MOVESET_ADDR_MISSING` becomes `0`.

Each reference points to the first row of a list. The list then runs over consecutive rows until its terminator (see
[Terminators](#terminators-and-special-values)).

## File container (`.tkmvst`)

`TKMovesetHeader` (`MovesetStructs.h`), at file offset 0. Size 0x100.

| Offset | Type | Field | Meaning |
| --- | --- | --- | --- |
| 0x00 | char[4] | `_signature` | `TKM2` |
| 0x04 | char[28] | `version_string` | tool version at extraction (`MOVESET_VERSION_STRING`, `1.0`; `d` appended in debug builds) |
| 0x20 | Byte[32] | `gameVersionHash` | zero for T7 |
| 0x40 | uint16 | `gameId` | `GameId_T7` for T7 files |
| 0x42 | uint16 | `minorVersion` | `GameVersions::T7::NONE` |
| 0x44 | uint32 | `header_size` | size of this header, aligned to 8 |
| 0x48 | uint32 | `block_list` | file offset of the block offset list |
| 0x4C | uint32 | `block_list_size` | number of block offsets (8 for T7) |
| 0x50 | uint32 | `moveset_data_start` | file offset of the moveset payload |
| 0x58 | uint64 | `moveset_data_size` | decompressed payload size; 0 when not compressed |
| 0x60 | uint8 | `compressionType` | 0 none, 1 LZ4, 2 LZMA, 3 compact zstd |
| 0x64 | uint32 | `crc32` | checksum of the payload (see below) |
| 0x68 | uint32 | `orig_crc32` | checksum at extraction time |
| 0x70 | uint64 | `date` | last modification timestamp |
| 0x78 | uint64 | `extraction_date` | extraction timestamp |
| 0x80 | uint32 | `characterId` | character id of the source player (`chara_id_offset`) |
| 0x88 | uint64 | `flags` | bit 0 modified, bit 1 debug build |
| 0x90 | char[32] | `origin` | game name, `Tekken 7` |
| 0xB0 | char[32] | `target_character` | display name |
| 0xD0 | char[32] | `orig_character_name` | display name at extraction |
| 0xF0 | uint64 | `game_specific_flags` | 0 for T7 |
| 0xF8 | uint16 | `conversion_origin` | source game when the file was converted |

Between `header_size` and `block_list` is the custom property list: `TKMovesetProperty { int id; int value; }`, ended
by `id == 0` (`TKMovesetProperty_END`). It holds the import profile (`LocalStorage/ImportProfile.cpp`).

The block list is `StructsT7::TKMovesetHeaderBlocks`: eight `uint64` offsets, relative to the start of the payload
(`moveset_data_start`). A block ends where the next one starts; the last ends at the end of the payload.

| Index | Field | Value written by the exporter | Contents |
| --- | --- | --- | --- |
| 0 | `movesetInfoBlock` | 0 | `MovesetInfo` |
| 1 | `tableBlock` | 0x150 | `MovesetInfo::table` |
| 2 | `motalistsBlock` | 0x280 | `MovesetInfo::motas` |
| 3 | `nameBlock` | 0x2E8 | NUL-terminated strings: metadata, move names, animation names |
| 4 | `movesetBlock` | 8-aligned | the 19 record tables, in table order, each 8-aligned |
| 5 | `animationBlock` | 8-aligned | every move animation, little-endian, back to back |
| 6 | `motaBlock` | 8-aligned | the selected MOTA archives |
| 7 | `movelistBlock` | 8-aligned | training move list (`MVLT`), or 8 zero bytes when there is none |

`ValidateT7Layout` requires block 0 to be 0, blocks sorted, `tableBlock` and `motalistsBlock` at those exact offsets,
every table inside the moveset block and no two non-empty tables overlapping.

The checksum (`T7Export::Serialize`, `Helpers::CalculateCrc32`) covers, in order: one empty property entry, payload
`[0, tableBlock)`, the table, the MOTA list, the moveset block, the animation block and the MOTA block. The name block
and the move list block are not covered, and neither is the real property list.

## MovesetInfo (moveset header)

`StructsT7::MovesetInfo`, size 0x2E8. In memory it is the address stored at `player + motbin_offset`. In a file it is
payload offset 0.

| Offset | Type | Field | Meaning |
| --- | --- | --- | --- |
| 0x000 | char[2] | `_0x0` | unknown |
| 0x002 | bool | `isInitialized` | 1 once the game has loaded the moveset (checked by `ExtractorT7::CanExtract`) |
| 0x003 | char[5] | `_0x3` | unknown |
| 0x008 | char* | `character_name_addr` | character name string |
| 0x010 | char* | `character_creator_addr` | creator string |
| 0x018 | char* | `date_addr` | date string |
| 0x020 | char* | `fulldate_addr` | full date string |
| 0x028 | uint16[56] | `orig_aliases` | default move id for each alias |
| 0x098 | uint16[56] | `current_aliases` | current move id for each alias |
| 0x108 | uint16[36] | `unknown_values` | unknown (`unknown_aliases` in `StructsT7_gameAddr`) |
| 0x150 | MovesetTable | `table` | 19 pointer/count pairs |
| 0x280 | MotaList | `motas` | 13 MOTA pointers |

Move ids `0x8000` and above are aliases: id `0x8000 + n` plays `current_aliases[n]`.

String fields in a file are payload-relative offsets (`nameBlock + offset`); `nullptr` means no string. In memory they
are absolute. At import `CorrectMovesetInfoValues` adds the allocation address to all four fields, including a null
one. The exporter adds the prefix `Tkm:` (`MOVESET_EXTRACTED_NAME_PREFIX`) to the stored character name, removing a
leading `[`.

The exporter reads only the first 0x2E0 bytes from the game (`SourceInfoSize`): the thirteenth MOTA pointer is never
read and is written as missing.

### MovesetTable

`MovesetInfo + 0x150`, 19 entries of `{ pointer listAddr; uint64 listCount; }` (16 bytes each, 0x130 total). In memory
`listAddr` is an absolute address. In a file it is an offset inside the moveset block. Index `i` is at
`0x150 + 0x10 * i`.

| Index | Field | Record | Stride |
| --- | --- | --- | --- |
| 0 | `reactions` | `Reactions` | 0x70 |
| 1 | `requirement` | `Requirement` | 0x08 |
| 2 | `hitCondition` | `HitCondition` | 0x18 |
| 3 | `projectile` | `Projectile` | 0xA8 |
| 4 | `pushback` | `Pushback` | 0x10 |
| 5 | `pushbackExtradata` | `PushbackExtradata` | 0x02 |
| 6 | `cancel` | `Cancel` | 0x28 |
| 7 | `groupCancel` | `Cancel` (grouped cancels) | 0x28 |
| 8 | `cancelExtradata` | `CancelExtradata` | 0x04 |
| 9 | `extraMoveProperty` | `ExtraMoveProperty` | 0x0C |
| 10 | `moveBeginningProp` | `OtherMoveProperty` (start properties) | 0x10 |
| 11 | `moveEndingProp` | `OtherMoveProperty` (end properties) | 0x10 |
| 12 | `move` | `Move` | 0xB0 |
| 13 | `voiceclip` | `Voiceclip` | 0x04 |
| 14 | `inputSequence` | `InputSequence` | 0x10 |
| 15 | `input` | `Input` | 0x08 |
| 16 | `unknownParryRelated` | `UnknownParryRelated` | 0x04 |
| 17 | `cameraData` | `CameraData` | 0x0C |
| 18 | `throwCameras` | `ThrowCamera` | 0x10 |

The same strides are listed in `T7Export::RecordSizes` (`T7ExportSnapshot.hpp`) and `ValidateT7Layout`.

### MotaList

`MovesetInfo + 0x280`, 13 pointers (0x68 bytes). Slot names from `StructsT7::MotaList`:

| Slot | Offset | Field | Contents | Extract flag |
| --- | --- | --- | --- | --- |
| 0 | 0x280 | `mota_0` | body animations | `ExtractSettings_MOTA_0_ANIM` |
| 1 | 0x288 | `mota_1` | body animations | `ExtractSettings_MOTA_1_ANIM` |
| 2 | 0x290 | `hand_1` | hands | `ExtractSettings_MOTA_2_HAND` |
| 3 | 0x298 | `hand_2` | hands | `ExtractSettings_MOTA_3_HAND` |
| 4 | 0x2A0 | `face_1` | face | `ExtractSettings_MOTA_4_FACE` |
| 5 | 0x2A8 | `face_2` | face | `ExtractSettings_MOTA_5_FACE` |
| 6 | 0x2B0 | `wings_1` | wings (more unknown) | `ExtractSettings_MOTA_6_WINGS` |
| 7 | 0x2B8 | `wings_2` | wings (more unknown) | `ExtractSettings_MOTA_7_WINGS` |
| 8 | 0x2C0 | `camera_1` | camera | `ExtractSettings_MOTA_8_CAMERA` |
| 9 | 0x2C8 | `camera_2` | camera | `ExtractSettings_MOTA_9_CAMERA` |
| 10 | 0x2D0 | `mota_10` | unknown | `ExtractSettings_MOTA_10_UNKNOWN` |
| 11 | 0x2D8 | `mota_11` | unknown | `ExtractSettings_MOTA_11_UNKNOWN` |
| 12 | 0x2E0 | `_unknown_` | unknown pointer, sometimes just before a MOTA | never extracted |

In a file each slot is an offset inside the MOTA block, or `MOVESET_ADDR_MISSING`. Several slots may share one
archive (one copy is stored). In the game's files, even slots are MOTAs inside the motbin and odd slots come from
`<code>rotsm.arc` (see [t7-behaviour.md](t7-behaviour.md#extraction-from-the-game-files)).

## Moves

`Move`, stride 0xB0.

| Offset | Type | Field | Meaning |
| --- | --- | --- | --- |
| 0x00 | char* | `name_addr` | move name |
| 0x08 | char* | `anim_name_addr` | animation name |
| 0x10 | void* | `anim_addr` | animation data |
| 0x18 | uint32 | `vuln` | vulnerability |
| 0x1C | uint32 | `hitlevel` | hit level |
| 0x20 | Cancel* | `cancel_addr` | main cancel list |
| 0x28 | Cancel* | `_0x28_cancel_addr` | secondary cancel list |
| 0x30 | int32 | `_0x30_int__0x28_related` | gate of `+0x28` (list present when >= 1) |
| 0x34 | int32 | `_0x34_int` | unknown |
| 0x38 | Cancel* | `_0x38_cancel_addr` | secondary cancel list |
| 0x40 | int32 | `_0x40_int__0x38_related` | gate of `+0x38` |
| 0x44 | int32 | `_0x44_int` | unknown |
| 0x48 | Cancel* | `_0x48_cancel_addr` | secondary cancel list |
| 0x50 | uint32 | `_0x50_int__0x48_related` | gate of `+0x48` |
| 0x54 | uint16 | `transition` | move played at the end (move id or alias) |
| 0x56 | int16 | `_0x56_short` | unknown |
| 0x58 | uint16 | `moveId_val1` | related to the move id |
| 0x5A | uint16 | `moveId_val2` | related to the character id |
| 0x5C | int16 | `_0x5C_short` | unknown |
| 0x5E | int16 | `_0x5E_short` | unknown |
| 0x60 | HitCondition* | `hit_condition_addr` | hit condition list |
| 0x68 | int32 | `anim_len` | animation length in frames |
| 0x6C | uint32 | `airborne_start` | |
| 0x70 | uint32 | `airborne_end` | |
| 0x74 | uint32 | `ground_fall` | |
| 0x78 | Voiceclip* | `voicelip_addr` | voice clip list, may be null |
| 0x80 | ExtraMoveProperty* | `extra_move_property_addr` | extra property list, may be null |
| 0x88 | OtherMoveProperty* | `move_start_extraprop_addr` | start property list, may be null |
| 0x90 | OtherMoveProperty* | `move_end_extraprop_addr` | end property list, may be null |
| 0x98 | int32 | `_0x98_int` | unknown ("facing/extras") |
| 0x9C | uint32 | `hitbox_location` | |
| 0xA0 | uint32 | `first_active_frame` | |
| 0xA4 | uint32 | `last_active_frame` | |
| 0xA8 | int16 | `_0xA8_short` | unknown |
| 0xAA | uint16 | `distance` | |
| 0xAC | int32 | `_0xAC_int` | unknown |

References in a file: `name_addr` and `anim_name_addr` are offsets in the name block; `anim_addr` is an offset in the
animation block (must be below the MOTA block size); every list pointer is a row index. Moves are referenced by id
(the row index), never by address, except `nextmove_addr` in the player structure.

The animation at `anim_addr` starts with its type byte: `0x64` or `0xC8` (little-endian in memory; the game's files
hold big-endian animations). Parsing is in `TKMovesets2/Utils/Animations*` (`TAnimUtils`).

## Cancels and grouped cancels

`Cancel`, stride 0x28. Used by both the `cancel` and the `groupCancel` tables.

| Offset | Type | Field | Meaning |
| --- | --- | --- | --- |
| 0x00 | uint64 | `command` | input; low 32 bits `direction`, high 32 bits `button`; special values below |
| 0x08 | Requirement* | `requirements_addr` | requirement list |
| 0x10 | CancelExtradata* | `extradata_addr` | one cancel extradata row |
| 0x18 | uint32 | `detection_start` | first frame the input is read |
| 0x1C | uint32 | `detection_end` | last frame the input is read |
| 0x20 | uint32 | `starting_frame` | frame the target move starts |
| 0x24 | uint16 | `move_id` | target move id or alias; for `0x800B`, a grouped-cancel row index |
| 0x26 | uint16 | `cancel_option` | option bits; computed by the game at load (see below) |

Special `command` values (`EditorT7::LoadMoveset` constants, `ValidateT7Semantics`):

| Value | Meaning |
| --- | --- |
| `0x8000` | end of a cancel list (this row is the last) |
| `0x800B` | jump to the grouped-cancel list whose first row is `move_id` |
| `0x800C` | end of a grouped-cancel list |
| `0x800D` and above (below `0x10000`) | input sequence `command - 0x800D` |

`cancel_option` bits, as the game computes them when it loads a moveset from its files (`T7Disk::BuildFile`):
1 = command is 0; 2 = a requirement id `>= 0x32C` without bit 15; 4 = a requirement id `>= 0x349` without bit 15;
8 = has requirement `0x84CE`; 0x10 = (command not 0, or has requirement 0x83, 0x84 or 0x85) and none of 0x22E, 0x22F,
0x27A, 0x32B, 0x27C; 0x40 = cancel extradata `& 0x3C00` is not `0x2800`; 0x80 = extradata `& 0x3F` is `0x16`;
0x100 (main cancels only) = target move id has bit 15 and is `0x8024`, `<= 0x801D` or `>= 0x8028`.

`CancelExtradata`, stride 4: `+0x0 uint32 value`. Meaning not documented in the code beyond the option bits above.

## Requirements

`Requirement`, stride 8.

| Offset | Type | Field |
| --- | --- | --- |
| 0x0 | uint32 | `condition` |
| 0x4 | uint32 / int32 / float | `param_unsigned` / `param_signed` / `param_float` |

A list ends on the row whose `condition` is 881 (`t7_req_end` in `game_addresses.ini`). Condition 217
(`t7_character_id_condition`) tests the character id; the importer rewrites it (see
[t7-behaviour.md](t7-behaviour.md#import)).

## Extra properties, start and end properties

`ExtraMoveProperty`, stride 0xC.

| Offset | Type | Field | Meaning |
| --- | --- | --- | --- |
| 0x0 | uint32 | `starting_frame` | frame the property runs; 32769 (`0x8001`) = instant |
| 0x4 | uint32 | `id` | property id |
| 0x8 | uint32 / int32 / float | `value_unsigned` / `value_signed` / `value_float` | parameter |

A list ends on the row whose `id` is 0, whatever its `starting_frame`.

`OtherMoveProperty` (start and end properties), stride 0x10.

| Offset | Type | Field | Meaning |
| --- | --- | --- | --- |
| 0x0 | Requirement* | `requirements_addr` | requirement list |
| 0x8 | uint32 | `extraprop` | property id; 881 ends the list |
| 0xC | uint32 | `value` | parameter |

## Hit conditions, reactions, pushbacks

`HitCondition`, stride 0x18.

| Offset | Type | Field | Meaning |
| --- | --- | --- | --- |
| 0x00 | Requirement* | `requirements_addr` | requirement list |
| 0x08 | uint32 | `damage` | |
| 0x0C | uint32 | `_0xC_int` | unknown |
| 0x10 | Reactions* | `reactions_addr` | reactions row |

A hit-condition list runs until the row whose requirement list starts with a 881 row (the catch-all last entry).
`ValidateT7Semantics` checks every list reaches such a row.

`Reactions`, stride 0x70.

| Offset | Type | Field |
| --- | --- | --- |
| 0x00 | Pushback* | `front_pushback` |
| 0x08 | Pushback* | `backturned_pushback` |
| 0x10 | Pushback* | `left_side_pushback` |
| 0x18 | Pushback* | `right_side_pushback` |
| 0x20 | Pushback* | `front_counterhit_pushback` |
| 0x28 | Pushback* | `downed_pushback` |
| 0x30 | Pushback* | `block_pushback` |
| 0x38 | uint16 | `front_direction` |
| 0x3A | uint16 | `back_direction` |
| 0x3C | uint16 | `left_side_direction` |
| 0x3E | uint16 | `right_side_direction` |
| 0x40 | uint16 | `front_counterhit_direction` |
| 0x42 | uint16 | `downed_direction` |
| 0x44 | uint32 | `_0x44_int` |
| 0x48 | uint32 | `_0x48_int` |
| 0x4C | uint16 | `vertical_pushback` |
| 0x4E | uint16 | `standing_moveid` |
| 0x50 | uint16 | `default_moveid` |
| 0x52 | uint16 | `crouch_moveid` |
| 0x54 | uint16 | `counterhit_moveid` |
| 0x56 | uint16 | `crouch_counterhit_moveid` |
| 0x58 | uint16 | `left_side_moveid` |
| 0x5A | uint16 | `crouch_left_side_moveid` |
| 0x5C | uint16 | `right_side_moveid` |
| 0x5E | uint16 | `crouch_right_side_moveid` |
| 0x60 | uint16 | `backturned_moveid` |
| 0x62 | uint16 | `crouch_backturned_moveid` |
| 0x64 | uint16 | `block_moveid` |
| 0x66 | uint16 | `crouch_block_moveid` |
| 0x68 | uint16 | `wallslump_moveid` |
| 0x6A | uint16 | `downed_moveid` |
| 0x6C | uint32 | `_0x6c` (padding, always 0; only in `StructsT7_gameAddr`) |

The seven pushbacks are also `pushbacks[7]` and the sixteen u16 from 0x4C are `moveids[16]`. `moveids[0]` is
`vertical_pushback`, not a move id.

`Pushback`, stride 0x10.

| Offset | Type | Field | Meaning |
| --- | --- | --- | --- |
| 0x0 | uint16 | `duration` | |
| 0x2 | uint16 | `displacement` | |
| 0x4 | uint32 | `num_of_loops` | unknown; assumed to be the number of extradata rows used |
| 0x8 | PushbackExtradata* | `extradata_addr` | first extradata row |

`PushbackExtradata`, stride 2: `+0x0 uint16 horizontal_offset`.

## Voice clips

`Voiceclip`, stride 4: `+0x0 uint32 id`. A list ends on `id == 0xFFFFFFFF`.

## Input sequences and inputs

`InputSequence`, stride 0x10.

| Offset | Type | Field | Meaning |
| --- | --- | --- | --- |
| 0x0 | uint16 | `input_window_frames` | assumed: frames allowed between first and last input |
| 0x2 | uint16 | `input_amount` | number of `Input` rows |
| 0x4 | int32 | `_0x4_int` | apparently unused |
| 0x8 | Input* | `input_addr` | first input row; when `input_amount` is 0 it is exported as missing |

`Input`, stride 8: `+0x0 uint64 command` (low 32 bits `direction`, high 32 bits `button`). Inputs are counted, not
terminated. A cancel uses sequence `n` through `command = 0x800D + n`.

## Projectiles

`Projectile`, stride 0xA8.

| Offset | Type | Field | Meaning |
| --- | --- | --- | --- |
| 0x00 | uint32 | `vfx_id` | |
| 0x04 | int32 | `_0x4_int` | unknown |
| 0x08 | uint32 | `vfx_variation_id` | |
| 0x0C | int32 | `_0xC_int` | unknown |
| 0x10 | int32 | `_0x10_int` | unknown |
| 0x14 | int32 | `_0x14_int` | unknown |
| 0x18 | uint32 | `delay` | frames before both velocities apply |
| 0x1C | uint32 | `vertical_velocity` | |
| 0x20 | uint32 | `horizontal_velocity` | |
| 0x24 | int32 | `_0x24_int` | 0 disables the projectile |
| 0x28 | uint32 | `duration` | |
| 0x2C | uint32 | `no_collision` | non-zero: no collision |
| 0x30 | uint32 | `size` | 0: no collision |
| 0x34 | int32 | `_0x34_int` | unknown |
| 0x38 | uint32 | `hit_level` | |
| 0x3C | int32[6] | `_0x3C_int` | unknown |
| 0x54 | uint32 | `voiceclip_on_hit` | |
| 0x58 | int32 | `_0x58_int` | unknown |
| 0x5C | int32 | `_0x5C_int` | unknown |
| 0x60 | HitCondition* | `hit_condition_addr` | hit condition list |
| 0x68 | Cancel* | `cancel_addr` | cancel list |
| 0x70 | int32 | `_0x70_int` | unknown |
| 0x74 | int32 | `_0x74_int` | unknown |
| 0x78 | uint32 | `can_hitbox_connect` | |
| 0x7C | int32 | `_0x7C_int` | unknown |
| 0x80 | int32 | `_0x80_int` | unknown |
| 0x84 | uint32 | `gravity` | |
| 0x88 | int32[8] | `_0x88_int` | unknown |

Projectiles are started by a move property (`0x820B`, `EditorConstants_ProjectileProperty`) whose value is a
projectile row. In the game's files a projectile reference of 0 means none, and the game fills `+0x78` with
`+0x30 * 2` and `+0x7C` with `+0x34` when they are 0.

## Throw cameras and camera data

`ThrowCamera`, stride 0x10.

| Offset | Type | Field | Meaning |
| --- | --- | --- | --- |
| 0x0 | uint64 | `side` | |
| 0x8 | CameraData* | `cameradata_addr` | camera data row |

`CameraData`, stride 0xC.

| Offset | Type | Field |
| --- | --- | --- |
| 0x0 | uint32 | `_0x0_int` |
| 0x4 | uint16 | `_0x4_short` |
| 0x6 | uint16 | `left_side_camera_data` |
| 0x8 | uint16 | `right_side_camera_data` |
| 0xA | uint16 | `_0xA_short` |

`UnknownParryRelated`, stride 4: `+0x0 uint32 value`. Meaning unknown; nothing references it.

## Reference summary

| From | Field | To | Null in memory |
| --- | --- | --- | --- |
| Move | `cancel_addr`, `_0x28/_0x38/_0x48_cancel_addr` | Cancel | 0 |
| Move | `hit_condition_addr` | HitCondition | 0 |
| Move | `voicelip_addr` | Voiceclip | 0 |
| Move | `extra_move_property_addr` | ExtraMoveProperty | 0 |
| Move | `move_start_extraprop_addr`, `move_end_extraprop_addr` | OtherMoveProperty (begin / end table) | 0 |
| Move | `name_addr`, `anim_name_addr` | name block | never null |
| Move | `anim_addr` | animation block | never null |
| Projectile | `cancel_addr`, `hit_condition_addr` | Cancel, HitCondition | 0 |
| Cancel, grouped cancel | `requirements_addr` | Requirement | 0 |
| Cancel, grouped cancel | `extradata_addr` | CancelExtradata | 0 |
| HitCondition | `requirements_addr`, `reactions_addr` | Requirement, Reactions | 0 |
| Reactions | the seven pushbacks | Pushback | 0 |
| Pushback | `extradata_addr` | PushbackExtradata | 0 |
| InputSequence | `input_addr` | Input | 0 |
| ThrowCamera | `cameradata_addr` | CameraData | 0 |
| OtherMoveProperty | `requirements_addr` | Requirement | 0 |
| Cancel | `move_id` | move id, alias, or grouped-cancel row (`0x800B`) | by value |
| Cancel | `command >= 0x800D` | input sequence row | by value |
| Move | `transition`; Reactions `moveids[1..15]` | move id or alias | by value |

Index-to-pointer code: `T7ImportUtils::ConvertMovesetIndexes`. Pointer-to-index code: the `REF` macro in
`T7Export::ReadSnapshot`. Disk-index-to-pointer code: the `ref` lambda in `T7Disk::BuildFile`.

## Terminators and special values

| List | Ends on | Source |
| --- | --- | --- |
| Requirement | `condition == 881` | `t7_req_end`, `ValidateT7Semantics` |
| Cancel | `command == 0x8000` | `EditorConstants_CancelCommandEnd` |
| Grouped cancel | `command == 0x800C` | `EditorConstants_GroupedCancelCommandEnd` |
| Extra property | `id == 0` | `EditorConstants_ExtraPropertyEnd` |
| Start / end property | `extraprop == 881` | `ValidateT7Semantics` |
| Voice clip | `id == 0xFFFFFFFF` | `ValidateT7Semantics` |
| Hit condition | row whose requirement list starts with 881 | `ValidateT7Semantics` |
| Input | counted by `InputSequence::input_amount` | |
| Pushback extradata | no terminator in the code (assumed counted by `Pushback::num_of_loops`) | |

Other constants: move id `>= 0x8000` = alias; `0x8001` (32769) as a move id is the alias the importer plays after an
instant import; `MOVESET_ADDR_MISSING` = `(uint64)-1`, `MOVESET_ADDR32_MISSING` = `(uint32)-1`
(`Utils/GameTypes.h`).

## MOTA archives

`StructsT7::MotaHeader`. Byte 5 is 1 for a big-endian archive (this is what the exporter and the validator read).

| Offset | Type | Field | Meaning |
| --- | --- | --- | --- |
| 0x00 | char[4] | `mota_string` | `MOTA` |
| 0x04 | bool | `_is_little_endian` | |
| 0x05 | bool | `_is_big_endian` | 1 = big-endian |
| 0x06 | uint16 | `unk0x6` | unknown |
| 0x08 | uint32 | `unk0x8` | unknown |
| 0x0C | uint32 | `anim_count` | number of animations |
| 0x10 | char | `unk0x10` | unknown |
| 0x11 | bool | `is_little_endian` | the byte the game checks |
| 0x12 | char | `unk0x12` | unknown |
| 0x13 | char | `unk0x13` | unknown |
| 0x14 | uint32[anim_count] | `anim_offset_list` | animation offsets from the archive start |

Each offset must be at least `0x14 + 4 * anim_count` and inside the archive. The exporter sizes an archive as its
highest animation offset plus that animation's size, checks every member animation, and stores it little-endian
(`TAnimUtils::FromMemory::ByteswapMota`). An archive with an invalid member is dropped (`Snapshot::discardedMotas`).

## Training move list (MVLT)

The move list block holds a copy of the game's `.mvl` file. Structures in `Structs_t7.h`.

`MvlHead`, 0x20 bytes before the strings.

| Offset | Type | Field | Meaning |
| --- | --- | --- | --- |
| 0x00 | char[4] | `mvlString` | `MVLT` |
| 0x04 | int32 | `_unk0x4` | low 16 bits are 5 on disk and 1000 in memory; meaning unknown |
| 0x08 | int32 | `displayables_count` | |
| 0x0C | int32 | `playables_count` | |
| 0x10 | int32 | `_unk0x10` | unknown |
| 0x14 | uint32 | `displayables_offset` | from the head |
| 0x18 | uint32 | `playables_offset` | from the head |
| 0x1C | uint32 | `inputs_offset` | from the head |
| 0x20 | char[] | `translation_strings` | string pool |

`MvlDisplayable`, stride 0x174 (one move list row).

| Offset | Type | Field | Meaning |
| --- | --- | --- | --- |
| 0x000 | int32[4] | `title_translation_offsets` | Japanese, English, Japanese 2, other |
| 0x010 | int32[12] | `translation_offsets` | Japanese, English, Portuguese, Spanish, French, Italian, German, Mexican, Cyrillic, Arabic, Chinese, Korean |
| 0x040 | uint32 | `_unk0x40` | unknown |
| 0x044 | int16 | `playable_id` | `MvlPlayable` row |
| 0x046 | uint16 | `_unk0x46` | unknown |
| 0x048 | uint32 | `type` | 0x2 disabled, 0x8 category, 0xA combo list start, 0xD unknown, 0x10000 regular |
| 0x04C .. 0x148 | uint32 | `_unk0x4c` .. `_unk0x148` | unknown |
| 0x14C | uint32 | `icons` | |
| 0x150 | uint8 | `icons_2` | |
| 0x151 | uint8 | `combo_damage` | |
| 0x152 | uint8 | `combo_difficulty` | |
| 0x153 | uint8 | `_unk0x153` | unknown |
| 0x154 .. 0x170 | uint32 | `_unk0x154` .. `_unk0x170` | unknown |

`MvlPlayable`, stride 0x18 (the demonstration the bot plays).

| Offset | Type | Field |
| --- | --- | --- |
| 0x00 | uint16 | `p2_action` |
| 0x02 | uint16 | `distance` |
| 0x04 | uint16 | `p2_rotation` |
| 0x06 | uint16 | `_unk0x6` |
| 0x08 | uint16 | `_unk0x8` |
| 0x0A | uint16 | `p1_facing_related` |
| 0x0C | uint16 | `_unk0xc` |
| 0x0E | uint16 | `input_count` |
| 0x10 | uint32 | `input_sequence_offset` |
| 0x14 | uint16 | `has_rage` |
| 0x16 | uint16 | `_unk0x16` |

`MvlInput`, stride 4: `+0x0 uint8 directions`, `+0x1 uint8 buttons` (together `uint16 command`), `+0x2 uint8
frame_duration`, `+0x3 uint8 trigger_highlight`.

Offset conventions differ by form:

| Field | Game memory | `.tkmvst` file | Game files (`.mvl`) |
| --- | --- | --- | --- |
| translation offsets | bytes from the start of their own displayable row (negative) | bytes from the `MvlHead` | bytes from `translation_strings` |
| `input_sequence_offset` | bytes from the start of its own playable row | index into the input array | index into the input array |

The exporter (`ReadTraining` in `T7ExportSnapshot.cpp`) writes the head, the deduplicated strings from 0x20, then
8-aligned displayables, playables and inputs. The importer converts back with
`T7ImportUtils::ConvertDisplayableMovelistOffsets`.

`MvlManager` (the game's training move list state, one per player, stride 0x80, array at `movelist_manager_addr`):

| Offset | Type | Field |
| --- | --- | --- |
| 0x00 | uint32 | `playingType` |
| 0x08 | void* | `player` |
| 0x10 | uint32 | `displayableEntriesCount` |
| 0x18 | void* | `mvlDisplayableBlock` |
| 0x20 | MvlHead* | `mvlHead` |
| 0x30 | uint32 | `selected_entry` |
| 0x60 | MvlInput* | `sequenceEnd` |
| 0x68 | MvlInput* | `sequenceStart` |
| 0x70 | int32 | `frame` |
| 0x74 | int32 | `currentInputFrame` |

Other fields (`_unk0x4`, `_unk0x14`, `_unk0x28` .. `_unk0x5c`, `_unk0x78`, `_unk0x7c`) are unknown.

## Older games

Struct files: `Structs_ttt2.h` (`StructsTTT2`, also used for Tekken Revolution), `Structs_t6.h` (`StructsT6`),
`Structs_t5.h` (`StructsT5`) and `Structs_t5dr.h` (`StructsT5DR`, only a different `MovesetInfo` on top of the T5
records). The comments in those files say many `_0x..` names were copied from T7 and do not match their offsets.

Common differences from T7:

- Pointers and table entries are 32-bit (`gameAddr32`; table entries `{ uint32 listAddr; uint32 listCount; }`).
- The games are console games read through emulators (PS3 for TTT2, TREV, T6 and T5 DR; PS2 for T5). The extractors
  byte-swap fields into little-endian when they write the file (for example `Extractor_ttt2.cpp`).
- In their `.tkmvst` files references are row indexes in 32-bit fields, `0xFFFFFFFF` = none. The converters widen
  this to `MOVESET_ADDR_MISSING` (`CONVERT_POSSIBLE_MISSING_ADDR`).
- TTT2 and TREV use the same 8-block list as T7. T6 and T5 use 5 blocks: `movesetInfoBlock`, `nameBlock`,
  `movesetBlock`, `animationBlock`, `motaBlock` (no table, MOTA list or move list block).
- `MOTA` headers are the same; `AltMotaHeader` describes another archive format found in these games, not usable by T7.

| Record | T7 | TTT2 / TREV | T6 | T5 / T5 DR |
| --- | --- | --- | --- | --- |
| Move | 0xB0 | 0x70 | 0x58 | 0x4C |
| Cancel | 0x28 | 0x20 | 0x20 | 0x18 |
| Requirement | 0x08 | 0x08 | 0x08 | 0x04 (16-bit condition and parameter) |
| Reactions | 0x70 | 0x50 | 0x50 | 0x50 |
| HitCondition | 0x18 | 0x0C (no `_0xC_int`) | 0x0C | 0x0C |
| Pushback | 0x10 | 0x0C | 0x0C | 0x0C |
| ExtraMoveProperty | 0x0C | 0x0C | 0x0C | 0x08 (16-bit frame and id) |
| OtherMoveProperty | 0x10 | 0x0C | 0x0C | 0x08 |
| Voiceclip | 0x04 | 0x04 | 0x04 | 0x02 |
| Input | 0x08 | 0x08 | 0x08 | 0x04 (16-bit direction and button) |
| InputSequence | 0x10 | 0x08 (int8, then uint8 window) | 0x08 | 0x08 (uint8 window, then int8) |
| Projectile | 0xA8 | 0x88, layout unknown (`gap`) | 0x88 `gap`, no table entry | 0x88 `gap`, no table entry |
| ThrowCamera | 0x10 | 0x08 | 0x08 | none |
| MovesetTable entries | 19 | 19 | 24 | 23 |
| Aliases | 2 x 56 uint16 | 2 x 56 uint16 | 2 x 69 uint16 | 2 x 36 uint32 |

Per game:

- **TTT2 / TREV** (`StructsTTT2`): same table order as T7. `MovesetInfo` 0x20C bytes, table at 0x140, MOTA list at
  0x1D8. `Move` keeps the three secondary cancel lists but has no `_0x34`, `_0x44`, `_0x5C`, `_0x5E`, `_0xAC` fields;
  `moveId_val2` comes before `moveId_val1`. `ThrowCamera` is `{ uint32 _0x0_uint; gameAddr32 cameradata_addr; }`.
- **T6** (`StructsT6`): no move names (`Move` starts with `_0x0_int`, `_0x4_int`), one cancel list per move, table
  without projectiles but with move list tables (`mvlPlayable`, `mvlInput`, `mvlDisplayable`) and unknown tables
  `StructA3`, `StructA5`, `StructA6` (always empty in tested movesets), then `cameraData` and `throwCameras`.
  `MovesetInfo` 0x268 bytes, table at 0x174.
- **T5** (`StructsT5`): many 16-bit fields (requirements, frames, voice clip ids, extra property frame and id). `Cancel`
  is `{ uint32 command; requirements; uint32 move_id; extradata; uint16 detection_start, detection_end,
  starting_frame, cancel_option; }`. Table of 23 entries without projectile or camera tables, with move list
  tables and unknown `StructA1` .. `StructA5`. `MovesetInfo` 0x26C bytes, table at 0x180.
- **T5 DR** (`StructsT5DR::MovesetInfo`): T5 records; `MovesetInfo` has 8 unknown bytes (`gap`) before the table,
  which starts at 0x188.

Offsets of the older `MovesetInfo` layouts above are computed from the struct definitions. How they are turned into
T7 records is described in [t7-behaviour.md](t7-behaviour.md#older-games-to-t7-conversion).
