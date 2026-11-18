# Conversion rules file

The T7 <-> retail T8 converters ([conversion.md](conversion.md)) read their table-shaped rules from a text file instead of
code: ID maps, cancel commands, alias slots, characters, parameter transforms, requirement conditions, fallbacks and
fixes. A rule change can ship to users with a game-address update, without a new build.

Parser and loader: `TKMovesets2/GameSpecific/MovesetConverters/ConversionRules.hpp` / `ConversionRules.cpp`
(namespace `ConversionRules`).

## Files

| File | Role |
| --- | --- |
| `TKMovesets2/Rules/t7_t8_conversion_rules.ini` | The file to edit |
| `TKMovesets2/Resources/game_addresses.ini` | Holds a generated copy of the rules between `# BEGIN T7 T8 CONVERSION RULES` and `# END T7 T8 CONVERSION RULES`. This file is embedded in the exe and downloaded by the updater |

The address loader skips the section (`GameAddressesFile.cpp`, the `conversionRules` flag around line 438), so the rules
never become addresses and never enter the injected DLL's shared-memory copy of the addresses.

## Build step

`TKMovesets2/CMakeLists.txt`, block "T7 <-> T8 conversion rules" (lines 306-357). It runs at configure time, before
`create_resources` embeds `Resources/`:

1. Both files are configure dependencies (`CMAKE_CONFIGURE_DEPENDS`): editing either re-runs the merge.
2. The rules text is normalized to LF with a final newline, and hashed (SHA-256).
3. `val:global_addr_version` is read from `game_addresses.ini`. Missing -> configure error.
4. If the section does not exist, it is appended after the addresses. If it exists, the previous begin line's
   `hash=... addr_version=...` is read. When the hash changed but `addr_version` is the same, configure stops:
   "raise val:global_addr_version". The build does not check `t7_t8_rules_version`.
5. The section is rewritten as:
   `# BEGIN T7 T8 CONVERSION RULES hash=<sha256> addr_version=<N>`, a "generated" comment line, the rules text,
   `# END T7 T8 CONVERSION RULES`. The source `Resources/game_addresses.ini` is written only when its text changed.

The embedded copy is reached through `game_addresses_ini` / `game_addresses_ini_size` (`EmbeddedResources.c`).
`ConversionRules::EmbeddedText` extracts the section; `ConversionRules::Embedded` parses it.

## Which rules a run uses

`ConversionRules::Current()` decides once per process (static) in `Load()`:

- Start with the embedded rules (`source` = "built-in").
- If `game_addresses.ini` exists in the **current directory**, parse its section (or the whole file when it has no
  markers). The GUI runs with the exe directory as current directory; `--headless` keeps the caller's directory.
- The file's rules replace the built-in ones when they parse and validate, and their `t7_t8_rules_version` is not lower
  than the built-in version. Equal versions are accepted.
- Otherwise the built-in rules stay and the reason is kept in `CurrentStatus().error`.

The updater (`Updater/UpdateCheck.cpp`, `UpdateAddresses`) downloads `TKMovesets2/Resources/game_addresses.ini` from the
repository's `master` branch and installs it only when its `val:global_addr_version` is higher than the loaded one.

Other users of the rules besides the converters: `RetailT8Import::ApplyProfileConditions` (T8 never-true row) and
`ImporterT7::ApplyCharacterIDFixes` (T7 never-true id).

## `check-rules`

`TKMovesets2 --headless check-rules [FILE]` (`main/headless.cpp`):

- With FILE (a `game_addresses.ini` or the bare rules file): parse and validate it. Prints `ok`, `version`, `format`,
  counts (`values`, `map_lines`, `lists`, `tables`, `transforms`, `t8_move_fields`), `built_in_version` and
  `would_replace_built_in` (true when its version is strictly higher than the built-in one). A bad file prints
  `ok: false` with the error, which names the line, and exits 1.
- Without FILE: the rules in use, with `source` ("built-in" or `game_addresses.ini`), the counts, `built_in_version`
  and `ignored_file_error` when a local file was refused.

## Line syntax

- One rule per line. `#` starts a comment anywhere on a line. Blank lines are ignored.
- Every line is `PREFIX:REST`. Unknown prefixes are refused.
- Numbers: decimal or `0x` hex, at most 10 digits, 32-bit.
- Spans (maps, lists): a value `a`, a range `a..b`, or an open range `a..` (up to `0xFFFFFFFF`).

