# Headless commands and tests

`TKMovesets2.exe --headless COMMAND [ARGUMENTS]` runs one command without opening the GUI. The dispatcher is
`RunHeadless` in `TKMovesets2/main/headless.cpp`; `WinMain` (`main/main.cpp`) calls it before changing the working
directory, checking updates or discovering games.

## General behaviour

- `TKMovesets2 --help` (also `-h`, `/?`) and `--headless --help` print the command list. `--headless` with no command
  prints it and exits 2.
- Commands print JSON on stdout (one object or array).
- Exit status: **0** success, **1** failure (`std::exception`, or a command reporting `ok: false`), **2** usage error
  (`std::invalid_argument`: wrong arguments, unknown command, unknown game key, output naming an input). Errors print
  `{"ok": false, "error": "..."}`.
- Relative paths resolve from the caller's current directory. This includes `game_addresses.ini`: game addresses and
  conversion rules are read from the current directory when the file exists there, else from the embedded copy.
- The exe is a GUI-subsystem program. When stdout is not redirected it attaches to the parent console; shells may not
  wait for it. Pipe the output (`| Out-String`) or use `Start-Process -Wait` when the exit code matters.
- Codecs: `none`, `lz4`, `lzma`, `compact-zstd`. File commands default to `none` unless stated.
- `PLAYER_SLOT`: 0 = P1, 1 = P2 (up to the game's character count for `export`/`import`).
- `CATEGORIES` (retail T8 animations): `default`, `all`, `none`, or a comma list of `body,hands,face,swing,camera,extra`.
- Commands that write an output refuse an output naming one of their inputs (lexical match, or the same existing file
  through `std::filesystem::equivalent`: other case, hard link, 8.3 name). Most use the `SameFile` helper.
- Game access column below: **offline** = no process; **disk** = reads the game's files only; **read** = attaches to the
  running game without write rights; **writes** = attaches with write rights and changes game memory.

## Moveset files

| Command | Arguments | What it does | Game | Output |
| --- | --- | --- | --- | --- |
| `list` | `DIRECTORY` | Scans a moveset folder with the GUI's scanner (`LocalStorage`), including invalid rows | offline | Array of `{file, valid, game_id, editable, character, original_character}` |
| `inspect` | `FILE...` | Opens and decompresses each file (`MovesetFile`) | offline | Array of `{file, ok, game_id, minor_version, origin, character, payload_bytes, compression}` or `error`. Exit 1 if any failed |
| `validate` | `FILE...` | `inspect` plus `MovesetFile::Validate` (layout, references; older games through their T7 conversion) | offline | Same as `inspect` |
| `convert` | `INPUT OUTPUT [CODEC]` | `MovesetFile::ConvertToT7`: T7 copied after validation; TTT2/TREV/T6/T5/T5DR through the legacy converters; retail T8 through `T8ToT7::Convert` | offline | `{ok, output, bytes}` |
| `convert-t8` | `INPUT OUTPUT [CODEC]` | `MovesetFile::ConvertToT8`, which is `T7ToRetailT8::ConvertStandalone` (see note) | offline | `{ok, output, bytes}` |
| `convert-t8-full` | `T7_INPUT OUTPUT [CODEC] [REPORT.json]` | `ConvertStandalone` with report | offline | `{ok, output, bytes, conversion}`; stdout omits `requirements`, `properties` and `animations.converted_moves`, REPORT.json has everything |
| `convert-t7` | `T8_INPUT OUTPUT [CODEC] [REPORT.json]` | `T8ToT7::Convert` with report. Retail T8 input only | offline | `{ok, output, bytes, conversion}`; stdout omits `unresolved_ids` / `dropped_ids` |
| `recompress` | `INPUT OUTPUT [CODEC]` | Validates, then rewrites with another codec | offline | `{ok, output, bytes}` |
| `profile` | `FILE` | Shows the import profile | offline | `{ok, stored, profile}` |
| `set-profile` | `FILE OUTPUT [KEY=VALUE...]` | Writes a copy with profile options changed; moveset data copied byte for byte. Keys: `identity=target\|ID`, `gender=target\|male\|female`, `story_flag=game\|N`, `in_heat=0\|1`, `flags_clear=0\|1`, `story_mode`, `dlc_story`, `special_modes`, `stage` (`0\|1`), `wave_index=game\|N`, `battle_script=game\|N`, `item=game\|none\|all`, `body=game\|normal\|big\|bear`, `story_effects=0\|1`, `global_writes=0\|1` | offline | `{ok, output, profile}` |
| `check-rules` | `[FILE]` | Validates the conversion rules of a `game_addresses.ini` or a bare rules file; without FILE, reports the rules in use. See [conversion-rules.md](conversion-rules.md) | offline | `{ok, version, format, values, map_lines, lists, tables, transforms, t8_move_fields, ...}` |
| `convert-records-t8` | `INPUT OUTPUT.json` | Converts T7 requirements and extra properties with the explicit record rules (`RetailT8::ConvertRecords`) into a logical JSON, not loadable | offline | `{ok, loadable_moveset: false}`; on unsupported records the report with `issues` and exit 1, output untouched |
| `plan-convert-t8` | `T7_INPUT OUTPUT.json` | Writes the strict planner's report (`T7ToRetailT8::Build` / `Report`) | offline | `{ok, loadable_moveset: false, issues_total}`; exit 1 when records are unsupported (plan still written) |
| `plan-import-t8` | `FILE...` | Offline pointer-graph preparation of retail files (`RetailT8Import::Prepare`) | offline | Array of `{file, ok, table_bytes, relocations, issues_total, issues, dependencies, available_panm, unavailable_panm, ...}` |
| `repair-handles-t8` | `CONVERTED DONOR OUTPUT [CODEC]` | For converted documents written before handles were kept: copies native body handles from a retail export, matched by animation hash (`EditorT8Retail::AdoptAnimationHandles`) | offline | `{ok, moves_updated, moves_unresolved, bytes}` |
| `animation-info` | `FILE...` | Reads T7-style animation files | offline | Array of `{file, ok, frames, bytes, crc32}` |

Note on `convert-t8`: it runs the same self-contained converter as `convert-t8-full`, without the report.

## Retail T8 animations

| Command | Arguments | What it does | Game | Output |
| --- | --- | --- | --- | --- |
| `read-animation-t8` | `ANIMATION_FILE NEW_PANM_FILE` | The editor's animation import reader: `.panm` as is, T7 0x64/0xC8 converted; validated | offline | `{ok, bytes, frames}` |
| `extract-animations-t8` | `FILE NEW_DIRECTORY` | The editor's "Extract animations" for one retail file | offline | `{ok, animations}` |
| `export-panm-t8` | `DOC MOVE OUTPUT.panm` | Writes one move's PANM. MOVE is a row or alias (`0x8001`) | offline | `{ok, move, instance, bytes}` |
| `export-panms-t8` | `DOC NEW_DIRECTORY` | Writes every PANM of a document | offline | `{ok, animations, directory}` |
| `import-panm-t8` | `DOC MOVE FILE.panm OUTPUT [NAME\|-] [CODEC]` | Puts a PANM on a move and saves a copy | offline | `{ok, move, instance, custom, replaced, animation_name, rewritten, bytes}` |
| `check-panm-t8` | `PANM_FILE...` | Whether each PANM can be imported (`EditorT8Retail::ValidatePanm`) | offline | Array of `{file, ok, bytes}` |
| `panm-dump` | `PANM_FILE OUTPUT.json` | Raw/static PANM to JSON `{frames, bones}` | offline | `{ok, frames, tracks}` |
| `panm-reencode` | `PANM_FILE OUTPUT.panm` | Decode raw, re-encode with `RetailPanm::Encode` (layout diagnostic) | offline | `{ok, bytes}` |
| `panm-to-t7` | `PANM_FILE OUTPUT_FILE c8\|64` | `RetailT8AnimToT7::Convert` | offline | `{ok, bytes, frames, type}` |
| `panm-skeleton-inc` | `INPUT.panm OUTPUT.inc` | Dev tool: writes the skeleton table used as `T7AnimToRetailT8Skeleton.inc` | offline | `{ok, bones, frames}` |
| `t7-anim-to-panm` | `T7_FILE MOVE_ROW T8_DONOR_WITH_ANIMATIONS OUTPUT.panm` | Converts one T7 move animation with a donor file's template animation (template overload of `T7AnimToRetailT8::Convert`; not the converter's path) | offline | `{ok, bytes}` |
| `dump-t7-anims` | `T7_FILE OUTPUT_DIRECTORY` | Research: every distinct T7 body animation as `<offset>.f32` plus `index.json` | offline | `{ok, animations, failed}` |

