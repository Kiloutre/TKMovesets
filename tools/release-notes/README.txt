TKMovesets2 1.3
===============

Moveset export, import and editing tool for Tekken, now with retail Tekken 8
support and Tekken 7 -> Tekken 8 conversion.


Supported game build
--------------------

Tekken 8, Steam build 26651490 (PE timestamp 0x6AF749A0, the 2026-11-20 patch).
Build 26155785 (PE timestamp 0x6ADB1AD5) used the same addresses except for 15
Tag mode functions.

The Tekken 8 addresses live in game_addresses.ini (section t8-retail). When a
patch moves them, Tools > Addresses scans the game's executable with the byte
patterns stored in the same file and writes the new values; nothing does this
automatically. The program can also download a newer game_addresses.ini from
the project repository. About 30 entries are structure offsets without a
pattern: a patch that changes the game's structures needs a new release.


Read before use
---------------

- Offline only. Use Practice or offline versus. Online loading is not
  implemented; do not use imported movesets in online modes.
- The target player must be in a match (Practice or an offline match) when you
  export or import. Players P1 and P2 are supported.
- Keep backups of your files. Saves are atomic and the first overwrite keeps a
  backup of the original file, but this is still a modding tool.


What's new in 1.3
-----------------

Tekken 8
- Addresses for Steam build 26651490 (the 2026-11-20 patch). Export, import
  and moveset state were checked on it in Practice; Tag mode's 15 moved
  functions were updated by the address scan but Tag mode was not tested on
  this build yet.

Tekken 5 Dark Resurrection (RPCS3) -> Tekken 7 / Tekken 8 conversion
- Requirement "always true" now converts to Tekken 7's "always true". It used
  to become "backturned", so most cancels never fired and imported fighters
  stood stuck in the round's director move.
- Cancel option bits are recomputed with Tekken 7's own rules instead of being
  copied from Tekken 5, whose bits mean different things. Moves now return to
  idle the way native Tekken 7 moves do.
- Pushback loop counts are converted from Tekken 5's fixed point (1.0 =
  65536). Copied as is, every hit pushed the opponent tens of thousands of
  times too long, until the game snapped them back across the stage.

Tag Tournament 2 -> Tekken 7 / Tekken 8 conversion
- Wii U extractions (EU v32, Cemu) are supported.
- Requirement and property ids, value tables and hitbox bytes now come from
  rules voted over the Wii U extractions, start and end properties included.
- Cancels on the tag button are disabled.
- Projectiles: the game's projectile rows are converted, and item spawns
  (balloons, cloches) map to Tekken 7's spawn properties. The original item
  models do not exist in Tekken 7 or 8.

Old TekkenMovesetExtractor folders
- Headless convert-json turns folders saved by the old Python extractor
  (Tekken 7 and Tag 2 JSON, anim/, mota files) into .tkmvst files.
- Cancels and reactions that point to moves the folder does not have (modded
  movesets such as Gill and Chun-Li) are sent to valid moves instead of
  making the file invalid. Tag 2 projectile rows the extractor saved as zeros
  get the game's rows.

Interface and tools
- Search boxes for the moveset lists in the Import and Editor tabs.
- Tekken 7 validation names the cancel it refuses (list, index, reason).
- Tag mode accepts another mod's hook on ProcessEvent.


What's new in 1.2
-----------------

- Tekken 7 -> Tekken 8: hand, face, wing and camera animations are converted.
- Tekken 8 editor: live editing of an imported moveset.
- Editor: cancel graph for Tekken 7 and Tekken 8 moves.
- Tekken 7 editor: grouped cancel references, pushback deletion and MOTA
  animation counts finished.
- Chinese and Russian translations.
- Converted animations keep the body state of down loops.


What's new in 1.1.1 and 1.1.2
-----------------------------

- Tekken 7 -> Tekken 8 animations: facing and juggle body state use Tekken 8's
  own layout, so juggled opponents no longer flip toward the attacker and
  turning moves no longer spin (issue #178).
- Tag mode starts its loader without restarting the game.
- Tekken 7 editor: Create new no longer crashes on movesets without a
  movelist, and it is all or nothing.
- Tekken 8 editor: hitbox locations show the game's bone names; float fields
  are coloured.


What's new in 1.0
-----------------

Tekken 8 (retail)
- Export: saves the moveset of P1 or P2 from a running match: the 20 record
  tables plus the character's animations (body, hands, face, swing, camera,
  extra).
- Import: loads a .tkmvst onto P1 or P2, including onto a different character.
  A check runs the importer against a local copy of game memory and audits the
  result before anything is written to the game.
- Editor: opens Tekken 8 .tkmvst files offline. Browse and search the 20
  tables, edit fields of moves, cancels, requirements, properties, hitboxes,
  hit conditions, reactions, pushbacks, inputs and more; create, duplicate and
  delete records and lists; save with no compression, LZ4 or LZMA. After an
  import you can play a move on a player and edit live.
- Animations: export and import individual body animations (PANM) between
  movesets.

Tekken 7 -> Tekken 8 conversion
- Convert (Edition menu) writes a separate T8_<name>.tkmvst from a Tekken 7
  moveset. Import also accepts Tekken 7 files and converts them on the fly.
- The conversion is self-contained: records and animations come from the
  Tekken 7 file plus tables shipped with the program. No Tekken 8 donor moveset
  is needed.
- Converted: moves, cancels, requirements and properties (mapped by handler
  comparison between the two games), animations, voiceclips, hitboxes, walk
  cancel timing, throws and projectiles (spawns and the projectile table).
- All 51 Tekken 7 characters convert and pass the offline import plan check.

Command line
- TKMovesets2.exe --headless --help lists file and Tekken 8 commands
  (validate, convert, convert-t8-full, export-t8, check-import-t8, import-t8
  and more). Output is JSON.


Known gaps
----------

- Projectiles from converted movesets travel and hit, but the original visual
  (for example Akuma's fireball) does not exist in Tekken 8: only the hand
  effect and the hit spark show.
- Hand, face, wing and camera animations played by properties are dropped from
  converted movesets: in Tekken 8 those ids index the target character's own
  assets, which converted movesets do not carry.
- Cancels and properties that depend on story mode, AI or game-mode /
  customization checks are kept off on purpose, as are stage gimmicks.
- Some Tekken 7 properties have no Tekken 8 equivalent or need parameter
  changes that are not implemented; they are dropped (the conversion report
  lists them).
- Jack, Gigas and Panda conversions have not been tested in game.
- Training-mode movelists are not converted or imported.
- Forcing a move from the editor preview while a character is stuck in certain
  shared states can crash the game; this also happens on unmodified characters
  (game-side).


Files
-----

TKMovesets2.exe        The program.
TKMovesetLoader.dll    Loader used by online/legacy features. The program
                       recreates it from its own copy if it is missing.
game_addresses.ini     Game addresses (see "Supported game build").
TKMInterface\          Editor labels and Tekken 8 character/move names.
LICENSE.txt            GNU GPL v3.
THIRD_PARTY_NOTICES.txt Licenses of the libraries built into the program.
SHA256SUMS.txt         Checksums of the files above.

The program stores its settings in %APPDATA%\TKMovesets2\TKMSettings.ini.
A TKMSettings.ini left next to the executable by an earlier version is copied
there on the first start and is not used after that.
