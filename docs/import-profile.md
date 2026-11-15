# Import profile

The import profile is a set of per-moveset options that change how a moveset is built when it is imported or
converted. It never changes the moveset data itself.

Code: `TKMovesets2/LocalStorage/ImportProfile.hpp` and `ImportProfile.cpp` (namespace `ImportProfile`).

## Where it is stored

The profile is stored in the file's custom property list: the `TKMovesetProperty {i32 id, i32 value}` entries
between the header and the block list (see [file-formats.md](file-formats.md#custom-property-list)).

- The list starts at the 8-aligned `header_size` and ends at the first entry with id 0
  (`TKMovesetProperty_END`) or at `block_list`, whichever comes first. At most 4096 entries are read
  (`ImportProfile::ReadProperties`).
- The list is outside the payload, so it is never compressed and can be read and changed without decompressing.
- Profile ids are `0x495000xx` (`'I' 'P'` + option number). Other ids in the list are not part of the profile and
  are kept unchanged.
- Both T7 and retail T8 files carry the list. Files written before profiles existed hold only the END entry; they
  read as the default profile.
- The T7 checksum is computed over an empty list (see [file-formats.md](file-formats.md#checksums)), and the
  retail T8 checksum covers only the payload. Changing the profile does not change a file's `crc32`.

## Keys

`ImportProfile::ToProperties` writes every key below, in this order, followed by END. `FromProperties` reads them:
an unknown id is ignored, a missing id keeps its default, and a value outside the accepted range is ignored (the
default stays).

| Id | Enum | Field | Accepted values | Default |
| --- | --- | --- | --- | --- |
| 0x49500001 | `IdVersion` | profile format version | written as 1, not read | 1 |
| 0x49500002 | `IdIdentity` | `identity` | 0 adopt target, 1 fixed | 0 |
| 0x49500003 | `IdCharacter` | `character` | 0..255 | 0 |
| 0x49500004 | `IdGender` | `gender` | 0 from target, 1 male, 2 female | 0 |
| 0x49500005 | `IdStoryFlag` | `storyFlag` | -1 game decides, 0..65535 | -1 |
| 0x49500006 | `IdInHeat` | `inHeat` | 0 no, any other value yes | 0 |
| 0x49500007 | `IdFlagsClear` | `flagsClear` | 0 no, any other value yes | 1 |
| 0x49500008 | `IdStoryMode` | `storyMode` | 0 default, 1 versus | 0 |
| 0x49500009 | `IdDlcStory` | `dlcStory` | 0 default, 1 versus | 0 |
| 0x4950000A | `IdWaveIndex` | `waveIndex` | -1 game decides, 0..65535 | -1 |
| 0x4950000B | `IdBattleScript` | `battleScript` | -1 game decides, 0 or more | -1 |
| 0x4950000C | `IdSpecialModes` | `specialModes` | 0 default, 1 versus | 0 |
| 0x4950000D | `IdItem` | `item` | 0 default, 1 none, 2 all | 0 |
| 0x4950000E | `IdBodyType` | `bodyType` | 0 default, 1 normal, 2 big, 3 bear | 0 |
| 0x4950000F | `IdStage` | `stage` | 0 default, 1 versus | 0 |
| 0x49500010 | `IdStoryEffects` | `storyEffects` | 0 keep, 1 strip | 0 |
| 0x49500011 | `IdGlobalWrites` | `globalWrites` | 0 keep, 1 strip | 0 |

The defaults reproduce the import and conversion behaviour from before profiles existed. With every key at its
default the profile decides nothing.

## What each key does

Requirement ids are in each game's own numbering. "Always true" and "never true" mean the requirement row is
rewritten so its check always passes or never passes. The rules are in `ImportProfile::RequirementOverride`
(requirements) and `ImportProfile::StripsProperty` (properties).

| Key | Effect |
| --- | --- |
| `identity`, `character` | Self character checks. 0: the moveset acts as the character it is imported onto. 1: it acts as `character` (an id in the file's game numbering). T7: requirement 217 and the related checks in `ImporterT7::ApplyCharacterIDFixes`. Retail T8: `RetailT8Import::AdaptCharacterRequirements` uses `character` as the source identity |
| `gender` | T7 583 (male) / 584 (female), T8 689 (male) / 690 (female). When set, the row matching the chosen gender is always true and the other never true |
| `storyFlag` | T8 777 "story flag == X": when set, true only when X equals the value. Also the conversion rule setting `story_flag` (0 when the game decides). 1..4 select story-chapter movesets |
| `inHeat` | Conversion rule setting `in_heat` (T8 to T7 conversion: T8 1028 "player is in Heat") |
| `flagsClear` | Conversion rule setting `flags_clear` (T8 to T7 conversion: flag checks 476 / 439 compared with 0) |
| `storyMode` | Versus: T7 558, 559, 636, 811 never true. T8 667, 668, 786, 789, 806, 1014 never true; 778, 796 always true; 484 / 485 / 486 (story flag >= / <= / == X) evaluated with value 0 |
| `dlcStory` | Versus: T8 801, 802, 810 never true; 803 always true |
| `waveIndex` | When set: T8 783 / 784 / 785 (>= / <= / == X) and 1016 (== X) evaluated against the value |
| `battleScript` | When set: T8 769 / 770 / 771 (>= / <= / == X) evaluated against the value |
| `specialModes` | Versus: T7 556, 560, 634, 765 and T8 664, 666, 764, 765, 766, 767, 772 never true (arcade, customization, treasure battle, Tekken Ball, episode) |
| `item` | None: T7 609, 643 and T8 718 never true. All: always true |
| `bodyType` | Normal / big / bear: T8 687 (bear) true only for bear; T8 691 (big) true for big and bear |
| `stage` | Versus: T7 573 never true, 574 always true. T8 682, 775 never true, 683 always true |
| `storyEffects` | Strip these properties. T7: 33347, 33348, 33437, 33439, 33471, 33807, 33808, 33812, 33813, 33814, 33994. T8: 33109..33111, 33549, 33555, 33709, 33760, 33788, 34130, 34131, 34136, 34137, 34144, 34145, 34149, 34151..34156, 34160, 34161, 34168 |
| `globalWrites` | Strip these T8 properties: 33536, 33674, 33743, 33781, 34027, 34159 |

`ImportProfile::ConversionSettings` gives the values the conversion rules read through `setting(NAME)`:
`story_flag`, `in_heat`, `flags_clear`.

## How it is applied

| Route | What happens | Code |
| --- | --- | --- |
| T7 import | Requirement rows the profile decides become 0 (always true) or the rules' `t8_to_t7_req_never_true` id with parameter 0. Stripped extra properties get frame 0x7FFF (`StrippedFrame`), which no move reaches. Identity is applied in the character id fixes | `ImporterT7::ApplyCharacterIDFixes` (`Importers/Importer_t7/Importer_t7.cpp`) |
| T7 import of a retail T8 file | The file is converted on the fly with its profile | `Importer_t7_OtherGames.cpp`, `T8ToT7::Convert` |
| Retail T8 import | Requirement rows (table 1) decided by the profile are rewritten to all zero (always true) or to the rules' never-true id and parameter. Stripped extra properties (table 9) get frame 0x7FFF. Identity goes through `ProfileIdentity` | `RetailT8Import::ProfileIdentity`, `ApplyProfileConditions`, `ApplyProfileProperties` (`Importers/Importer_t8/RetailImport.cpp`), called from `Importer_t8.cpp` |
| Retail T8 audit and live edit | Same rewrite, so the expected image matches the imported one | `RetailImport.cpp` (audit), `RetailLiveEdit.cpp` |
| T8 to T7 conversion | Decided requirements are resolved, stripped properties (extra, start and end) are dropped | `T8ToT7::Convert` (`MovesetConverters/T8ToT7.cpp`) |
| T7 to T8 conversion | Same, with T7 numbering | `T7ToRetailT8::ConvertStandalone` (`MovesetConverters/T7ToRetailT8Standalone.cpp`) |

Native imports only strip extra properties. Start and end properties can only be dropped by the conversions.

## How it is written

| Writer | What it writes | Code |
| --- | --- | --- |
| T7 export | Default profile | `T7Export::Serialize` |
| Retail T8 export, retail serializer | Default profile unless a list is given | `RetailT8Snapshot::Serialize` |
| Conversions | The source file's profile. A fixed `character` is mapped to the target game's numbering through the rules map `t7_t8_character`; if it has no counterpart, `identity` falls back to adopt target | `T8ToT7::Convert`, `T7ToRetailT8::ConvertStandalone`, `MovesetFile::ConvertToT7` |
| T7 editor save | Keeps the list as loaded | `EditorLogic::BuildSaveSnapshot` |
| Retail editor save | Passes the loaded list through | `RetailEditorOperations.cpp`, `RetailEditorData::Serialize` |
| Import settings popup | Rewrites the list | `Submenu_Edition::RenderImportProfilePopup` (`Layouts/Submenus/Submenu_Edition.cpp`) |
| Headless `set-profile` | Rewrites the list | `main/headless.cpp` |

`ImportProfile::WithProfile` rewrites a file with a new profile: it removes the profile ids (0x49500001..0x49500011),
appends the new ones and END, and moves `block_list` and `moveset_data_start` by the change in list size. The header
fields before the list, the block list, any padding after it and the payload (compressed or not) are copied byte for
byte. The result is written with `FileIO::WriteAtomically`. The popup refuses to save while the file is open in an
editor, because the editor would write its own copy of the list back on its next save.

## Editing the profile

GUI: the **Import settings** button on each moveset in the Edition list. **Defaults** resets every key.

Headless (`main/headless.cpp`):

- `profile FILE` prints `stored` (whether the file has any properties) and the profile (identity, gender, story flag,
  in Heat, flags clear and a text summary from `ImportProfile::Describe`).
- `set-profile FILE OUTPUT KEY=VALUE...` writes a copy with the given keys changed. The output must differ from the
  input.

| `set-profile` key | Values | Field |
| --- | --- | --- |
| `identity` | `target` or a character id 0..255 | `identity`, `character` |
| `gender` | `target`, `male`, `female` | `gender` |
| `story_flag` | `game` or 0..65535 | `storyFlag` |
| `in_heat` | 0, 1 | `inHeat` |
| `flags_clear` | 0, 1 | `flagsClear` |
| `story_mode` | 0, 1 | `storyMode` |
| `dlc_story` | 0, 1 | `dlcStory` |
| `special_modes` | 0, 1 | `specialModes` |
| `stage` | 0, 1 | `stage` |
| `wave_index` | `game` or 0..65535 | `waveIndex` |
| `battle_script` | `game` or 0..2147483647 | `battleScript` |
| `item` | `game`, `none`, `all` | `item` |
| `body` | `game`, `normal`, `big`, `bear` | `bodyType` |
| `story_effects` | 0, 1 | `storyEffects` |
| `global_writes` | 0, 1 | `globalWrites` |

## Not covered by the profile

Game mode checks (T7 563, T8 672) have no option: the value versus uses in each game is unknown. Sides, walls,
rounds, timer, CPU or human control, the practice dummy, opponent gender and size, and Rage are left to the game.
