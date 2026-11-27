# T7 <-> retail T8 conversion

This page describes the two moveset converters between Tekken 7 and retail Tekken 8 (Steam build 26155785):

- **T7 -> retail T8:** `T7ToRetailT8::ConvertStandalone` in `TKMovesets2/GameSpecific/MovesetConverters/T7ToRetailT8Standalone.cpp`.
- **Retail T8 -> T7:** `T8ToT7::Convert` in `TKMovesets2/GameSpecific/MovesetConverters/T8ToT7.cpp`.

Both converters are self-contained. They read the source file and the tables shipped with the tool, and nothing else: no donor
moveset, no file from a character folder, no running game. The table-shaped rules (ID maps, commands, alias slots,
characters, parameter transforms, fallbacks) come from the conversion rules described in [conversion-rules.md](conversion-rules.md).

An older strict converter, `T7ToRetailT8::Convert`, is described at the end of this page.

## Entry points

| Route | T7 -> T8 | T8 -> T7 |
| --- | --- | --- |
| GUI, Edition menu, Convert | `Submenu_Edition::RenderConversionPopup` starts a `FileJob` (`FileJobKind::ConvertToT8`), which calls `MovesetFile::ConvertToT8` | Same popup, `FileJobKind::ConvertToT7`, which calls `MovesetFile::ConvertToT7` |
| GUI, Import menu | The retail T8 profile accepts T7 files (`supportedImports = { GameId_T7 }` in `Games.cpp`). `ImporterT8::ImportMovesetData` calls `ImporterT8::ConvertT7`, which calls `ConvertStandalone` | The T7 profile accepts T8 files. `ImporterT7::_Import_FromT8` (`Importer_t7_OtherGames.cpp`) calls `T8ToT7::Convert`, then imports the result with `_Import_FromT7`. CNT/CBT files are refused |
| Headless | `convert-t8-full T7_INPUT OUTPUT [CODEC] [REPORT.json]` (with report), `convert-t8 INPUT OUTPUT [CODEC]` (through `MovesetFile::ConvertToT8`, no report), `import-t8` / `check-import-t8` with a T7 file (through `ImporterT8::ConvertT7`) | `convert-t7 T8_INPUT OUTPUT [CODEC] [REPORT.json]` (with report), `convert INPUT OUTPUT [CODEC]` on a retail file (through `MovesetFile::ConvertToT7`, no report), `import FILE t7 SLOT` with a retail T8 file |

Notes:

- GUI Convert (`RunFileJob`, `LocalStorage/FileJob.cpp`) keeps the source file's compression unless a codec is given. The
  output path comes from `ConvertedMovesetPath` (`MovesetFileConverter.cpp`): the source file name plus ` (T8)` or ` (T7)`,
  with ` (2)`, ` (3)`... added when that file exists. The output must differ from the source.
- `CanConvertMoveset` (`MovesetFileConverter.cpp`) decides which targets the Convert popup shows: T8 only for T7 sources
  and only the retail profile; T7 for TTT2, TREV, T6, T5 and T8 sources.
- Every route passes the source file's import profile (`ImportProfile::Read`) to the converter. The profile supplies
  `setting(...)` values for the rules and travels to the output (see "Character identity").
- `MovesetFile::ConvertToT8` and `MovesetFile::ConvertToT7` validate the source first (`MovesetFile::Validate`).

## T7 -> retail T8 (`ConvertStandalone`)

### Input checks

- The header must be a T7 header (`GameId_T7`), else `std::invalid_argument`.
- The payload must pass `T7ImportUtils::ValidateT7Semantics`.
- The moveset must have at least one move.

### Output container

- A `T8RS` payload (`RetailT8Snapshot`), version 1, or version 2 when converted animations are embedded (`T8AN` asset
  directory). Records use a file-local virtual address space based at `0x10000000`.
- The outer header is a copy of the source header with: `version_string` = current, `characterId` = 124 (placeholder),
  `origin` = "Tekken 8", `conversion_origin` = `GameVersions::T8::FROM_T7`, the import profile as properties, and a names
  section (`RetailT8Snapshot::NamesSection`) holding every T7 move and animation name under its retail name hash.
- Before returning, the result is checked with `RetailT8Import::Prepare`. An invalid reference graph throws.

### Source header

| Field | Value |
| --- | --- |
| `+0x00` | `0x00010000` (skip-animation-lookup 0, populated 1) |
| `compile_date` | `0x0134d94f`, the value shared by every native export of build 26155785 |
| `signature` | `TEK` |
| String pointers, `_0x170` | Placeholder (`Base`). The importer rebinds them from the live root |
| `previous_identity`, `identity` | `code(123)`, `code(124)` with `code(n) = n << 16 \| (0x10000 - n)` |
| Table pointers and counts | The converted tables |
| Original / current aliases | All 60 slots first get T7 alias 1 (the idle). Then T7 slots 0..55 are written at `AliasSlot(slot)` (`map:t7_t8_alias_slot`: retail inserted slots 30..32 and 58) |
| Unknown aliases | The first 32 of T7's 36 (`0x120..0x15F`). The identity words at `0x160..0x167` are not overwritten |
| Runtime slots | Null |