## Game files on disk

| Command | Arguments | What it does | Game | Output |
| --- | --- | --- | --- | --- |
| `read-tkdata-t8` | `TKDATA_BIN VFS_PATH OUT_FILE` | One decrypted file of the retail archive (output must be new) | disk | `{ok, bytes, anmbin_identities}` |
| `extract-disk-animations-t8` | `TKDATA_BIN NEW_DIRECTORY [CODE,...]` | Every PANM of `com.anmbin` and the character `.anmbin` files | disk | `{ok, archives, animations}` |
| `build-disk-movesets-t8` | `TKDATA_BIN DIRECTORY [CODE,...\|all] [CATEGORIES] [CODEC]` | Builds retail movesets from the game's files (`RetailT8Disk::WriteMovesetFile`). Codec default `compact-zstd`. A moveset whose file exists fails; nothing is overwritten | disk | `{ok, movesets: [{code, ok, domain, moves, animations, dataless} \| {code, ok:false, error}]}`; exit 1 if any failed |
| `build-disk-movesets-t7` | `TKDATA_BIN DIRECTORY [CODE,...\|all] [CODEC]` | Builds T7 movesets from the game's files (`T7Disk::WriteMovesetFile`). Codec default `lz4`. Existing files are left alone (`existed`) | disk | `{ok, movesets: [{code, ok, name, character, moves, animations, existed}]}`; exit 1 if any failed |
| `verify-anmbin-t8` | `FILE ANMBIN_DIRECTORY` | Checks a retail export's PANMs against extracted archives | disk | `{ok, character, animation_character, code, blobs, bytes}` |
| `dump-payload-t8` | `FILE OUT_FILE` | Writes the decompressed payload (output must be new). For T7 files also the block offsets | offline | `{ok, bytes[, blocks]}` |
| `t8-record-stats` | `OUTPUT.json IDLE_DIRECTORY FILE...` | Dev tool: field value statistics over native retail exports, idle PANMs saved to IDLE_DIRECTORY; source of the built-in converter tables | offline | `{ok, files, moves}` |

