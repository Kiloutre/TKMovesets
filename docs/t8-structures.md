# Retail Tekken 8 moveset and animation structures

This page describes the data layouts TKMovesets2 uses for retail Tekken 8, Steam build 26155785. It covers the
moveset as the game holds it in memory (the `.motbin` after the game's load step), the record tables, the encrypted
move fields, and the animation side (PANM data, ANMBIN archives, animation handles and the game's animation maps).

All offsets are hexadecimal. Fields named `_0xNN` or marked "unknown" are not identified. In memory every pointer is
absolute. On disk (inside `tkdata.bin`) table pointers in the header are relative to the end of the header and
record pointers are row indices; the conversion is described in [t8-behaviour.md](t8-behaviour.md#building-movesets-from-the-games-files).

Main sources:

- `TKMovesets2/GameSpecific/Structures/Structs_t8.h` (namespace `StructsT8`): record structs with `static_assert`
  offsets.
- `TKMovesets2/GameSpecific/Extractors/Extractor_t8/RetailSnapshot.hpp` (`RetailT8Snapshot::Tables`): pointer/count
  offsets and strides of the 20 tables.
- `TKMovesets2/GameSpecific/Importers/Importer_t8/RetailImport.hpp` (`RetailT8Import::PointerFields`): the typed
  pointer graph between tables.
- `TKMovesets2/GameSpecific/Editors/Editor_t8Retail/Editor_t8Retail.cpp` (`EditorT8Retail::BuildSchema`,
  `Decode`, `Encode`, `SetPlainCopy`): field schema and encryption.
- `TKMovesets2/GameSpecific/Extractors/Extractor_t8/RetailDiskMoveset.cpp` (`RetailT8Disk::Relocate`): disk form,
  disk encryption and fields derived by the load step.
- `TKMovesets2/GameSpecific/Extractors/Extractor_t8/RetailRuntime.hpp` (namespace `RetailT8Runtime`): animation
  maps, handles and player lookup.

## Table index numbers

The code refers to tables by index (snapshot directory order, `RetailT8Snapshot::Tables`). The same numbers are used
below.

| # | Name | Record struct | Stride |
|---:|---|---|---:|
| 0 | reactions | `Reactions` | 0x70 |
| 1 | requirements | `Requirement` | 0x14 |
| 2 | hit_conditions | `HitCondition` | 0x18 |
| 3 | projectiles | `Projectile` | 0xE0 |
| 4 | pushbacks | `Pushback` | 0x10 |
| 5 | pushback_extra | `PushbackExtra` | 0x2 |
| 6 | cancels | `Cancel` | 0x28 |
| 7 | group_cancels | `Cancel` | 0x28 |
| 8 | cancel_extra | `CancelExtra` | 0x4 |
| 9 | extra_properties | `ExtraMoveProperty` | 0x28 |
| 10 | start_properties | `OtherMoveProperty` | 0x20 |
| 11 | end_properties | `OtherMoveProperty` | 0x20 |
| 12 | moves | `Move` | 0x448 |
| 13 | voiceclips | `Voiceclip` | 0xC |
| 14 | input_sequences | `InputSequence` | 0x10 |
| 15 | inputs | `Input` | 0x8 |
| 16 | parry | `Parry` | 0x4 |
| 17 | cameras | `CameraData` | 0xC |
| 18 | throws | `ThrowCamera` | 0x10 |
| 19 | dialogues | `Dialogue` | 0x18 |

## Source header (0x318 bytes)

`StructsT8::MovesetHeader`. The moveset root pointer in the player struct points at this header.

| Offset | Type | Name | Meaning |
|---|---|---|---|
| 0x000 | u16 | skip_animation_lookup | 0 on disk: move +0x50 holds a body animation list index that the load step converts |
| 0x002 | u8 | relocated | set to 1 by the load step |
| 0x003 | u8 | _0x3 | unknown |
| 0x004 | u32 | compile_date | date as a decimal number (for example 2024xxxx). The tool accepts 20240101..20991231 and uses it as the game build check |
| 0x008 | char[8] | signature | `"TEK"` |
| 0x010 | ptr[4] | strings | character name, creator, date, full date. In retail memory these point at a shared placeholder |
| 0x030 | u16[60] | orig_aliases | original alias table |
| 0x0A8 | u16[60] | current_aliases | alias table the game uses: alias `0x8000 + i` resolves to move `current_aliases[i]` |
| 0x120 | u16[32] | unknown_aliases | unknown |
| 0x160 | u32 | previous_identity | identity code of (character id - 1) |
| 0x164 | u32 | identity | identity code of the character id |
| 0x168 | ptr | reactions | table 0 pointer |
| 0x170 | ptr | _0x170 | runtime value, cleared by the load step |
| 0x178 | u64 | reactions_count | table 0 count |
| 0x180 | {ptr, u64} x 19 | tables 1..19 | pointer/count pairs, see below |
| 0x2B0 | ptr[13] | runtime_slots | runtime resources (0x2B0..0x310), cleared by the load step |

Alias 0x8001 is the idle stance: the importer and preview read the idle row from header +0xAA. Aliases are
resolved for ids 0x8000..0x803B (`EditorT8Retail::ResolveMove`, `RetailT8Preview::Queue`).

### The 20 table pointer/count pairs

The reactions pair is not uniform: there is an extra qword (+0x170) between its pointer and its count.

| # | Table | Pointer offset | Count offset |
|---:|---|---|---|
| 0 | reactions | 0x168 | 0x178 |
| 1 | requirements | 0x180 | 0x188 |
| 2 | hit_conditions | 0x190 | 0x198 |
| 3 | projectiles | 0x1A0 | 0x1A8 |
| 4 | pushbacks | 0x1B0 | 0x1B8 |
| 5 | pushback_extra | 0x1C0 | 0x1C8 |
| 6 | cancels | 0x1D0 | 0x1D8 |
| 7 | group_cancels | 0x1E0 | 0x1E8 |
| 8 | cancel_extra | 0x1F0 | 0x1F8 |
| 9 | extra_properties | 0x200 | 0x208 |
| 10 | start_properties | 0x210 | 0x218 |
| 11 | end_properties | 0x220 | 0x228 |
| 12 | moves | 0x230 | 0x238 |
| 13 | voiceclips | 0x240 | 0x248 |
| 14 | input_sequences | 0x250 | 0x258 |
| 15 | inputs | 0x260 | 0x268 |
| 16 | parry | 0x270 | 0x278 |
| 17 | cameras | 0x280 | 0x288 |
| 18 | throws | 0x290 | 0x298 |
| 19 | dialogues | 0x2A0 | 0x2A8 |

Each table is read through its own pointer and count. The tool does not assume the tables are adjacent or in this
order in memory.

### Identity codes

`code(n) = n << 16 | (0x10000 - n)` (low 16 bits). The header holds `code(id - 1)` at +0x160 and `code(id)` at +0x164,
and every move holds `code(id - 1)` in its encrypted `character_id` field (+0xD0). Note that
`code(id - 1) = id * 0xFFFF + 1`; the animation slot code reads the animation domain back as
`(root[+0x160] - 1) / 0xFFFF` (RE notes, `.docs/retail-t8-nonbody-anim-re.md` 2.1).

Implemented in `RetailT8Import::ApplyConvertedIdentity` (`Importer_t8/RetailImport.cpp`). The disk load step writes
`0x73FF8D` style values only for character 0x74 (`RetailT8Disk::Relocate`).

## Typed pointer graph

`RetailT8Import::PointerFields` lists every pointer field that refers to another table. Import, live edit, image
audit and the editor's rebuilder all use this list. A pointer must point at the start of a row of its target table,
or be null.

| Source table | Field offset | Target table |
|---|---|---|
| 0 reactions | 0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30 | 4 pushbacks |
| 2 hit_conditions | 0x00 | 1 requirements |
| 2 hit_conditions | 0x10 | 0 reactions |
| 3 projectiles | 0x90 | 2 hit_conditions |
| 3 projectiles | 0x98 | 6 cancels |
| 4 pushbacks | 0x08 | 5 pushback_extra |
| 6 cancels, 7 group_cancels | 0x08 | 1 requirements |
| 6 cancels, 7 group_cancels | 0x10 | 8 cancel_extra |
| 9 extra_properties | 0x08 | 1 requirements |
| 10 start_properties, 11 end_properties | 0x00 | 1 requirements |
| 12 moves | 0x98, 0xA0, 0xB0, 0xC0 | 6 cancels (may be external, see below) |
| 12 moves | 0x110 | 2 hit_conditions |
| 12 moves | 0x130 | 13 voiceclips |
| 12 moves | 0x138 | 9 extra_properties |
| 12 moves | 0x140 | 10 start_properties |
| 12 moves | 0x148 | 11 end_properties |
| 14 input_sequences | 0x08 | 15 inputs |
| 18 throws | 0x08 | 17 cameras |
| 19 dialogues | 0x08 | 1 requirements |

Other references are ids, not pointers:

| Where | Field | Refers to |
|---|---|---|
| Cancel (6) | move_id +0x24 (u16) | move row or alias 0x8000..0x803B |
| Cancel (6) with command 0x8012 | move_id +0x24 | group_cancels row |
| Cancel (6, 7) command | low 32 bits - 0x8014 | input_sequences row (when low 32 bits are 0x8014..0xFFFF) |
| Move (12) | transition +0xCC | move row or alias |
| Reactions (0) | moveid[14] +0x50..+0x6B | move rows or aliases |
| Property 33403 (tables 9, 10, 11) | parameter 1 | projectiles row |
| Property 34447 (tables 9, 10, 11) | parameter 1 | throws row |
| Header | aliases +0x30..+0x167 (u16) | move rows |

Sources: `EditorT8Retail::ReferencesTo` and `EditorT8Retail::Rebuild` (`RetailEditorOperations.cpp`),
`RetailT8Commands` (`MovesetConverters/RetailCommands.hpp`).

### External cancel lists

Move fields 0x98, 0xA0, 0xB0 and 0xC0 normally point into the cancel table. Some native moves point at a cancel list
that lives outside it. These lists are 40-byte cancel records ending with command 0x8000, at most 4096 entries in the
tool. Their requirement and extra pointers do point into the moveset's own tables. The banks at 0xA0, 0xB0 and 0xC0 are
active only when their signed gate at +8 (0xA8, 0xB8, 0xC8) is greater than 0; otherwise the pointer is unused data.
See `RetailT8Snapshot::ExternalCancelFields` and `RetailT8Snapshot::CaptureNative`.

## Record tables

### Reactions (table 0, 0x70)

| Offset | Type | Name | Meaning |
|---|---|---|---|
| 0x00 | ptr[7] | pushbacks | front, backturned, left_side, right_side, front_counterhit, downed, block |
| 0x38 | u16[6] | direction | front, back, left_side, right_side, front_counterhit, downed |
| 0x44 | u16[4] | rotation | front, back, left_side, right_side |
| 0x4C | s16 | vertical_pushback | |
| 0x4E | u16 | downed_rotation | 0 in every retail file; not a move id |
| 0x50 | u16[14] | moveid | standing, crouch, counterhit, crouch_counterhit, left_side, crouch_left_side, right_side, crouch_right_side, backturned, crouch_backturned, block, crouch_block, wallslump, downed. The only move ids the game's reaction picker reads |
| 0x6C | u16[2] | _0x6C | 0 in every retail file |

### Requirements (table 1, 0x14)

| Offset | Type | Name |
|---|---|---|
| 0x00 | u32 | condition |
| 0x04 | u32[4] | params |

A requirement list ends with condition 1100 (0x44C). Condition ids used by the tool:

| Condition | Meaning in the tool |
|---|---|
| 220, 221 | own character identity, scalar form (low 16 bits of param 0; legacy selector codes 0x81..0x8F accepted) |
| 499, 500 | own character identity, list form (all four params; 125 = unused entry) |
| 224, 225 | secondary identity, scalar form |
| 503, 504 | secondary identity, list form (255 = unused entry) |
| 689, 690 | gender (import profile) |
| 777 | story flag (import profile) |

See `RetailT8Import::AdaptCharacterRequirements` and `ApplyProfileConditions`.

### Hit conditions (table 2, 0x18)

| Offset | Type | Name |
|---|---|---|
| 0x00 | ptr | requirements |
| 0x08 | u32 | damage |
| 0x0C | u32 | _0xC (unknown) |
| 0x10 | ptr | reactions |

A hit condition list ends with the row whose requirement list starts with 1100 (`EditorT8Retail::ListSize`).

### Projectiles (table 3, 0xE0)

| Offset | Type | Name | Meaning |
|---|---|---|---|
| 0x00 | u32 | vfx_id | |
| 0x04 | u32[11] | _0x4 | unknown |
| 0x30 | u32 | _0x30 | unknown; load step uses it for 0xAC |
| 0x34 | u32 | _0x34 | unknown; load step uses it for 0xB0 |
| 0x38 | u32 | _0x38 | unknown |
| 0x3C | u32 | hit_level | |
| 0x40 | u32[10] | _0x40 | unknown |
| 0x68 | u32[3] | voiceclip | {category, 0, id} |
| 0x74 | u32[7] | _0x74 | unknown |
| 0x90 | ptr | hit_condition | |
| 0x98 | ptr | cancel | |
| 0xA0..0xA8 | u32 x3 | | unknown |
| 0xAC | u32 | _0xAC | load step: if 0, set to 2 x [0x30] |
| 0xB0 | u32 | _0xB0 | load step: if 0, set to [0x34] |
| 0xB4..0xDF | | | unknown |

### Pushbacks (table 4, 0x10) and pushback extra (table 5, 0x2)

| Offset | Type | Name |
|---|---|---|
| 0x00 | u16 | duration |
| 0x02 | u16 | displacement |
| 0x04 | u32 | extra_count (number of pushback_extra rows) |
| 0x08 | ptr | extra |

Pushback extra is one u16 `value`.

### Cancels (table 6) and group cancels (table 7), 0x28

| Offset | Type | Name | Meaning |
|---|---|---|---|
| 0x00 | u64 | command | direction bits in the low dword, buttons in the high dword, or a special value |
| 0x08 | ptr | requirements | |
| 0x10 | ptr | extra | cancel_extra row |
| 0x18 | u32 | window_start | |
| 0x1C | u32 | window_end | |
| 0x20 | u32 | starting_frame | |
| 0x24 | u16 | move_id | move row, alias, or group_cancels row (command 0x8012) |
| 0x26 | u16 | option | derived by the game's load step (see below) |

Special commands (`RetailCommands.hpp`):

| Value | Meaning |
|---|---|
| 0x8000 | list end ("AUTO"); terminates cancel lists |
| 0x8012 | group cancel: move_id is a group_cancels row |
| 0x8013 | group list end; terminates group cancel lists |
| 0x8014 + n | input sequence n (low dword 0x8014..0xFFFF) |
| 0x8001..0x8011 | other specials; meaning unknown |

Command bits: directions in low-dword bits 1..9 (D/B, D, D/F, B, N, F, U/B, U, U/F). High dword: buttons 1..4 in bits
0..3, Heat bit 4, Special Style bit 5, Rage Art bit 6, bit 29 = any one of the listed buttons.

Option bits computed at load (`RetailT8Disk::Relocate`; requirement list read up to 1100):

| Bit | Set when |
|---|---|
| 0x1 | command is 0 |
| 0x2 | a requirement id > 0x406, not 0x419, without bit 15 |
| 0x4 | a requirement id > 0x423 without bit 15 |
| 0x8 | requirement 0x87FB present |
| 0x10 | (command != 0 or requirement 0x83..0x87 present) and none of 0x29B, 0x29C, 0x3FF, 0x2E9, 0x321, 0x322 |
| 0x40 | cancel extra `& 0x3C00` is neither 0x2800 nor 0x2C00 |
| 0x80 | cancel extra `& 0x3F` == 0x16 |
| 0x100 | main cancels only: move_id has bit 15 and is 0x8027, below 0x8021 or above 0x802A |
| 0x200 | main cancels only: requirement 0x9F with param 1 |

### Cancel extra (table 8, 0x4)

One u32 `value`. Meaning partly known through the option bits above.

### Extra properties (table 9, 0x28)

| Offset | Type | Name |
|---|---|---|
| 0x00 | u32 | frame |
| 0x04 | u32 | _0x4 (unknown) |
| 0x08 | ptr | requirements |
| 0x10 | u32 | id (property) |
| 0x14 | u32[5] | params |

The list ends at the row with property id 0, whatever the frame.

### Start properties (table 10) and end properties (table 11), 0x20

| Offset | Type | Name |
|---|---|---|
| 0x00 | ptr | requirements |
| 0x08 | u32 | id (property) |
| 0x0C | u32[5] | params |

The list ends at the row with id 1100.

### Moves (table 12, 0x448)

| Offset | Type | Name | Meaning |
|---|---|---|---|
| 0x000 | EncryptedValue | name_key | Kamui hash of the move name |
| 0x020 | EncryptedValue | anim_key | Kamui hash of the animation name |
| 0x040 | ptr | name_addr | move name string (retail memory: shared placeholder) |
| 0x048 | ptr | anim_name_addr | animation name string |
| 0x050 | u64 | anim_addr | disk: body animation list index (masked 0xFFFF3FFF); memory: animation handle |
| 0x058 | EncryptedValue | vuln | vulnerability |
| 0x078 | EncryptedValue | hit_level | |
| 0x098 | ptr | cancels | |
| 0x0A0 | ptr | cancels_2 | used only when cancel_gate_2 > 0 |
| 0x0A8 | s32 | cancel_gate_2 | |
| 0x0AC | u32 | _0xAC | unknown |
| 0x0B0 | ptr | cancels_3 | |
| 0x0B8 | s32 | cancel_gate_3 | |
| 0x0BC | u32 | _0xBC | unknown |
| 0x0C0 | ptr | cancels_4 | |
| 0x0C8 | s32 | cancel_gate_4 | |
| 0x0CC | u16 | transition | move id or alias |
| 0x0CE | u16 | end_rotation | |
| 0x0D0 | EncryptedValue | character_id | `code(id - 1)` |
| 0x0F0 | EncryptedValue | move_key | Kamui hash of a `CODE_name` string |
| 0x110 | ptr | hit_conditions | |
| 0x118 | s16 | damage_override | |
| 0x11A | u16 | _0x11A | unknown |
| 0x11C | u32 | animation_length_adjuster | |
| 0x120 | u32 | animation_length | load step: frames, minus (adjuster - 1) when adjuster > 1; 0xFFFFFFFF when the game has no data for the animation |
| 0x124 | u32 | airborne_start | |
| 0x128 | u32 | airborne_end | |
| 0x12C | u32 | ground_fall | |
| 0x130 | ptr | voiceclip | disk: -1 = null |
| 0x138 | ptr | extra_properties | disk: -1 = null |
| 0x140 | ptr | start_properties | disk: -1 = null |
| 0x148 | ptr | end_properties | disk: -1 = null |
| 0x150 | u32 | _0x150 | unknown |
| 0x154 | u32 | _0x154 | unknown |
| 0x158 | u32 | active_start | overall active window; 0 = no hitboxes |
| 0x15C | u32 | active_end | |
| 0x160 | Hitbox[8] | hitboxes | 0x30 each |
| 0x2E0 | u16 | collision | |
| 0x2E2 | u16 | distance | |
| 0x2E4 | u32[89] | _0x2E4 | unknown; native defaults repeat every 0x2C (8 slots) |

Hitbox (0x30):

| Offset | Type | Name | Meaning |
|---|---|---|---|
| 0x00 | u32 | start | first active frame of this slot |
| 0x04 | u32 | last | last active frame |
| 0x08 | u32 | location | byte 0 limb A, byte 1 limb B (0 = sweep of A from the previous frame) |
| 0x0C | f32 | radius | 0 = character default |
| 0x10 | f32 | vertical_height | > 0: vertical segment of that height centred on A |
| 0x14 | f32[3] | offset_a | axis order not verified |
| 0x20 | f32[3] | offset_b | |
| 0x2C | u32 | starts_off | low byte copied to the player's per-slot off flag at move start |

### Voiceclips (table 13, 0xC)

| Offset | Type | Name |
|---|---|---|
| 0x0 | u32 | category |
| 0x4 | u32 | _0x4 (unknown) |
| 0x8 | u32 | id |

The list ends with a row whose first u32 is 0xFFFFFFFF.

### Input sequences (table 14, 0x10) and inputs (table 15, 0x8)

| Offset | Type | Name |
|---|---|---|
| 0x0 | u16 | window |
| 0x2 | u16 | input_count |
| 0x4 | u32 | _0x4 (unknown) |
| 0x8 | ptr | inputs (first of `input_count` rows) |

Input: u32 `direction`, u32 `buttons` (same bit meanings as a cancel command's low and high dword).

### Parry (table 16, 0x4)

One u32 `value`. Meaning unknown (the editor labels it `key`).

### Cameras (table 17, 0xC) and throw cameras (table 18, 0x10)

CameraData:

| Offset | Type | Name |
|---|---|---|
| 0x0 | u32 | probability |
| 0x4 | u16 | type |
| 0x6 | u16 | left_camera |
| 0x8 | u16 | right_camera |
| 0xA | u16 | rotation |

ThrowCamera: u64 `side` at 0x0, ptr `camera` (cameras row) at 0x8.

### Dialogues (table 19, 0x18)

| Offset | Type | Name |
|---|---|---|
| 0x00 | u16 | type |
| 0x02 | u16 | id |
| 0x04 | u32 | _0x4 (unknown) |
| 0x08 | ptr | requirements |
| 0x10 | u32 | voice_key |
| 0x14 | s32 | facial_animation |

### List terminators (summary)

| Table | Terminator |
|---|---|
| requirements | condition 1100 |
| cancels | command 0x8000 |
| group_cancels | command 0x8000 or 0x8013 |
| extra_properties | property id 0 |
| start_properties, end_properties | id 1100 |
| voiceclips | first u32 0xFFFFFFFF |
| hit_conditions | row whose requirement list starts with 1100 |
| inputs, pushback_extra | no terminator: length comes from input_count / extra_count |

Source: `EditorT8Retail::ListSize` and `EditorT8Retail::SaveRecord`.

## Encrypted move fields

Six move fields are 0x20-byte `EncryptedValue` blocks: name_key (0x00), anim_key (0x20), vuln (0x58), hit_level (0x78),
character_id (0xD0), move_key (0xF0).

| Offset in block | Type | Meaning |
|---|---|---|
| 0x00 | u64 | encrypted value |
| 0x08 | u64 | key |
| 0x10 | u32[4] | check slots |

### Memory form

Decode (`EditorT8Retail::Decode`, also `RetailNames::Decode`):

```
check = xor over i in 0..3 of ((low32(value) >> 8*i) ^ low32(rotl64(key, 8*i + 8)))
if check == 0: check = 1
valid only if high32(value) == check
s = value ^ 0x1D
plain = low32((rotl64(key, s & 31) & ~31) ^ s)
```

Encode (`EditorT8Retail::Encode`):

```
low = plain ^ low32(rotl64(key, plain & 31) & ~31) ^ 0x1D
value = low | (check computed from low as above, 1 if 0) << 32
```

The key is kept per field; editing a value re-encodes it with the existing key.

Check slots: in native moves the four u32 slots hold `0x765 + row`, except that when `row % 8 >= 4`, slot
`row % 8 - 4` holds the plain value. The game cross-checks this copy (a stale hit-level copy makes the attack never
connect). See `EditorT8Retail::SetPlainCopy` and `EditorT8Retail::DuplicateStructure`.

### Disk form

In the `.motbin` inside `tkdata.bin` each block is eight u32 words. With `K` the 16-word key table and
`Perm = {4, 3, 2, 6, 15, 0, 1, 5}`, word j is `K[Perm[j]] ^ t` for a per-block noise value t, except one word that
holds `value ^ K[Perm[j]]`. Decoding XORs each word with `K[Perm[j]]` and takes the odd one out (or t when all are
equal). See `DecodeDisk` in `RetailDiskMoveset.cpp`. The same 16-word table `K` is used to XOR the `tkdata.bin` footer.

### Names

Move and animation names are not stored as strings in retail records: name_key and anim_key hold the Kamui hash of
the name (`RetailNames::Hash`, a 32-bit hash with length-dependent branches). TKMovesets files carry the strings in a
names section (see [t8-behaviour.md](t8-behaviour.md#file-written)).

## Animations

### Categories

Six categories, in ANMBIN order (`RetailT8Animations::Categories`):

| Index | Name |
|---:|---|
| 0 | body (full body) |
| 1 | hands |
| 2 | face (facial) |
| 3 | swing ("wing") |
| 4 | camera |
| 5 | extra |

Moves reference only body animations (move +0x50). The other categories are played through properties and per-player
animation slots by list index; this part comes from the RE notes (`.docs/retail-t8-nonbody-anim-re.md`) and is not used
by import code.

### Animation handles

A body animation handle (move +0x50 in memory) is:

```
0xEF00_0000_0000_0000 | (domain & 0xFF) << 40 | instance
```

- `domain` is the animation character (asset id), independent of the player's visual identity. 0x81 is the shared
  domain (`com.anmbin`).
- `instance` (low 32 bits) is the animation id inside the domain.
- Bits 32..39 and 48..55 are 0. A handle with the high 32 bits all zero is a legacy unqualified value and is skipped.
- `RetailT8Runtime::AnimationCharacter` reads the domain from all handles of a move table and refuses mixed domains or
  domain 255.

Instance ranges used by the tool:

| Range | Use |
|---|---|
| native ids | game animations (from ANMBIN list 0) |
| 0x00010000 and up | custom PANMs in a document (editor imports, converted T7 animations); `FirstCustomInstance` in `RetailPanmOperations.cpp` |
| 0x80000000 and up | ids the import registers in the game's map: `0x80000000 | ((FNV-1a32(panm) + probe) & 0x7FFFFFFF)` (`RetailT8Resources::ChooseAnimationId`) |

Converted T7 files use domain 124 (`T7ToRetailT8::ConvertedCharacter`) as a placeholder.

### Game animation maps

All maps below are MSVC `unordered_map` layouts: FNV-1a 64-bit over the 4 key bytes (`RetailT8Runtime::Hash`),
16-byte buckets `{lo, hi}`, empty bucket = `{head, head}`. Readers walk a bucket from `hi` through `prev` links until
`lo` (`RetailT8Runtime::Lookup`, bounded to 16384 nodes). Node layout: +0x00 next, +0x08 prev, +0x10 u32 key, +0x18 value.

Animation container (`anim_container` path in `game_addresses.ini`), domain -> vector of instance maps:

| Offset | Meaning |
|---|---|
| container +0x08 | list head |
| container +0x18 | buckets |
| container +0x30 | mask |
| node +0x10 | u32 domain |
| node +0x18 / +0x20 | begin / end of a vector of instance-map pointers; vector[0] is the map readers use |

Instance map (vector[0]):

| Offset | Meaning |
|---|---|
| map +0x00 | pointer to an object whose +0x08 is a resource whose +0x118 points at `{data, size}` of the retained ANMBIN |
| map +0x10 | list head |
| map +0x18 | count |
| map +0x20 | buckets |
| map +0x38 | mask |
| map +0x48 | u8: 1 when the map is populated |
| node +0x10 | u32 instance |
| node +0x18 | ResourceRef pointer; ResourceRef +0x08 = AnimResource |

AnimResource (0x1A8 bytes; fields the tool reads or builds):

| Offset | Meaning |
|---|---|
| +0x000 | vtable (retail RVA 0x85A59C0) |
| +0x008 | id |
| +0x010 | 3 in entries the tool builds; meaning unknown |
| +0x118 | owner of the retained ANMBIN (see instance map) |
| +0x140 | load request; request +0x30 == 2 means done |
| +0x190 | compiled animation object |
| +0x198 | shared-pointer control block |
| +0x1A0 | raw PANM pointer (may be stale after the archive is replaced) |

Resolution order for a body handle is the shared domain 0x81 first, then the character's domain. The game's
resolver takes the 0x81 entry only when it is ready (finished request, compiled object with the game's vtable);
`RetailT8Import::ReadyAnimation` follows the same rule. Sources: `RetailT8Runtime::ResolveAnimation`, `Panm`,
`DomainMap`, `MapNode`, `RetailT8Resources::FindAnimationResource`.

Animation manager (`anim_manager` path), category index per domain, used by extraction to list a character's
animation slots:

| Offset | Meaning |
|---|---|
| manager +0x2090 | list head of map A (domain -> inner map) |
| manager +0x20A0 | buckets |
| manager +0x20B8 | mask |
| domain node +0x20 | inner list head |
| domain node +0x28 | inner count |
| inner node +0x10 | u32 key = list index | 1 << (20 + category) |
| inner node +0x18 | u64 handle |

Bits 14 and 15 of a list index are flags and are masked out (`index & 0xFFFF3FFF`). Source:
`RetailT8Snapshot::CaptureComplete`; layout notes in `.docs/retail-t8-nonbody-anim-re.md` 1.1.

### ANMBIN archive

`<code>.anmbin` (per character) and `com.anmbin` (shared domain 0x81), inside `tkdata.bin` under `mothead/bin/`.
The game keeps the archive in memory with the same layout, except that the pool and PANM offsets have been turned into
absolute pointers by the engine (`RetailT8Runtime::ArchivePanm`). Header is at least 0x98 bytes.

| Offset | Type | Meaning |
|---|---|---|
| 0x04 + 4 x cat | u32 | pool record count of category `cat` |
| 0x1C + 4 x cat | u32 | reference list count of category `cat` |
| 0x38 + 8 x cat | u64 | pool offset of category `cat` |
| 0x68 + 8 x cat | u64 | reference list offset of category `cat` |

Pool record (0x38 bytes): +0x00 u64 instance (low 32 bits used), +0x08 u64 PANM offset. Offset 0 means a shared
reference: the data lives in `com.anmbin`. The other bytes are unknown. Reference list `cat` is an array of u32
animation ids; list 0 maps a move's disk animation index to its body animation id. Sources:
`RetailT8GameData::Anmbin`, `RetailT8Runtime::ArchivePanm`.

An identity can be declared (a pool record exists) without any data in any archive. On build 26155785 this is about
9 slots per character.

### PANM

A PANM is a FlatBuffers buffer with file identifier `"PANM"` at +4 and the root table offset at +0. Field numbers
below are FlatBuffers field indices.

| Table | Field | Meaning |
|---|---|---|
| root | 2 | u8 kind; must be 1 |
| root | 3 | animation table |
| animation | 1 | frames - 1 |
| animation | 2 | f32 fps |
| animation | 3 | enum 1..3 (native: 1) |
| animation | 4 | vector of bone groups |
| animation | 5 | sample base: start of the raw sample block (buffer size) |
| animation | 6 | sample block extent |
| animation | 7, 9 | copied by the engine; meaning unknown |
| animation | 8 | keyframed data; 0 in raw files (the tool refuses nonzero for building) |
| animation | 10 | mode table: field 1 flag, field 2 rate, field 3 u32 list |
| group | 1 | group type 1..5 (native 1) |
| group | 2 | vector of bones |
| bone | 1 | name string |
| bone | 2 | vector of tracks |
| track | 1 | name (`"Transform"`) |
| track | 2 | type 1..9 (Transform = 1) |
| track | 3 | encoding: 1 raw per frame, 2 static, 3 compressed |
| track | 4 | sample count |
| track | 5 | option table of byte flags (fields 1, 2, 3, 4, 8) |
| track | 6 | sample offset from the sample base |
| track | 7 | sample size |

Total size = animation field 5 + animation field 6 (`RetailT8Snapshot::PanmSize`). Frame count = field 1 + 1
(`EditorT8Retail::PanmFrames`). Native body PANMs have one bone group and one Transform track per bone. The editor
accepts a raw PANM only with the native body structure (Top/Trans, at least 20 bones; `EditorT8Retail::ValidatePanm`).
Which FlatBuffers builder details the game's decoder depends on is unknown; the encoder matches native output
(`.docs/retail-t8-panm-layout.md`).

### Compiled animation object (0xF0 bytes)

What the engine's PANM compile returns and what an AnimResource +0x190 points at. The tool builds it locally in
`RetailT8Resources::BuildCompiledAnimation` (`Importer_t8/RetailResourceLoader.cpp`).

| Offset | Content |
|---|---|
| +0x00 | vtable (retail RVA 0x75C32F0) |
| +0x08 | u32 frames - 1 |
| +0x0C | f32 fps |
| +0x10 | u8 animation field 3 |
| +0x18 | u8 mode flag |
| +0x1C | f32 mode rate (1.0 default) |
| +0x20, +0x50 | two EASTL hash maps (0x30 each), empty: buckets = static sentinel (RVA 0x959ABD0), count 1, size 0, load 1.0, growth 2.0 |
| +0x80 | EASTL rbtree: group type -> bone tree -> track tree |
| +0xB0 | pointer to the PANM sample block (inside the PANM) |
| +0xB8..+0xD0 | animation fields 6..9 as u64 |
| +0xD8 | data base used by samplers (= +0xB0) |
| +0xE0, +0xE8 | data owner; 0 |

Tree nodes: group 0x58 (key u8 at +0x20, bone tree anchor at +0x28), bone 0x68 (EASTL string name at +0x20, track
tree anchor at +0x38), track 0x68 (string at +0x20, entry at +0x38: u8 type, u8 encoding, u32 samples at +0x3C,
u32 flags at +0x40, u32 offset at +0x44, u32 size at +0x48). Track flags: bit 0..3 = option fields 1..4, bit 7 =
option field 8. Layout notes: `.docs/retail-t8-compiled-anim-re.md`.

## Player struct and round registry

These are the runtime structures the tool reads to find players (`RetailT8Runtime::Players`). Values from
`game_addresses.ini` are listed in [t8-behaviour.md](t8-behaviour.md#game_addressesini-t8-retail).

| Where | Offset | Meaning |
|---|---|---|
| module + registry_rva | 0 | registry pointer |
| registry | +0x10 | round map |
| round map | +0x100 / +0x110 / +0x128 | list head / buckets / mask (keys are not hashed: key & mask) |
| module + round_key_rva | 0 | u32 round key |
| round node | +0x18 | round object; round +0x00 = player list |
| player list | +0x08 / +0x10 | begin / end of entry pointers (at most 64) |
| entry | +0x90 | player |
| player | +0x04 | u32 slot (0..3) |
| player | +0x168 | u32 character id |
| player | +0x16C | u32 secondary identity |
| player | +0x390 | current move frame (timer) |
| player | +0x3E0 | current move pointer |
| player | +0x28F0 | queued next move pointer |
| player | +0x290C | queued next move id |
| player | +0x3B50 | moveset root |
| player | +0x3B58..+0x3B70 | four context roots |

Player slots are separate allocations; there is no fixed player stride.

## CNT/CBT test builds

The Closed Network Test layout is in `TKMovesets2/GameSpecific/Structures/Structs_t8cnt.h` (namespaces `StructsT8CNT`
and `StructsT8CNT_gameAddr`) and is read by `ExtractorT8CNT` (`Extractor_t8/Extractor_t8cnt.cpp`). It is a different
format and is never used for retail files. Known differences:

| Item | CNT | Retail |
|---|---|---|
| Header | 0x310 bytes (`MovesetInfo`) | 0x318 bytes |
| Aliases | 57 + 57 u16, then 38 unknown u16 | 60 + 60 + 32 u16 |
| Tables | 19 pointer/count pairs; last is `UnknownNew` (0x18) | 20 pairs; last is dialogues (0x18) |
| Move | 0x380 bytes, unencrypted vuln/hitlevel, hitboxes at 0xB8 (0x2C each) | 0x448 bytes, six encrypted fields, hitboxes at 0x160 (0x30 each) |
| Projectile | different size | 0xE0 |
| Other | MOTA lists, displayable movelist structs (`Mvl*`) | no MOTA; training movelist not handled |

The CBT build has no extractor, importer or editor in this tool (only an online handler entry in
`GameSpecific/Games.cpp`).