### Tables

Retail table numbers follow `RetailT8Snapshot::Tables`.

| # | Table | What it becomes |
| --- | --- | --- |
| 0 | Reactions | T7 row copied. 7 pushback pointers rewritten. `downed_rotation` (`+0x4E`) set to 0. The 14 move ids from `+0x50` go through `MoveId` |
| 1 | Requirements | One row per T7 row (see "Requirement rows"), plus one extra row `[1100]` used as the unconditional guard |
| 2 | Hit conditions | T7 row copied. Requirement and reaction pointers rewritten |
| 3 | Projectiles | T7 0xA8 row -> retail 0xE0 row at the same index. `0x00..0x37` copied; vfx through `map:t7_t8_vfx` (62 -> 2081, 66 -> 2083); hit level `0x38 -> 0x3C`; voiceclip `0x54 -> 0x68` as `{category, 0, id}`; hit condition `0x60 -> 0x90`; cancel `0x68 -> 0x98`; `0x74 -> 0xA4`; `0xA8` = 1; `0x78 -> 0xAC` only for vfx 0 and 62; `0x80 -> 0xB8`; `0x84 -> 0xBC`; `0x94 -> 0xCC`. T7 `0x7C`, `0x44`, `0x98` are not mapped |
| 4, 5 | Pushbacks, pushback extra | Copied. Pushback extra pointer rewritten |
| 6, 7 | Cancels, group cancels | Copied, then: command through `Command()`; walk-out retime; requirement and extra pointers rewritten; move id through `MoveId` except for group cancel commands (`0x8012`) |
| 8 | Cancel extra | Copied |
| 9, 10, 11 | Extra, start, end properties | Rebuilt per list head (see "Property rows"). Rows without a retail id are left out |
| 12 | Moves | Built from a zeroed record (see "Move records") |
| 13 | Voiceclips | Rebuilt per list head. T7 `category << 24 \| id` -> `{category, 0, id}`; categories in `map:t7_to_t8_voice_fixed_id` get a fixed id (category 2 -> 39302). An unterminated or empty list becomes null |
| 14, 15 | Input sequences, inputs | Copied. Input pointer rewritten |
| 16 | Parry-related | Copied |
| 17 | Cameras | Copied |
| 18 | Throw cameras | Side copied, camera pointer rewritten |
| 19 | Dialogues | Empty |

### Cancel commands

`Command()` in `T7ToRetailT8Standalone.cpp`:

- Commands with button bits (`command >> 32`): T7 Rage Art bit 4 moves to retail bit 6 (retail uses bit 4 for Heat).
  Other bits are copied.
- Special commands (`0x8000` and up) go through `map:t7_t8_cmd`: `0x8000..0x8002` unchanged, `0x8003..0x800A` +2,
  `0x800B..` -> `0x8012..` (group cancel, group end, input sequences).
- Other commands go through `map:t7_t8_walk`: `0x410 -> 0x10`, `0x440 -> 0x40`. Unmapped commands are copied.
- Walk-out cancels (a `t7_t8_walk` command) whose window start and starting frame are both
  `val:t7_t8_walk_cancel_t7_frame` (1) get `val:t7_t8_walk_cancel_t8_frame` (5). Counted as `walk_cancels_retimed`.

### Move ids

`MoveId`: an id in `0x8000..0x8037` (T7 alias slots 0..55) becomes `0x8000 + AliasSlot(id - 0x8000)`. Other ids are rows
and stay unchanged (T7 rows keep their index).

### Move records

Each move is built from scratch (no template record):

| Retail field | Source |
| --- | --- |
| Fields listed in `field:t7_to_t8_move` | The rule value (most common native value) |
| `name_key` | Encrypted retail hash of the T7 move name (`RetailNames::Hash`) |
| `anim_key` | Encrypted hash of the T7 animation name |
| `vuln`, `hit_level` | Encrypted T7 values |
| `character_id` | Encrypted `code(123)` (placeholder identity minus 1) |
| `move_key` | Encrypted hash of `move name + "#t7"` |
| `name_addr`, `anim_name_addr` | Placeholder (`Base`) |
| `anim_addr` | Handle of the converted animation, `0xEF00000000000000 \| 124 << 40 \| instance`; the idle's handle when the animation did not convert; `1` when the idle's did not convert either |
| Cancels 1..4, cancel gates 2..4 | T7 cancel pointers and gates |
| `transition` | `MoveId(T7 transition)` |
| `end_rotation` | T7 `_0x56` |
| Hit conditions, voiceclip, extra/start/end property lists | Rewritten pointers |
| Animation length, airborne start/end, ground fall, active start/end, `_0x150` (T7 `_0x98`), collision, distance | T7 values |
| Hitboxes | See below |