## Game addresses

| Command | Arguments | What it does | Game | Output |
| --- | --- | --- | --- | --- |
| `scan-addresses-t8` | `[EXE] [--ini FILE]` | Searches the T8 exe file for the address patterns; changes nothing. EXE defaults to the running game's exe, else the Steam install; the ini to the one next to the program | disk | `{ok, written, exe, ini, pe_timestamp, changes, updated, failed, keys}` |
| `update-addresses-t8` | `[EXE] [--ini FILE]` | Same, and writes changed values to the ini | disk (writes the ini file) | Same; exit 1 if the write failed |

## Running game, any supported game

| Command | Arguments | What it does | Game | Output |
| --- | --- | --- | --- | --- |
| `games` | none | Lists game profiles from `Games.cpp` | offline | `{ok, games: [{key, name, process, game_id, minor_version, characters, export, import, imports_from}]}` |
| `export` | `GAME DIRECTORY PLAYER_SLOT\|all [--codec C] [--motas default\|all\|none\|0,1,...] [--animations CATEGORIES] [--no-archive] [--unknown-motas] [--overwrite] [--no-movelist]` | The Extract menu for the game key (`--animations` and `--no-archive` retail T8 only). Defaults as in the menu | read | `{ok, game, directory, exports: [{slot, ok, code, error, character, name, files}]}` |
| `import` | `FILE GAME PLAYER_SLOT [--no-instant] [--keep-unused] [--motas all\|none\|0,1,...]` | The Import menu. Defaults: apply instantly, free unused movesets, all MOTAs. The file's game must be one the target imports | writes | `{ok, game, slot, code, error, source_game, source_character, previous_moveset, moveset, bytes}` |

## Running game, retail T8

All attach to `Polaris-Win64-Shipping.exe`.

