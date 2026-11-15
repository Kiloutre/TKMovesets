TKMovesets2 1.1
===============

Moveset export, import and editing tool for Tekken, now with retail Tekken 8
support and Tekken 7 -> Tekken 8 conversion.


Supported game build
--------------------

Tekken 8, Steam build 26155785 (PE timestamp 0x6ADB1AD5).

The Tekken 8 addresses live in game_addresses.ini (section t8-retail). The
program checks the game's PE timestamp and will not detect players on any other
build. A game patch therefore needs new addresses: until an updated
game_addresses.ini is published (the program can download a newer one from the
project repository) or a new release ships, Tekken 8 export, import and live
features will not work on the patched game.


Read before use
---------------

- Offline only. Use Practice or offline versus. Online loading is not
  implemented; do not use imported movesets in online modes.
- The target player must be in a match (Practice or an offline match) when you
  export or import. Players P1 and P2 are supported.
- Keep backups of your files. Saves are atomic and the first overwrite keeps a
  backup of the original file, but this is still a modding tool.


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