| Prefix | Form |
| --- | --- |
| `val:NAME = NUMBER` | A value. Each name once |
| `list:NAME = SPAN, SPAN, ...` | A set of values. Several lines with one name add up |
| `map:NAME = T7 <-> T8` / `T7 -> T8` / `T7 <- T8` | One map line. Both sides are spans; ranges map position by position and must have the same length; an open range maps only to an open range. `none` is allowed on the target side of a one-way line (`X -> none`, `none <- Y`): no counterpart in that direction |
| `table:NAME = KEY:VALUE, FIRST..LAST:VALUE, ...` | A lookup table. A range maps `FIRST+i` to `VALUE+i`. At most 65,536 keys per range, 1,048,576 keys over all tables. A key once per table |
| `field:t7_to_t8_move = OFFSET[..LAST] = VALUE` | Retail move field defaults. A range steps by 4 |
| `t7_to_t8_prop:SOURCE -> TARGET  p0 = EXPR ; p2 = EXPR [; miss = drop]` | Forward parameter transform |
| `t8_to_t7_prop:SOURCE -> TARGET  v = EXPR [; id = EXPR] [; miss = drop]` | Backward parameter transform |
| `t7_to_t8_req:ID = EXPR` / `t8_to_t7_req:ID = EXPR` | Requirement condition |
| `t8_to_t7_fix:KIND:ID  [drop = EXPR ;] [v = EXPR ;] [miss = drop]` | Fix on a T7 record the T8 -> T7 converter writes |
| `t8_to_t7_guard:ID  [when = EXPR ;] id = EXPR ; v = EXPR` | Guard for a condition-less T7 requirement list |

### Expressions

- Operands: numbers, `v`, `p0..p4`, `lookup(TABLE, EXPR)`, `setting(NAME)`, parentheses.
- Operators, from loosest: comparisons `== != < <= > >=` (one per level, not chained; result 1 or 0), `|`, `^`, `&`,
  `<< >>` (a shift of 32 or more gives 0), `+ -`. All arithmetic is unsigned 32-bit.
- `lookup` without a matching key is a miss. A miss: in a transform, the row goes through the maps unchanged, or is
  dropped with `miss = drop`; in a condition, the requirement is false; in a fix, the value is kept, or dropped with
  `miss = drop`; in a guard, no guard is added.
- `setting(NAME)` reads the moveset's import profile (`ImportProfile::ConversionSettings`). Known names and defaults
  (`ConversionRules::DefaultSettings`): `story_flag` 0, `in_heat` 0, `flags_clear` 1.
- Nesting of parentheses/lookups is limited to 32, expression tree depth to 64.

Inputs a rule may read:

| Rule | Reads |
| --- | --- |
| `t7_to_t8_prop`, `t7_to_t8_req` | `v` (T7 value) only |
| `t8_to_t7_prop`, `t8_to_t7_req` | `p0..p4` (retail parameters) only |
| `t8_to_t7_fix:prop`, `t8_to_t7_fix:req` | `v` (the T7 value about to be written) and `p0..p4` |
| Other `t8_to_t7_fix` kinds, `t8_to_t7_guard` | `v` only |

## Sections and keys

The set of `val:`, `map:` and `list:` names is fixed (`RequiredValues`, `RequiredMaps`, `OptionalMaps`,
`RequiredLists` in `ConversionRules.cpp`). A missing required name or an unknown name rejects the file. A rule for a
newer build needs a new `t7_t8_rules_format`.

### Values

| Key | Shipped | Used by |
| --- | --- | --- |
| `t7_t8_rules_version` | 6 | Version of the rule content; at least 1. Compared with the built-in version at load |
| `t7_t8_rules_format` | 1 | Syntax version; must equal `ConversionRules::SupportedFormat` (1) |
| `t7_to_t8_req_highest_case` | 875 | T7 -> T8: unresolved T7 ids above it are never-true |
| `t7_to_t8_req_never_true` | 220 | T7 -> T8 never-true row id (not 0) |
| `t7_to_t8_req_never_true_param` | `0x88` | Its parameter |
| `t8_to_t7_req_never_true` | 901 | T8 -> T7 never-true row id (not 0) |
| `t7_t8_req_unknown_zero_params_true` | 1 | Both directions: an unknown requirement with parameter(s) 0 is assumed true |
| `t8_to_t7_placeholder_character` | 120 | T8 -> T7 identity for characters with no T7 counterpart. At most `0xFF` and not a T7 id of `map:t7_t8_character` |
| `t7_t8_walk_cancel_t7_frame` | 1 | Walk-out cancel opening frame in T7 (1..32767) |
| `t7_t8_walk_cancel_t8_frame` | 5 | The same in retail (1..32767) |
| `t7_t8_prop_alias_block_size` | 33 | Length of each `map:t7_t8_prop_alias_block` block (1..64) |