| Command | Arguments | What it does | Game | Output |
| --- | --- | --- | --- | --- |
| `export-t8` | `NEW_DIRECTORY PLAYER_SLOT [CATEGORIES] [no-archive]` | Exports a player's moveset (compact-zstd). `no-archive`: animations the game has not loaded are recorded as missing instead of read from `tkdata.bin` | read | `{ok, code, slot, character, error}` |
| `check-import-t8` | `FILE PLAYER_SLOT` | Runs the importer into local shadow memory (`RetailImportShadow`) and audits the image (`RetailT8Import::AuditImage`). T7 files are converted first | read | Import report (below) plus `live_root_unchanged`, `shadow_published`, `shadow_writes`, `image_audit_ok`, `audited_bytes`, `audited_animations`, `external_cancel_lists` |
| `import-t8` | `FILE PLAYER_SLOT` | Imports a retail or T7 file (memory writes, no DLL), then watches the player for 3 s | writes | Import report plus `active_move_observations`, `process_alive`, `published` |
| `stage-t8` | `FILE PLAYER_SLOT` | Full import with writes except the player's pointers (benchmark) | writes | Import report plus `player_roots_unchanged`, `process_alive`, `timings_ms` |
| `check-preview-t8` | `FILE PLAYER_SLOT MOVE_ID` | Shadow import plus move queue | read | As `check-import-t8` plus `preview_move`, `preview_next_pointer`, `preview_next_id` |
| `preview-t8` | `FILE PLAYER_SLOT MOVE_ID` | Real import and move queue | writes | As `import-t8` plus the preview fields |
| `queue-t8` | `PLAYER_SLOT MOVE_ID` | Queues a move (row or `0x80xx` alias) on the current moveset | writes | `{ok, slot, root, moves, character, current, queued}` |
| `state-t8` | `PLAYER_SLOT` | Reads the player's moveset state | read | `{ok, slot, root, moves, character, current}` |
| `live-edit-t8` | `PLAYER_SLOT PUBLISHED_FILE EDITED_FILE` | Writes edited records into the published moveset (`RetailLiveEdit::Sync`) | writes | `{ok, structural, reason, records, bytes, root}`; exit 1 when the change is structural |
| `live-verify-t8` | `PLAYER_SLOT DOC [EDIT...]` | Imports DOC, applies each EDIT through the editor and live editing, audits after each step. EDIT: `TABLE:ROW:FIELD=VALUE`, `anim:ROW=DONOR_ROW`, `rename:ROW=NAME`, `rows:TABLE:FIRST=ID,...`, `delete:TABLE:ROW=COUNT`, `revert` | writes | `{ok, slot, steps}` |
| `compare-compiled-t8` | `PLAYER_SLOT [COUNT]` | Compares engine-compiled animations with `RetailT8Resources::BuildCompiledAnimation` (default 64) | read | `{ok, domain, compared, matched, skipped, unsupported, mismatches}`; exit 1 unless all match |
| `anim-info-t8` | `PLAYER_SLOT MOVE...` | Resolves move animation handles to native resources | read | `{ok, container, moves}` |
| `poke-t8` | `ADDRESS QWORD` | Research: writes one qword | writes | `{ok, address, old, new}` |
| `tag-t8` | `P3 P4\|none [SECONDS]`, `off [SECONDS]`, `trigger A\|B` | Tools > Extra Tag mode: injects the loader DLL (`OnlineT8`), sends the command through shared memory, prints status lines for SECONDS (default 5). `trigger` (not in the help text) sends a tag request to a running loader | writes (injects a DLL) | `{ok, injected_now, state, status}`; `trigger`: `{ok, team}` |

Import report fields (`import-t8` family): `ok`, `dry_run`, `code`, `error`, `slot`, `previous_moveset`, `moveset`,
`bytes`, `progress`, `source_character`, `target_character`, `binding_root`, `character_fixes`, `memory_only`,
`published_animations`, `reused_animations`, `memory_only_fallback_moves`, `native_bound_animations`,
`native_unloaded_kept`, `archive_animations`, `dataless_animations`, `unavailable_animations`, `archive_error`,
`binding_identity_mismatches`, `binding_fallback_moves`, `dropped_external_cancels`, `target_fallback`, `fallback_cancel_lists`, `edited_animations_kept`,
`native_snapshot_v4`, `custom_animations`, `custom_animations_missing_carrier`, `carrier_fallback_moves`,
`restored_carriers`. When a T7 file fails to convert: `{ok: false, error, process_attached: true}`, exit 1.

## Self-tests

| Command | Arguments | What it does |
| --- | --- | --- |
| `ui-smoke` | none | Builds the application offscreen and renders every menu in two locales and three sizes; ImGui errors fail it. Output `{ok, frames}` |

Test builds only (`TKM_EDITOR_FORM_TESTS`, set when `BUILD_TESTING` is on):

