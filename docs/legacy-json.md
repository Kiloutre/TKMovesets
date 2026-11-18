# Old TekkenMovesetExtractor folders

The Python TekkenMovesetExtractor (the tool before TKMovesets) saved each moveset as a folder:

| File | Content |
| --- | --- |
| `<name>.json` | Every record list. Records reference each other by index (`cancel_idx`, `requirement_idx`...); -1 or a negative index means none |
| `anim/<animation name>.bin` | One file per animation, little-endian (the extractor swapped big-endian games) |
| `mota_0.bin` ... `mota_11.bin` | The MOTA files. `mota_0` and `mota_1` are 20-byte placeholders the extractor wrote instead of the game's data |

`--headless convert-json DIRECTORY FOLDER...` turns such folders into `.tkmvst` files
(`TKMovesets2/GameSpecific/Extractors/LegacyJsonMoveset.cpp`):

| JSON `version` | Output | How |
| --- | --- | --- |
| `Tekken7` | `T7_<name> (MovesetExtractor).tkmvst` | The moveset is laid out as the game holds it in memory, then exported by the T7 memory exporter (`T7Export::ReadSnapshot`), like `T7Disk` |
| `Tag2` (`tag2_*`: Cemu Wii U, `rpcs3_tag2_*`: RPCS3 PS3) | `TTT2_<name> (Wii U).tkmvst`, `TTT2_<name> (PS3).tkmvst` | Written as `ExtractorTTT2` writes its files. The TTT2 file converts to T7 (and from there to T8) like a TTT2 memory export |

Other versions (Tekken 4, 5, 6, Revolution) are refused.

`<name>` is the folder name without its game prefix (`t7_FRV_mod` -> `Frv Mod`), so modified movesets keep a name of their
own; `orig_character_name` holds the character's in-game name. `characterId` is the JSON `character_id` and
`extraction_date` the JSON's.

## Details

- An animation missing from the folder (or unreadable) is looked for, by name, in the other folders of the same game
  next to it, then in folders without a game prefix (`animpool`), as the extractor's importer did. The report lists
  them in `borrowed_animations`.
- The extractor read one input more than the list holds (`input_extradata_size + 1`): the last `input_extradata` entry
  is not part of the list, as in the game.
- MOTA files that are not valid MOTA data (TTT2 `mota_2` and `mota_3` use another format) are left out
  (`skipped_motas`), as the TTT2 memory exporter does without "unknown MOTAs". The importer fills missing MOTAs.
- A record index outside its list is written as "none" and counted in `dropped_references`.
- The extractor guessed where each animation ends and often cut the last byte, a 0 in the game (46 of Kazuya's 1310
  files, all identical to the game's files otherwise). An animation file that does not parse gets up to 8 zero bytes
  back (`padded_animations`, 36 to 51 per T7 moveset).
- Some MOTA headers were swapped to little-endian without their animations: those animations are swapped too.
- Many T7 folders hold only the 20-byte header of `mota_9` (camera): it is left out, and the importer fills it.
  Other MOTA files that still do not read are left out (`skipped_motas`).
- The extractor's byte swapper (`ByteSwap.py`, `Swap64Anim`) sizes each bone's base pose with `descriptor - 4 < 4`.
  In the game's code the test is unsigned; in Python it is not, so bones with a descriptor below 4 (12-byte base pose,
  counted in the header's float bones) got 6 bytes, and the rest of the animation was swapped at the wrong offsets:
  the keyframe table stayed big-endian. Its steps are byte swaps, so running them again with the same values gives
  the game's big-endian animation back (`UndoExtractorSwap`), which is then swapped properly. This concerns TTT2
  animations: two-character win poses, item moves, most of Ogre's, and face MOTA animations
  (`unswapped_animations`, `unswapped_mota_animations`).
- An animation no folder has in a readable form is played by the moveset's first animation; its name is kept and
  the report lists it (`replaced_animations`).

## Batch of 2026-11-18

`extracted_chars` (TekkenMovesetExtractor): 54 `tag2_*`, 7 `rpcs3_tag2_*` and 59 of 61 `t7_*` folders converted, all
pass `validate`. Refused: `t7_GILL` (8 cancels to move `0xFFFF`) and `t7_CHUN_LI` (`ValidateT7Semantics`: invalid
cancel target, input sequence or requirement list; cause not found yet). `t4_*`, `t5_*`, `t6_*` were not converted.

## What the JSON format does not hold

The extractor did not save these fields. The converter writes them empty (none / 0), as the extractor's own importer did:

| Data | T7 | TTT2 |
| --- | --- | --- |
| Move start and move end property lists (`moveBeginningProp`, `moveEndingProp`) and the moves' references to them | lost | lost |
| Move `_0x28_cancel_addr`, `_0x48_cancel_addr` (the game's are empty in the T7 files checked) | lost | lost |
| Move `_0x38_cancel_addr` (saved as the game's raw pointer, always 0 in these folders) | lost | lost |
| Move `_0x5E_short` | lost | - |
| Reactions `standing_moveid` (0x4E / 0x32) and the two ints before `vertical_pushback` | 0 | 0 |
| Hit condition `_0xC_int` | 0 | - |
| Projectiles | kept | zeros: the extractor did not read TTT2 projectiles |
| Training movelist | none | none |

Checked against `T7_Kazuya.tkmvst` built from the game's files: the game has 677 move end properties used by 156 of
Kazuya's 2333 moves, no move start properties, and its `_0x28` / `_0x48` cancel pointers, `_0x5E_short`,
`standing_moveid`, reaction ints and hit condition `_0xC_int` are all empty.