Encrypted fields (`SetEncodedField`) are 0x20-byte blocks: value, key, and four u32 check slots set to `0x765 + row`.
Rows with `row % 8 >= 4` keep a plain copy of the value in slot `row % 8 - 4`. The key is a fixed function of row and
field offset, so the bytes are deterministic.

Hitboxes: the T7 dword packs two halves. When the low half is a bare bone (`low != 0`, `low < 0x100`) and the high half
has a size byte, both halves are used as they are (`packed_hitboxes` in the report). Otherwise each half is cut to its
low byte (bone only). Up to two hitbox slots are written, each with the move's active frames; a zero or duplicate second
bone is skipped.

### Requirement rows

For each T7 requirement row, the first rule that applies:

1. **Import profile:** `ImportProfile::RequirementOverride` decides it -> always-true (id 0) or never-true.
2. **Condition rule** `t7_to_t8_req:ID` -> always-true or never-true.
3. **Parameter transform** `t7_to_t8_prop:ID` or a current-alias block (`map:t7_t8_prop_alias_block`) -> retail id and
   parameters.
4. **ID mapping** (`mapRequirement`), in this order:
   - `map:t7_t8_req` (881 -> 1100, 558/559 -> 667/668, overrides; `none` leaves the id unresolved);
   - alias catalog rows with confidence `static` (`RetailT8Aliases.inc`);
   - pooled cross-character votes (`T7ToRetailT8Votes.inc`, 227 rows, generated by `tools/generate_t7_t8_cross_votes.py`);
   - any other alias catalog row with a target.
   A target in `list:t7_t8_drop_t8_props` (for ids `0x8000` and up) counts as unresolved. The first parameter is kept.
5. **Fallbacks** for an unresolved id:
   - `0x8000` and up (a property call inside a requirement list): row id 0, a no-op (`unresolved_properties_dropped`).
   - Above `val:t7_to_t8_req_highest_case` (875) or in `list:t7_to_t8_req_known_false`: never-true.
   - Parameter 0, `val:t7_t8_req_unknown_zero_params_true` set, not in `list:t7_to_t8_req_default_false`: always-true
     (`unresolved_assumed_true`).
   - Anything else: never-true.

Never-true is `{val:t7_to_t8_req_never_true, val:t7_to_t8_req_never_true_param}` = `{220, 0x88}` (an identity check on
an unimplemented selector slot).

### Property rows

Extra properties (table 9), for each T7 row until id 0:

- Id 0 ends the list. The retail end row is `{frame 0, id 0}`.
- `ImportProfile::StripsProperty` -> row left out.
- Projectile spawn (`map:t7_t8_projectile_prop`, T7 33291 -> retail `0x827B`): kept when the value is a valid projectile
  row (the table is converted row for row), else dropped.
- Throw (T7 33852 -> retail `0x868F`, Rage Art 33853 -> `0x8690`): the value selects a table-18 row. Its table-17
  variants keep their weights, side and rotation. Presets (`0x16`) keep their shared IDs. Animated variants (`0x15`)
  use the embedded category-4 clips from the T7 camera MOTAs. A missing variant drops the call during conversion, or
  silences it at import if either custom camera could not be published. Import remaps the IDs of both sides after collisions.