| Command | Arguments | What it does |
| --- | --- | --- |
| `editor-smoke` | none | Runs the editor form tests (`tests/EditorFormTests.cpp`, `tests/RetailEditorTests.cpp`) |
| `retail-editor-smoke` | `INPUT OUTPUT` | Edits and saves a retail document, checks the result |
| `retail-editor-structure-smoke` | `INPUT OUTPUT` | Duplicates and renames moves, creates lists, saves, reopens, checks animation bytes |
| `retail-editor-render` | `INPUT OUTPUT.json [references\|lists]` | Renders the retail editor offscreen and dumps draw data |
| `retail-editor-ui-test` | none | Dear ImGui Test Engine run over the retail editor |
| `retail-panm-fixture` | `OUTPUT_DIRECTORY` | Writes PANM test fixtures |
| `anim-preview-check` | `REFERENCE.json` | Animation preview pose and skinning against Blender (`tests/data/anim_preview_blender_reference.json`, from `tools/build_preview_mannequin.py`) |
| `anim-preview-shot` | `FILE MOVE OUT_DIR [FRAMES] [yaw=R] [pitch=R] [preview=off] [displacement=off] [size=WxH]` | Opens FILE in its editor in a hidden GLFW window with the GUI's OpenGL backend, opens the move form and writes `frame_NNNN.png` per frame (`all`, `A-B` or `1,5,9`). MOVE: row (retail aliases such as `0x8001` resolve) |
| `anim-preview-joints` | `ANIMATION OUT.json` | Mannequin joint positions per frame of a `.panm` or T7 animation |
| `anim-preview-export` | `T7_FILE MOVE OUT [panm]` | A T7 move's animation bytes, or their conversion onto the built-in retail skeleton |
| `anim-preview-scan` | `FILE` | Decodes and poses every move animation of a T7 or retail T8 moveset as the move forms do |

## Regression tests

Build with an x64 Visual Studio developer prompt, the CMake presets (`CMakePresets.json`: `x64-debug`, `x64-release`, ...)
and vcpkg. The root `CMakeLists.txt` includes `CTest`, so `BUILD_TESTING` is on by default.

```bat
cmake --preset x64-release -B out/build/verified-release -DCMAKE_TOOLCHAIN_FILE=PATH/TO/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build out/build/verified-release
ctest --test-dir out/build/verified-release --output-on-failure
```

CTest entries (`TKMovesets2/CMakeLists.txt`, `if(BUILD_TESTING)`):

| Test | Runs |
| --- | --- |
| `compact-compression` | `CompactCompressionTests` (`tests/CompactCompressionTests.cpp`) |
| `tag-t8-grow` | `TagT8GrowTests` against a fake battle in process |
| `headless-editor-smoke` | `TKMovesets2 --headless editor-smoke` |
| `moveset-regressions` | `MovesetRegressionTests` with no argument: the full native suite (T7 -> T8, T8 -> T7, PANM, snapshots, import, export graph, process writes, file jobs, editor, semantic validation, animations, ...) |
| `retail-import-preparation` | `MovesetRegressionTests --test-retail-import` |
| `game-static-data` | `MovesetRegressionTests --test-game-static-data` |
| `address-scanner` | `MovesetRegressionTests --test-address-scanner` |
| `headless-ui-smoke` | `TKMovesets2 --headless ui-smoke` |
| `retail-editor-ui` | `TKMovesets2 --headless retail-editor-ui-test` |
| `headless-help`, `headless-games` | `--help`; `games` must list `t8-retail` |
| `headless-missing-file`, `headless-update-addresses-missing-exe`, `headless-export-unknown-game` | Must fail (`WILL_FAIL`) |
| `game-static-data-generation`, `retail-record-conversion`, `editor-reference-data`, `native-asset-inspection`, `headless-regressions` | Python scripts in `tests/` (only when Python 3 is found). `headless-regressions` (`tests/headless_regressions.py`) drives the shipped exe through its headless commands with fixtures written by `MovesetRegressionTests` |

`tests/retail_asset_comparison.py` is in the folder but not registered in CTest.

`MovesetRegressionTests` options (`tests/MovesetRegression.cpp`, `main`):

| Option | Does |
| --- | --- |
| none, or `FILE...` | Full suite; each FILE is also checked as a historical T7/T5 fixture |
| `--test-file-jobs`, `--test-offline-editor`, `--test-game-static-data`, `--test-address-scanner`, `--test-retail-export`, `--test-semantic`, `--test-retail-import` | One part of the suite |
| `--write-fixture OUT`, `--write-semantic-fixture OUT`, `--write-t7-retail-fixture OUT` | Writes a synthetic file |
| `--benchmark-editor PATH` | Editor save benchmark |
| `--check-legacy-export MEMORY.json` | Legacy T7 export image check (`tools/check_legacy_t7_export.py`) |
| `--export-retail-live NEW_DIRECTORY` | Read-only live retail export of every detected slot, validated (needs the game) |

No CTest entry attaches to a game.