### Maps

| Map | Shipped content | Used by |
| --- | --- | --- |
| `t7_t8_req` (optional name, but must map `881 <-> 1100`) | `881 <-> 1100`, `558 <-> 667`, `559 <-> 668` | Requirement ids, before transforms' fallbacks, catalog and votes. `none` makes an id unresolved |
| `t7_t8_prop` (optional) | none | Property id overrides, before catalog and votes. `none` drops the id |
| `t7_t8_projectile_prop` | `33291 <-> 0x827B` | Projectile spawn property (value = projectile row). Exactly one `<->` pair, both sides non-zero |
| `t7_t8_throw_prop` | `33852 <-> 0x868F` | Throw property (value = throw row). Same constraint |
| `t7_t8_prop_alias_block` | `0x83C4 <-> 0x850D`, `0x83E5 <-> 0x852E` | Current-alias overwrite/reset blocks: start ids of blocks of `t7_t8_prop_alias_block_size` properties; the index is an alias slot, the value an own move id |
| `t7_t8_cmd` | `0x8000..0x8002 <-> 0x8000..0x8002`, `0x8003..0x8004 <-> 0x8005..0x8006`, `0x8005..0x800A -> 0x8007..0x800C`, `0x800B.. <-> 0x8012..` | Special cancel commands (`0x8000` and up only). Must map `0x8000 <-> 0x8000`, and `0x8000` maps only to itself |
| `t7_t8_walk` | `0x410 <-> 0x10`, `0x440 <-> 0x40` | Walk-out cancel commands (values up to `0x7FFF`) |
| `t7_t8_alias_slot` | `0..29 <-> 0..29`, `30..54 <-> 33..57`, `55 <-> 59` | Alias slot numbering. Every T7 slot 0..55 needs a T8 slot; T8 values up to 59 |
| `t7_t8_character` | 17 lines, T7 id <-> T8 id (Paul..Bryan, Heihachi, Kazuya, Steve..Claudio, Shaheen, Nina, Master Raven, Lee, Bob, Kuma, Panda, Eddy, 45, 47, Zafina, Kunimitsu, Lidia) | Character identities (values up to `0xFF`). No line = no counterpart |
| `t7_to_t8_voice_fixed_id` | `2 -> 39302` | T7 voiceclip category -> fixed retail clip id (category up to `0xFF`, id up to `0xFFFFFF`) |
| `t7_t8_vfx` | `62 <-> 2081`, `66 <-> 2083` | Projectile effect ids |

Within one map, two lines may not give one source value a target in the same direction.

### Lists