- Hand animation (T7 `0x842E` left / `0x842F` right -> retail `0x8610` / `0x8611`, `T7HandToRetailT8::HandProperties`):
  kept when its animation converted (see [Animations](#animations)), with the animation's retail hand index as value;
  else dropped, as before.
- Camera animation (`T7CameraToRetailT8::CameraProperties`: T7 33849, 33850, 33861..33865 -> their alias-catalog
  targets `0x868C`, `0x868D`, `0x8698..0x869C`): kept the same way, with the retail camera index as value. T7 33845
  (either side) becomes two rows, `0x868C` + `0x868D`: its catalog target `0x8688` ignores its value in retail, and in
  game a converted `0x8688` row flew the camera across the stage and left it there.
  A list that plays a converted camera drops its T7 drama camera rows (33443 type, 33444 number; catalog targets
  `0x8313` SetDramaType, `0x8314` SetDramaNo): in retail the drama camera replaces the camera animation, so Kazuya's
  intros 2155/2157/2236 and win poses 2323..2331 showed the sky, the floor or the empty stage. Drama rows of lists
  without a camera animation are kept.
- Wing animation (`T7WingToRetailT8::WingProperties`: T7 33840 "Play Wing Animations" -> `0x8683`): kept the same way,
  with the retail wing (swing) index as value. Only T7 Kazuya uses it (19 rows).
- Face animation (`T7FaceToRetailT8::FaceProperties`: T7 33986 "Play Facial Animation Sequence" -> `0x877A`): kept the
  same way, with the retail face index as value (639 rows over 48 characters). "Facial Expression" (33985) takes an
  expression code, not an animation, and is unchanged.
- `t7_to_t8_prop` transform or alias block -> retail id and parameters.
- Otherwise `mapProperty`: `map:t7_t8_prop`, then static catalog rows, cross votes, other catalog rows. A target in
  `list:t7_t8_drop_t8_props` (hand, face, wing, camera asset properties) is dropped.
- Every retail extra property points to the unconditional `[1100]` requirement row.

Start and end properties (tables 10, 11): id 881 ends the list and becomes 1100. Other rows go through the transform and
`mapProperty` as above. Their T7 requirement pointer is kept (the guard row when null).

Lists are cached by head row and bounded to 4096 rows per list. Property lists over 64 MiB throw.

### Animations

- Every distinct T7 body animation (by animation offset) is decoded (`RetailPanm::DecodeT7`) and converted onto the
  built-in skeleton (`T7AnimToRetailT8::Convert` in `T7AnimToRetailT8.cpp`, skeleton in `T7AnimToRetailT8Skeleton.inc`:
  43 retail body bones, track options and frame-0 pose of a native idle, generated with `--headless panm-skeleton-inc`).
- The retargeting is closed-form: 0x64 KEF triples become ZYX Euler angles, each retail bone is a constant frame change
  of one T7 channel, T7 UpperBody is split over Spine1/Spine2. The result is written with `RetailPanm::Encode`.
- Each converted animation gets its own instance, starting at `0x00010000`, and is embedded in the `T8AN` directory with
  address `T7AnimToRetailT8::CustomAssetAddress`.
- Checks before decoding: the layout must hold at least the 23 T7 body channels and at most `MaxConvertibleFrames()`
  frames. Each attempt is charged to a 1 GiB budget at the size its length would produce, and stays charged when it
  fails. Exceeding the budget throws `std::length_error` (the whole conversion fails).
- An animation that fails to convert is logged and counted (`conversion_failed`); its moves use the idle's animation
  (`fallback_idle`).
- Hand animations (`T7HandToRetailT8.cpp`): every (hand, T7 hand animation) an extra-property row plays is converted
  once. The T7 value indexes hand MOTA slot 2, or slot 3 with bit `0x4000`; the hand comes from the property (retail keeps
  a left and a right copy of the same T7 animation). Each T7 channel is one finger joint (19 channels, or 15 without the
  metacarpals); the retail rotation is the inverse of the T7 0x64 axis-angle rotation, mirrored (x and y negated) for the
  left hand; translations keep the native rest offsets (`T7HandToRetailT8Skeleton.inc`, generated with
  `--headless panm-skeleton-inc` from two native T8 Kazuya hand PANMs). Against native T8 Kazuya, 46 right and 50 left
  same-length pairs match within 1e-4 rad on average (int16 quantization). Converted hand animations get retail hand
  indices from `0x2000` (native lists hold a few hundred; retail masks bits 14 and 15 out of the index) and are embedded
  as category 1 with instance `0x00020000 | index`. One that does not convert is counted (`animations.hands.failed`) and
  its rows stay dropped. Start and end lists are not covered: no T7 character plays a hand animation there.
- Camera animations (`T7CameraToRetailT8.cpp`): every T7 camera an extra-property row plays (slot 8, or slot 9 with bit
  `0x4000`; all 0xC8 clips) is converted once:
  - position: the first `0x0B` channel, or the second in look-at clips (the first is the target), / 10;
  - rotation: the `0x07` channel as ZYX Euler (`Rz * Ry * Rx`), or a look-at (local -Z towards the target, world up +Y);
  - roll: the first `0x15` channel, in degrees, about world Z before the rotation (`Rz(roll) * q`); further `0x15`
    channels are ignored; quaternion signs are kept continuous;
  - FOV: the `0x0F` channel x 9/16; smoothing `0x03` -> a `Camera_Param` track's scale (x animated, y and z at their
    first value, the native shape);
  - written in the native camera layout (`RetailPanm::EncodeGroups`: group 1 `cam_transform` Transform and
    `Camera_Param`, group 5 `cam_transform` FieldOfView; packed codec `PackTransforms` / `PackScalars`, a constant FOV
    static). Every native camera PANM re-encodes byte for byte except 2 of 395 with a layout no converter writes.
  Against native T8 cameras of the same length: 176 pairs within 0.006 cm, 2e-4 rad and 3e-4 FOV, roll included. Indices
  from `0x2000`, category 4, instance `0x00040000 | index`; report `animations.cameras`.
- Wing animations (`T7WingToRetailT8.cpp`): every T7 wing animation an extra-property row plays (slot 6, or slot 7 with
  bit `0x4000`) is converted once. Each bone is a channel group (`0x0B` position, `0x07` rotation, optional `0x03`
  scale): rotation = the inverse of the 0x64 axis-angle rotation, position / 10, scale copied. The bone names come from
  the built-in rigs (`T7WingToRetailT8Rigs.inc`, from native T8 swing PANMs of the shared archive; T8 Kazuya's own
  swing list plays 17 of that kind): the rig with the animation's bone count whose rest offsets its frame-0 positions
  match within 0.05 cm. Written as raw tracks without options, like native raw swing PANMs; category 3, instance
  `0x00030000 | index`; report `animations.wings`. Against native T8 swing PANMs: all 34 T7 wing animations that get a
  rig match one, within 0.034 rad (0.001 rad on all longer than 30 frames), positions exact. T7 Kazuya plays 8: 7
  convert; #7 (a larger wing, bones 20 cm longer) matches no rig and its row is dropped. The 10-bone `OPT600` rig is left
  out (no T7 property plays it, and one of its bone names is only inferred).
- Face animations (`T7FaceToRetailT8.cpp`): every T7 face animation an extra-property row plays (slot 4, or slot 5 with
  bit `0x4000`) is converted once onto the legacy 43-bone `*_Joint` face rig. Channels 2k / 2k+1 are bone k's position
  and rotation (the four trailing scale channels are 1.0 everywhere and dropped); rotation = the inverse of the 0x64
  axis-angle rotation, position / 10. Written like native face PANMs: one group, packed tracks (static for a bone that
  never moves), options {3, 1}, {4, 1}. Against the 10 native T8 files of this rig with a T7 source (com x3, hms x6 =
  T7 Emilie 5#11..16, test x1): 43 of 43 bones, mean 1e-4 rad, worst 0.018 rad and 0.006 cm. Category 2, instance
  `0x00050000 | index`; report `animations.faces`. Over 51 characters 559 face animations convert; Negan's 10 played
  faces (116 channels) do not, and their 10 rows stay dropped.
  **Caveat:** 5,940 of the 5,969 T8 face PANMs (T8 Kazuya: 165 of 166) use the 185/186-bone T8 face rig, which shares 8
  bone names with the legacy one at other rest positions; on a character whose model has only that rig the converted
  faces may not move anything ([.docs/t7-to-retail-t8-face-wings.md](../.docs/t7-to-retail-t8-face-wings.md)).
- Converted files normally play through the memory-only import. When the target's animation map is not ready,
  `ImporterT8` imports them through the target fallback instead (moves play the target's idle animation; see
  [t8-behaviour.md](t8-behaviour.md)).

### Character identity

- Output `characterId` is the placeholder `T7ToRetailT8::ConvertedCharacter` = 124 (unused by retail characters).
- Requirement rows whose mapped retail id is 220..227 (identity checks) with a parameter below `0x81`: the moveset's own
  T7 character becomes 124; other characters go through `map:t7_t8_character`; a character with no counterpart becomes
  `0xFE`. The high 16 bits are kept.
- At import (`ImporterT8::ImportMovesetData`):
  - `RetailT8Import::AdaptCharacterRequirements` swaps the source identity (124, or the profile's fixed identity) with
    the target's in identity checks 220, 221, 224, 225, 499, 500, 503, 504.
  - `RetailT8Import::ApplyConvertedIdentity` writes the target's identity codes: header `+0x160 = code(id - 1)`,
    `+0x164 = code(id)`, every move's encrypted `+0xD0 = code(id - 1)` (and its plain check copy).
- Import profile: a fixed identity is moved to T8 numbering with `map:t7_t8_character`; without a counterpart the profile
  falls back to "adopt target".

### Report

`T7ToRetailT8::ConversionReport`, written as JSON by `ConversionReportJson`. Fields that the standalone converter never
sets stay 0: `paired_moves`, `animations.by_move_name`, `animations.by_animation_name`, `animations.carriers_available`,
`animations.embedded_native`, `voiceclips.dropped`. Main fields:

| Field | Meaning |
| --- | --- |
| `moves`, `source_character`, `converted_character` | Move count, T7 header character, 124 |
| `animations.converted_from_t7`, `fallback_idle`, `conversion_failed`, `custom_animation_bytes`, `converted_moves` | Animation results |
| `animations.hands.converted`, `failed`, `property_rows` | Hand animations converted, not converted, and hand rows kept |
| `animations.cameras.converted`, `failed`, `property_rows` | The same for camera animations |
| `animations.wings.converted`, `failed`, `property_rows` | The same for wing animations |
| `animations.faces.converted`, `failed`, `property_rows` | The same for face animations |
| `walk_cancels_retimed`, `packed_hitboxes`, `voiceclips.converted` | Record counters |
| `requirements.catalog`, `cross_character_votes`, `unresolved`, `unresolved_assumed_true`, `unresolved_properties_dropped`, `unresolved_ids` | Requirement id resolution (`unresolved_ids`: `{id, rows}`) |
| `properties.catalog`, `cross_character_votes`, `dropped`, `dropped_ids` | Property id resolution |
| `projectile_spawns`, `projectile_spawns_dropped`, `throw_cameras_dropped` | Projectile and throw rows |

### What is dropped or rejected

Dropped (conversion continues):

- Property rows with no retail id, rows the import profile strips, rows a transform drops (`miss = drop`).
- Hand, face, camera or wing animations whose animation did not convert, and the other asset properties of
  `list:t7_t8_drop_t8_props`.
- Projectile spawns and throws whose row is out of range.
- Property calls (`0x8000`+) inside requirement lists without a retail id.
- MOTA archives themselves (only the animations properties play are converted), the training movelist and dialogues.
- T7 projectile fields `0x7C`, `0x44`, `0x98`.
- T7 unknown aliases 32..35.

Rejected (the conversion throws):

- Non-T7 input, input failing T7 semantic validation, a moveset with no moves.
- Unterminated property lists, references outside a table.
- Property lists over 64 MiB, any table over 512 MiB, converted animations over 1 GiB, out of memory.
- A converted file whose reference graph fails `RetailT8Import::Prepare`.

### Live edition (`LiveConversion`)

Editing a T7 moveset that was imported into retail T8 does not convert the whole moveset again. `T7ToRetailT8::LiveConversion`
runs the full conversion once (its file is the bytes `ConvertStandalone` returns) and keeps what the row rules read: the
table layout, the rebuilt property-list and voiceclip starts, the converted animation of each T7 animation offset and
the move names. `UpdateRow` then converts one edited T7 row with the same rules and writes its records into the file:

- Tables converted row for row (reactions, requirements, hit conditions, projectiles, pushbacks, cancels, group
  cancels, extradata, input sequences, inputs, parry, cameras, throws): the T8 row of the same index.
- Property lists (9, 10, 11) and voiceclips (13): the output rows the T7 row became (a row walked from two list heads
  is written twice). A row no move reaches changes nothing.
- Moves: the row, as long as its names, animation and list starts stay as converted.

The edits a single row cannot carry return a reason and change nothing: a property that gains or loses a retail
equivalent, a moved list end, a move pointing at a list no move used, a renamed move or a new animation. They need a
new import. `MovesetRegressionTests --check-t7-live FILE...` checks on real files that single-row updates give the
bytes of a full conversion of the edited moveset.

In the editor (`Editor_t7_LiveRetail.cpp`), the T7 live writers write into `T7LiveShadow`, a copy of the T7 moveset in
file layout, instead of the game; the edited rows are converted with `UpdateRow` and written into the published moveset
by `RetailLiveEdit::Sync`, which applies the importer's identity, profile and animation fixups.

## Retail T8 -> T7 (`T8ToT7::Convert`)

### Input checks

- Header must be `GameId_T8` with minor version `RETAIL`, else `std::invalid_argument`. CNT/CBT files are refused.
- The payload must pass `RetailT8Snapshot::Validate`.
- At least one move.

### Output

- A complete uncompressed T7 file in the T7 exporter's layout (`T7Export::Serialize`): moveset info, names, records,
  animations, no MOTA block, empty movelist block.
- Header: `gameId` T7, `conversion_origin` = `GameVersions::T7::FROM_T8`, `characterId` = the converted identity,
  `origin` = "Tekken 7", target and original character names, source dates, the import profile as properties. Header
  padding is zeroed so one input always gives the same bytes.
- The result must pass `T7ImportUtils::ValidateT7Semantics`, else the conversion throws.

### Tables

| T7 table | What it becomes |
| --- | --- |
| Reactions | Same layout. Pushback pointers become rows. The 15 move ids from `+0x4E` go through `moveId` (below) |
| Requirements | Row for row (see "Requirement rows"), then a never-true row, an 881 row, and guarded copies (`t8_to_t7_guard`) |
| Hit conditions | Same layout, pointers become rows |
| Projectiles | Retail 0xE0 -> T7 0xA8, the inverse of the forward layout. vfx 2081/2083 -> 62/66. Voiceclip `{category, 0, id}` -> `category << 24 \| id` unless the category is in `list:t8_to_t7_voice_drop`. Then `t8_to_t7_fix:projectile` fixes |
| Pushbacks, pushback extra, cancel extra, input sequences, inputs, parry, cameras, throw cameras | Same layouts, pointers become rows. Input sequences with input count 0 get a null input pointer |
| Cancels, group cancels | See "Cancels". v4 files: external cancel lists captured with the moveset are appended to the cancel table |
| Extra, start, end properties | Rebuilt per list head; rows with no T7 id are left out |
| Moves | See "Move records" |
| Voiceclips | Rebuilt per list head. Categories above `0xFF` or in `list:t8_to_t7_voice_drop` (2) are dropped. `t8_to_t7_fix:voice` fixes apply |

### Requirement rows

For each retail requirement row (4 parameters), the first rule that applies:

1. Import profile override -> always-true (`{0, 0}`) or never-true.
2. Condition rule `t8_to_t7_req:ID` -> always-true or never-true.
3. Parameter transform `t8_to_t7_prop:ID` (it may compute the T7 id with `id = EXPR`) or an alias block.
4. ID mapping (`mapRequirement`): `map:t7_t8_req` (1100 -> 881, 667/668 -> 558/559); ids `0x8000`+ in
   `list:t7_t8_drop_t8_props` are unresolved; then the alias catalog read backwards with confidence `static`, then the
   pooled votes (`T8ToT7Votes.inc`, 227 rows), then the best catalog row (rank static, handler, strong, interp, weak;
   lowest T7 id on ties). The first parameter is kept.
5. Fallbacks: `0x8000`+ -> `{0, 0}` (no-op); all parameters 0 with `val:t7_t8_req_unknown_zero_params_true` -> `{0, 0}`
   (assumed true, unless a transform dropped the row); otherwise never-true `{val:t8_to_t7_req_never_true, 0}` = `{901, 0}`.
6. `t8_to_t7_fix:req` on the result (a drop makes it never-true).

Identity checks: retail ids 220..227 with a first parameter below `0x81`: the file's own character becomes the converted
T7 identity, others go through `map:t7_t8_character`, `0xFE` when none.

### Property rows

- Extra list end: id 0. Start/end list end: id 1100 -> 881.
- Import profile strips -> left out.
- Extra properties only: projectile spawn `0x827B` -> 33291 and throw `0x868F` -> 33852 when the row is valid, else dropped.
- `t8_to_t7_prop` transform or alias block; otherwise `mapProperty`: drop list first, `map:t7_t8_prop`, then the catalog
  and votes as for requirements. The T7 value is the first retail parameter.
- `t8_to_t7_fix:prop` on the result.
- T7 extra properties have no requirement list. A retail extra property whose requirement list holds more than its 1100
  end is written anyway and fires unconditionally; counted as `properties.extra_requirements_dropped`.
- Start/end properties keep their requirement pointer as a row.

### Cancels

`convertCancel` in `T8ToT7.cpp`:

- Commands with button bits or below `0x8000` are copied (button bits unchanged; there is no inverse of the forward Rage
  Art bit move).
- Special commands go through `map:t7_t8_cmd` backwards: `0x8000..0x8002` unchanged, `0x8005/0x8006 -> 0x8003/0x8004`,
  `0x8012.. -> 0x800B..`. The forward `0x8003..0x800A +2` line is one-way, so retail `0x8007..0x800C` have no T7 command.
- A special command with no rule keeps its row behind the never-true requirement row (`special_cancels_disabled`).
  Its raw value is kept when below `0x800B`, else it becomes 0.
- Walk-out cancels: retail `0x10/0x40` opening on frame 5 (window start and starting frame) -> `0x410/0x440` on frame 1.
- Move id through `moveId` except for group cancels; `t8_to_t7_fix:cancel` on the T7 command.
- Command-0 cancels whose requirement list has no condition get a guarded copy when a `t8_to_t7_guard` rule matches.
- Cancel option bits (`+38`) are recomputed with T7's loader rules (same code as `T7DiskMoveset.cpp`).

### Move ids

`moveId`: retail alias ids `0x8000..0x803B` go back through `map:t7_t8_alias_slot`. A retail-only slot (30..32, 58)
becomes the row the retail current alias names. Other ids are rows.

### Move records

| T7 field | Source |
| --- | --- |
| Name, animation name | v4 extension strings; else the hash resolved from the file's editor names, its names section, then the shipped `t8-retail` name table; else `move_XXXXXXXX` / `anim_XXXXXXXX` |
| `anim_addr` | The converted animation; else the fallback (below) |
| `vuln`, `hitlevel` | Decrypted `0x58`, `0x78` |
| Cancel pointers and gates | `0x98..0xC8`. Secondary banks only when their gate is at least 1 |
| `transition`, `_0x56` | `moveId(0xCC)`, `0xCE` |
| `moveId_val1/val2` | `code(ownT7 - 1) + row` |
| Hit condition, voiceclip, property lists | Pointers as rows |
| Animation length, airborne, ground fall | `0x120..0x12C` |
| `_0x98` | `0x150` |
| `hitbox_location` | `(u32 at 0x198) << 16 \| (u32 at 0x168 & 0xFFFF)` (first two retail hitbox bones) |
| Active frames | `0x158`, `0x15C` |
| `_0xA8`, `distance` | `0x2E0`, `0x2E2` |

After all moves are built, `t8_to_t7_fix:move` rules rewrite u32 fields of every move.

Aliases: T7 slots 0..55 take retail slot `AliasSlot(slot)`. Unknown aliases: retail `0x120..0x15F` fill T7 slots 0..31;
32..35 stay 0.

### Animations

- Every embedded body PANM (asset type 0) is converted once per instance: `RetailPanm::Decode` (raw, static and packed
  tracks), `RetailT8AnimToT7::ToChannels`, then `Write64` (T7 0x64). When 0x64 cannot hold it, `WriteC8` (0xC8).
- Moves whose PANM is missing (`animations.missing`) or fails (`conversion_failed`) use the idle's animation; without one,
  the first converted animation; without any, a one-frame rest pose (`fallback_moves`).
- Budgets: the per-frame channel work is charged to 1 GiB before it runs and stays charged on failure; the converted
  animation block must fit 192 MiB. Exceeding either throws.

### Character identity

- `ownT7` = `map:t7_t8_character` of the T8 `characterId`; when there is none, `val:t8_to_t7_placeholder_character`
  (120). The report sets `placeholder_character`.
- At T7 import, `ImporterT7::ApplyCharacterIDFixes` swaps identity checks (`character_id_condition` from the game
  addresses) that name the file's character for the current character.
- Import profile: a fixed identity is moved to T7 numbering; without a counterpart it becomes "adopt target".

### Report

`T8ToT7::Report`, JSON from `T8ToT7::ReportJson`: `moves`, `source_character`, `converted_character`,
`placeholder_character`, `external_cancel_lists`, `dangling_references`, `special_cancels_disabled`,
`walk_cancels_retimed`, `animations` (`converted`, `missing`, `conversion_failed`, `fallback_moves`, `bytes`),
`voiceclips` (`converted`, `dropped`), `rule_fixes` (`changed`, `dropped`, `guarded_lists`), `requirements` (`fixed`,
`cross_character_votes`, `catalog`, `unresolved`, `unresolved_assumed_true`, `unresolved_properties_dropped`,
`unresolved_ids`), `properties` (`fixed`, `cross_character_votes`, `catalog`, `dropped`, `dropped_ids`,
`extra_requirements_dropped`).

### What is dropped or rejected

Dropped: properties and requirements with no T7 id (as above), hand/face/wing/camera asset properties, voiceclip
category 2, retail special cancels without a rule (disabled), unknown aliases beyond 32, MOTAs and the training movelist
(the T7 importer keeps the target's MOTAs), dangling pointers (counted, written as null).

Rejected: non-retail input, a snapshot failing validation, no moves, unterminated lists, tables over 192 MiB, property
lists over 64 MiB, animation budgets, out of memory, an output failing T7 semantic validation.

## Determinism

- Neither converter reads anything but the source file, the built-in tables and the rules in use. Encryption keys,
  animation instances and list layouts are computed from row numbers, so there is no randomness.
- The output also depends on the import profile stored in the source file and on the rules in use: the built-in rules,
  or the rules section of a `game_addresses.ini` in the current directory when it qualifies (see
  [conversion-rules.md](conversion-rules.md)).
- T8 -> T7 determinism is checked by `tests/T8ToT7Tests.cpp` (two conversions compared byte for byte). No test in
  `tests/T7ToRetailT8Tests.cpp` compares two forward conversions; the background notes report a manual check.

## Strict converter (`T7ToRetailT8::Convert`)

`T7ToRetailT8.cpp` holds an older, strict T7 -> T8 converter and its record planner:

- `T7ToRetailT8::Build` produces a `Plan` (table inventory, references, move names, converted requirement and property
  records, per-row id alignments with confidence and provenance). It uses only the 14 explicit rules of
  `RetailT8Rules.inc` (`RetailT8::ConvertId`, `RetailT8Records.hpp`). Any unsupported record is an issue; with issues the
  four converted arrays are cleared.
- `T7ToRetailT8::Report` writes the plan as JSON (`--headless plan-convert-t8`).
- `T7ToRetailT8::Convert` builds a T8RS file only when every record converted. It rejects non-empty projectile, camera
  and throw-camera tables, voiceclip lists other than terminators, and T7 characters missing from its own built-in
  identity table. Moves get `anim_addr = 1` and native name hashes, so animations must be bound by the importer.
- Current callers: `tests/T7ToRetailT8Tests.cpp` and `tests/RetailSnapshotTests.cpp`. `--headless convert-t8` does not
  call it (it calls `MovesetFile::ConvertToT8`, which is `ConvertStandalone`).