| List | Shipped | Used by |
| --- | --- | --- |
| `t7_to_t8_req_known_false` | `418, 739` | T7 -> T8: unresolved ids with no T7 evaluator case below the highest case: never-true |
| `t7_to_t8_req_default_false` | `560, 563, 636` | T7 -> T8: unknown flag checks not assumed true (customization, game mode, assist) |
| `t7_t8_drop_t8_props` | `0x860F..0x8613, 0x8683..0x8685, 0x8780, 0x8688..0x86B0, 0x877A..0x877B` | Both directions: retail properties that play hand, face, wing or camera assets by id; dropped. Exception: T7 -> T8 hand, face, wing and camera animations that convert (`T7HandToRetailT8`, `T7FaceToRetailT8`, `T7WingToRetailT8`, `T7CameraToRetailT8`, see [conversion.md](conversion.md#animations)) |
| `t8_to_t7_voice_drop` | `2` | T8 -> T7: voiceclip categories with no T7 value; dropped |

### Tables

Any name; a table must exist when a `lookup` names it. Shipped: `t7_to_t8_battle_flag`, `t8_to_t7_battle_flag`,
`t8_to_t7_throw_break`, `t8_to_t7_opponent_facing`.

### Move field defaults

`field:t7_to_t8_move` lines (at least one required): u32 values written into every converted retail move before the T7
fields. Offsets must be 4-aligned and inside the 0x448-byte retail move. Shipped: `0x118`, `0x11C` and the
`0x2E4..0x42C` block (most common value over 64,293 native moves).

### Transforms

- `t7_to_t8_prop:SOURCE -> TARGET` sets retail parameters `p0..p4` (unset ones are 0) from `v`.
- `t8_to_t7_prop:SOURCE -> TARGET` must set `v`; `id = EXPR` computes the T7 id from the parameters instead of TARGET. A
  computed id of 0, 881 or 1100 drops the row.
- Transforms run on property rows and on requirement rows. A requirement row a transform drops stays never-true; a
  property row is left out.
- One transform per source id and direction. Neither source nor target may be a list end: 0, T7 881, retail 1100 (and a
  `t8_to_t7_prop` target may not be 1100).

Shipped transforms: sounds `0x84C3/0x84C4/0x84C6 <-> 0x87EF/0x87F0/0x87F2` (`category << 24 | id` <-> `{category, 0, id}`),
battle flag `0x8409 <-> 0x8552` (sub-code tables), floor level `0x83C2 <-> 0x850B`, T8 -> T7 throw break `0x87 -> 0x83/0x84`
by button, opponent facing `0x1FD -> 0x5C`.

### Requirement conditions

`t7_to_t8_req:ID = EXPR` / `t8_to_t7_req:ID = EXPR`: whether requirement ID holds in normal play. The converter writes an
always-true row (EXPR non-zero) or a never-true row. One condition per id and direction; none on 0 or the list end.
Shipped (T8 -> T7): 777 story flag (`p0 == setting(story_flag)`), 476 and 439 (`setting(flags_clear) & (p1 == 0)`), 1028
in Heat (`p0 == setting(in_heat)`).

### Fixes (`t8_to_t7_fix`)

Run on the T7 value the T8 -> T7 converter is about to write, after every map and transform. `drop = EXPR` non-zero drops
the row; `v = EXPR` replaces the value. One fix per `KIND:ID`. Counted in the report under `rule_fixes` (`changed`,
`dropped`).

| KIND | ID | `v` | Drop means |
| --- | --- | --- | --- |
| `prop` | T7 property id (not 0) | T7 value | Row left out |
| `req` | T7 requirement id (not 881) | T7 parameter | Never-true |
| `voice` | Voiceclip category (below `0xFF`) | Clip id (24 bits) | Clip left out |
| `cancel` | T7 cancel command (not `0x8000`) | Target move id | Never-true |
| `move` | u32 offset in the 0xB0-byte T7 move | Field value | Not allowed |
| `projectile` | u32 offset in the 0xA8-byte T7 projectile | Field value | Not allowed |

`move` and `projectile` offsets must be 4-aligned, inside the record, and not inside a reference field (move: `0x00`,
`0x08`, `0x10`, `0x20`, `0x28`, `0x38`, `0x48`, `0x60`, `0x78`, `0x80`, `0x88`, `0x90`; projectile: `0x60`, `0x68`).
Shipped: `t8_to_t7_fix:prop:0x8036  drop = (v >> 8) > 31` (trail effect ids past T7's table).

### Guards (`t8_to_t7_guard`)

In T7 a requirement list made only of rows 0 and property rows (`0x8000`+) is always true. When a command-0 cancel (or
group cancel) has such a list, and the list holds T7 property row ID with value `v` for which `when` holds, the cancel gets
a copy of the list with the row `{id, v}` in front. ID must be `0x8000` or higher; `id` and `v` must both be set; one guard
per id. A computed id of 0, 881 or `0x8000`+ adds nothing. Counted as `rule_fixes.guarded_lists`.
Shipped: `t8_to_t7_guard:0x8106  when = (v & 0xFFFF) == 0 ; id = 284 ; v = ((v >> 16) << 16) | 1`.

## Validation limits

Any failure rejects the whole file (`ConversionRules::Parse`, then `Validate`); the error names the line.

- Text at most 4 MiB and 100,000 lines.
- Only `881 <-> 1100` may map to a requirement list end; `map:t7_t8_req` must contain it.
- `map:t7_t8_prop` may not cover a property list end (0, T7 881, retail 1100) on a side that maps.
- `map:t7_t8_cmd` covers `0x8000` and up only.
- Map value ranges per map as listed above.
- Table key limits as listed above.

## What stays in code

Record layouts and field offsets, field encryption, animation decoding and retargeting, list terminators, the converted
T8 placeholder identity (124, `T7ToRetailT8::ConvertedCharacter`), the alias catalog (`RetailT8Aliases.inc`) and the vote
tables (`T7ToRetailT8Votes.inc`, `T8ToT7Votes.inc`).

## Tests

`tests/T8ToT7Tests.cpp`, `RunConversionRulesTests`: the embedded rules hold the expected commands, slots, characters and
transforms; a list of unsafe edits is refused; a newer file with an override parses.
