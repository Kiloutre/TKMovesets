# Code map

Where things live in the TKMovesets2 source tree, with the key classes and functions of each folder. Paths are relative
to the repository root. Build files: root `CMakeLists.txt` (project, `include(CTest)`), `TKMovesets2/CMakeLists.txt`
(targets, conversion rules merge, embedded resources, tests), `CMakePresets.json`, `vcpkg.json`.

Targets built by `TKMovesets2/CMakeLists.txt`: `TKMovesets2` (the WIN32 exe), `TKMovesetLoader` (the injected DLL),
`GameSpecific` and `MovesetIO` (static libraries), `FileEmbedder` (resource embedder), and with `BUILD_TESTING`:
`MovesetRegressionTests`, `CompactCompressionTests`, `TagT8GrowTests`.

## `TKMovesets2/main/`

Program entry and headless mode.

| File | Contents |
| --- | --- |
| `main.cpp` | `WinMain`: `--headless` / `--help` go to `RunHeadless` before anything else; otherwise sets the working directory to the exe folder, applies pending updates, starts the GUI |
| `run_gui.cpp` | `run_gui`: GLFW/OpenGL/ImGui setup, locale, icon, main loop |
| `arguments_handling.cpp` | Moveset files passed on the command line (opened in the editor where they are, forwarded through `OpenRequests`); animation files copied into the animation library |
| `headless.cpp` / `.hpp` | `RunHeadless`: every `--headless` command, see [headless.md](headless.md). Also `UiSmoke` |

## `TKMovesets2/Layouts/`

User interface (Dear ImGui).

| File | Contents |
| --- | --- |
| `MainWindow.cpp` | `MainWindow`: owns the game interaction objects, storage and submenus; `Update` renders one frame |
| `NavigationMenu.cpp`, `SideMenu.cpp` | Left navigation; side panel with update status (`SideMenu::CheckForUpdates`) |
| `Submenus/Submenu_Extract.cpp`, `Submenu_Import.cpp` | Extract and Import menus |
| `Submenus/Submenu_Edition.cpp` | Moveset list for editing: open, duplicate, Convert (`RenderConversionPopup`, `FileJob`), import profile (`RenderImportProfilePopup`) |
| `Submenus/Submenu_PersistentPlay.cpp`, `Submenu_OnlinePlay.cpp` | Persistent and online play (T7) |
| `Submenus/Submenu_Addresses.cpp` | Tools > Addresses: exe scan and address update |
| `Submenus/Submenu_Extra.cpp` | Tools > Extra (retail T8 Tag mode) |
| `Submenus/T8DiskExtraction.cpp` | Builds movesets from the game's `tkdata.bin` (`RetailT8Disk`, `T7Disk`) |
| `Submenus/Submenu_About.cpp`, `MovesetListSort.hpp` | About page; moveset list sorting |
| `EditorVisuals/EditorVisuals.cpp` | `EditorVisuals`: base editor window (form windows, subwindows, save, live edit) |
| `EditorVisuals/EditorForm.cpp`, `EditorFormList.cpp`, `EditorSubwindow.hpp`, `EditorClipboard.hpp` | Base classes for record forms, list forms, subwindows; clipboard |
| `EditorVisuals/AnimationPreview.cpp` | `AnimationPreview`: the move forms' 3D animation preview (OpenGL into a texture, orbit camera, timeline with the active frames in red); used by `TEditorMove` (T7) and the retail moves form |
| `EditorVisuals/CancelGraphView.cpp` | `CancelGraphView`: the move forms' read-only cancel graph, T7 (`TEditorMove`) and retail T8 (`RetailRecordForm`, moves table), opened by the icon next to the cancels field and drawn in place of the fields (no Apply footer, `EditorForm::ShowsApplyButton`). One node per move with an `AnimationPreview::Thumbnail` (still pose, plays on hover), coloured by kind (attack, throw, screw / tornado) as are the links leading to it; links labelled with inputs and requirements; pan (drag), zoom (wheel), autoscroll (middle click), click opens the move; Co_ moves hidden unless ticked; incoming cancels behind the button left of the root. Data: `BuildCancelGraph` (`GameSpecific/Editors/CancelGraph.cpp`) over a `CancelGraphSource`, implemented by `EditorT7` (`Editor_t7_CancelGraph.cpp`) and `EditorVisualsT8Retail` (`EditorVisuals_t8Retail_CancelGraph.cpp`): breadth first over the main cancel list, grouped cancels expanded, stops at generic moves, moves already shown, the depth (default 3) and 400 nodes; incoming cancels (one level) on request |

## `TKMovesets2/GameInteractions/`

Threads that talk to a running game.

| File | Contents |
| --- | --- |
| `GameInteraction.cpp` | `GameInteraction` (a `ThreadedClass`): worker thread that attaches to the process, owns the factories' objects, publishes state to the UI |
| `GameExtract.cpp` | `GameExtract`: queued extractions, uses the game's `Extractor` |
| `GameImport.cpp` | `GameImport`: queued imports, uses `Games::FactoryGetImporter`; import warnings for the UI |
| `GameSharedMem.cpp` | `GameSharedMem` (a `GameImport`): DLL injection and shared memory for persistent/online play (`Online`) |
| `AttachRetryBackoff.hpp` | Attach retry timing (1/2/4/8/10 s) |
| `GameProcess/GameProcess.cpp` | `GameProcess`: attach by name or pid, typed reads/writes, allocation, remote threads, DLL injection, AOB scan, `PauseGuard` (suspend/resume), `ExpectBytes` (commit-time preconditions) |
| `GameProcess/RemoteWriteBatch.cpp` | `GameProcess::WriteBatch`: staged writes; new uploads before shared pointer installs; rollback of originals on failure; pinning of allocations when rollback is incomplete |
| `GameProcess/GameData.cpp` | `GameData` (a `GameAddressesWrapper`): reads with base address, endianness and pointer paths from the address file |

## `TKMovesets2/GameSpecific/`

Per-game formats and operations.

| File | Contents |
| --- | --- |
| `Games.cpp` / `.hpp` | `cg_gamesInfo`: the game profile table (Tekken 8 retail, T8 CBT, T8 CNT, Tekken 7, TTT2 1.00/1.03, Tekken Revolution, Tekken 6 RPCS3/PPSSPP, Tekken 5 DR, Tekken 5). Lookups `GetGameInfoFromIndex`, `GetGameInfoFromIdentifier`, `IsGameEditable`, `IsGameLiveEditable`; factories `FactoryGetExtractor`, `FactoryGetImporter`, `FactoryGetEditorLogic`, `FactoryGetEditorVisuals`, `FactoryGetOnline` |
| `GameInfo.hpp` | `GameInfo` (name, process name, prefix, game id/minor version, character count, flags, data strings, supported imports, factories, base address function), `GameFactory<T>`, `EditorFactory<T>` |
| `GameIDs.hpp` | `GameId_*`, `GameVersions` (e.g. `T8::RETAIL`, `T8::FROM_T7`, `T7::FROM_T8`) |
| `BaseAddresses.cpp` | Base address functions: from the address file, RPCS3, PCSX2, PPSSPP |
| `BaseGameSpecificClass.hpp` | Common base: process, game data, game info |

Game profiles and their factories (`Games.cpp`):

| Profile | Extractor | Importer | Editor logic / visuals | Online |
| --- | --- | --- | --- | --- |
| Tekken 8 (retail, `t8-retail`) | `ExtractorT8` | `ImporterT8` (imports T8 and T7 files) | `EditorT8Retail` / `EditorVisualsT8Retail` | none |
| Tekken 8 CBT (`t8cbt`) | none | none | none | `OnlineT8` |
| Tekken 8 CNT (`t8cnt`) | `ExtractorT8CNT` | none | `EditorT8CNT` / none | none |
| Tekken 7 (`t7`) | `ExtractorT7` | `ImporterT7` (imports TTT2, TREV, T6, T5, T8) | `EditorT7` / `EditorVisuals_T7` | `OnlineT7` |
| TTT2 1.00 / 1.03, Tekken Revolution | `ExtractorTTT2` | none | none | none |
| Tekken 6 RPCS3 / PPSSPP | `ExtractorT6` | none | none | none |
| Tekken 5 DR / Tekken 5 | `ExtractorT5DR` / `ExtractorT5` | none | none | none |

### `GameSpecific/Extractors/`

| File | Contents |
| --- | --- |
| `Extractor.cpp` / `.hpp` | `Extractor`: base class (`Extract`, `CanExtract`, `GetCharacterAddress`, `GetCharacterID`, `GetPlayerCharacterName`), block writing and compression helpers (`ExtractorUtils`) |
| `ExtractorSettings.hpp` | `ExtractSettings_*` flags (compression, MOTAs, retail animation categories, `T8_NO_ARCHIVE`, ...) |
| `Extractor_t7/Extractor_t7.cpp` | `ExtractorT7`: Tekken 7 extraction from memory |
| `Extractor_t7/T7ExportSnapshot.cpp` | `T7Export`: pointer-graph read of a T7 moveset and `Serialize` to a T7 file (also used by `T8ToT7`) |
| `Extractor_t7/T7DiskMoveset.cpp` | `T7Disk`: T7 movesets from the game's `tkdata.bin` (`Characters`, `ReadTkdataFile`, `WriteMovesetFile`) |
| `Extractor_t8/Extractor_t8.cpp` | `ExtractorT8`: retail T8 extraction (tables plus animations, from memory or `tkdata.bin`) |
| `Extractor_t8/Extractor_t8cnt.cpp` | `ExtractorT8CNT`: CNT test build |
| `Extractor_t8/RetailSnapshot.cpp` | `RetailT8Snapshot`: the `T8RS` payload (header, table directory `Tables`, `T8AN` assets, names/notes sections, v4 extension), `Validate`, `Serialize`, `Table`, `Assets`, `ReadExtension` |
| `Extractor_t8/RetailGameData.cpp` | `RetailT8GameData`: reads `tkdata.bin`, `Anmbin` archives, `Source` (PANM lookup on disk) |
| `Extractor_t8/RetailDiskMoveset.cpp` | `RetailT8Disk`: retail movesets from `<code>.motbin` / `.anmbin` (`WriteMovesetFile`) |
| `Extractor_t8/RetailRuntime.hpp` | `RetailT8Runtime`: player registry, player offsets, animation maps and handle helpers |
| `Extractor_ttt2/`, `Extractor_t6/`, `Extractor_t5/` | `ExtractorTTT2` (also TREV), `ExtractorT6`, `ExtractorT5`, `ExtractorT5DR` |

### `GameSpecific/Importers/`

| File | Contents |
| --- | --- |
| `Importer.cpp` / `.hpp` | `Importer`: base class (`Import` from file or bytes, `ImportMovesetData`, `CanImport`, `GetCharacterAddress`, `GetMovesetAddress`, `CleanupUnusedMovesets`, `lastLoaded`), `ImportSettingsUtils` |
| `Importer_t7/Importer_t7.cpp` | `ImporterT7`: T7 import (allocation, relocation, MOTAs, `ApplyCharacterIDFixes`) |
| `Importer_t7/Importer_t7_OtherGames.cpp` | `ImporterT7::_Import_FromT8`, `_Import_FromTTT2`, `_Import_FromTREV`, `_Import_FromT6`, `_Import_FromT5`: convert, then import as T7 |
| `Importer_t7/Importer_t7_Utils.cpp` | `T7ImportUtils`, including `ValidateT7Semantics` (used by every T7 producer) |
| `Importer_t8/Importer_t8.cpp` | `ImporterT8`: retail import by memory writes only (no DLL); `ConvertT7` for T7 files; memory-only animation registration; binding route |
| `Importer_t8/RetailImport.cpp` | `RetailT8Import`: `Prepare` (offline pointer graph), `AuditImage`, `AdaptCharacterRequirements`, `ApplyConvertedIdentity`, profile conditions/properties |
| `Importer_t8/RetailImportShadow.hpp` | `RetailImportShadow`: a `GameProcess` that reads the live game and keeps all writes local (dry runs) |
| `Importer_t8/RetailLiveEdit.cpp` | `RetailLiveEdit`: `Sync` / `Apply`, direct record writes into a published moveset |
| `Importer_t8/RetailPreview.cpp` | `RetailT8Preview`: `Read` player state, `Queue` a move |
| `Importer_t8/RetailResourceLoader.cpp`, `RetailAnimationArchive.cpp` | `RetailT8Resources`: native animation resources, locally built compiled animations (`BuildCompiledAnimation`), archive verification (`VerifyArchives`) |

### `GameSpecific/Editors/`

| File | Contents |
| --- | --- |
| `EditorLogic.cpp` / `.hpp` | `EditorLogic`: base editor logic (inputs, record forms, mutations, save snapshot) |
| `TEditor.cpp` | `TEditor`: shared Tekken-family logic (references, list shifting) |
| `RecordEdit.cpp` | `RecordEdit`: transactional record list edits |
| `Editor_t7/` | `EditorT7`: T7 editing, live edition, displayable movelist |
| `Editor_t7/Editor_t7_LiveRetail.cpp` | `T7RetailLive`, `T7LiveShadow`: live edition of a T7 moveset imported into retail T8 (edited rows converted alone, written by `RetailLiveEdit::Sync`) |
| `Editor_t8/` | `EditorT8CNT`: CNT records |
| `Editor_t8Retail/Editor_t8Retail.cpp` | `EditorT8Retail`: retail document editing (records, encrypted fields, names, animations, `BuildSaveSnapshot`) |
| `Editor_t8Retail/RetailEditorData.cpp` | `RetailEditorData`: editor metadata in the outer header (provenance, names, notes) |
| `Editor_t8Retail/RetailEditorOperations.cpp`, `RetailPanmOperations.cpp` | Structural operations; PANM import/export (`ImportPanm`, `MoveAnimation`, `ExportAnimations`) |

### `GameSpecific/EditorVisuals/`

| Folder | Contents |
| --- | --- |
| `EditorVisuals_t7/` | `EditorVisuals_T7`, record forms in `Structures/` (`TEditorMove`, `TEditorCancels`, `TEditorRequirements`, ...), references subwindows, animation subwindow |
| `EditorVisuals_t8/` | CNT requirement/extra-property forms |
| `EditorVisuals_t8Retail/` | `EditorVisualsT8Retail` (retail editor UI, browser), `RetailFieldCodecs` |

### `GameSpecific/MovesetConverters/`

See [conversion.md](conversion.md) and [conversion-rules.md](conversion-rules.md).

| File | Contents |
| --- | --- |
| `T7ToRetailT8Standalone.cpp` | `T7ToRetailT8::ConvertStandalone`, `LiveConversion` (single-row updates for live edition), `ConversionReportJson`, `EncodeRetailField` |
| `T7ToRetailT8.cpp` / `.hpp` | Strict planner/converter: `Build`, `Report`, `Convert`; `Plan`, `ConversionReport`, `ConvertedCharacter` (124) |
| `T8ToT7.cpp` / `.hpp` | `T8ToT7::Convert`, `ReportJson`, `CharacterT8ToT7` |
| `ConversionRules.cpp` / `.hpp` | Rules parser and loader: `Parse`, `Section`, `Embedded`, `Current`, `CurrentStatus`, `Rules::ToT8/ToT7/RequirementHolds/Fix/Guard` |
| `T7AnimToRetailT8.cpp`, `T7AnimToRetailT8Skeleton.inc` | T7 body animation -> PANM on the built-in skeleton (`Convert`, `MaxConvertibleFrames`, `Rewrite`) |
| `T7FaceToRetailT8.cpp` | T7 face MOTA animation -> retail face PANM on the legacy 43-bone `*_Joint` rig (`Convert`), T7/retail face property ids |
| `T7WingToRetailT8.cpp`, `T7WingToRetailT8Rigs.inc` | T7 wing MOTA animation -> retail swing PANM on the built-in rig its rest offsets match (`Convert`), T7/retail wing property ids |
| `T7CameraToRetailT8.cpp` | T7 camera MOTA clip (0xC8) -> retail camera PANM (`Read`, `Convert`), T7/retail camera property ids |
| `T7HandToRetailT8.cpp`, `T7HandToRetailT8Skeleton.inc` | T7 hand MOTA animation -> retail hand PANM (`Convert`), MOTA archive reader (`MotaAnimations`), T7/retail hand property ids and retail hand indices |
| `RetailT8AnimToT7.cpp` | PANM -> T7 0x64 / 0xC8 (`ToChannels`, `Write64`, `WriteC8`, `Convert`) |
| `RetailPanm.cpp` | `RetailPanm`: PANM `Encode`, `DecodeRaw`, `Decode` (packed tracks), `DecodePackedTrack`, `DecodeT7`; multi-group PANMs (`EncodeGroups`, `DecodeGroups`) and the packed codec (`PackTransforms`, `PackScalars`, `UnpackScalars`) |
| `RetailT8Aliases.hpp/.inc` | Alias catalog (`RetailT8::Aliases`, `FindAlias`, 1,239 rows), generated by `tools/generate_retail_t8_conversion_aliases.py` |
| `T7ToRetailT8Votes.inc`, `T8ToT7Votes.inc` | Pooled ID votes (227 rows each), generated by `tools/generate_t7_t8_cross_votes.py` |
| `RetailT8Records.hpp`, `RetailT8Rules.inc` | Logical records and the 14 explicit rules (`ConvertId`, `ConvertRecords`) |
| `RetailNames.hpp` | Retail name hash (`RetailNames::Hash`) |
| `RetailCommands.hpp` | Retail cancel command values |
| `T7AnimToRetailT8.inc` | Earlier fitted animation map; not included by any source file |
| `MovesetFileConverter.cpp` | `CanConvertMoveset`, `ConvertedMovesetPath` |
| `MovesetConverters.hpp`, `Converter_*_to_t7.cpp`, `*_Aliases.cpp`, `Aliases.cpp`, `LegacyLayout.hpp` | Legacy converters `TTT2ToT7`, `TREVToT7`, `T6ToT7`, `T5ToT7`, `T5DRToT7` and their ID aliases |

### Other `GameSpecific/` folders

| Folder | Contents |
| --- | --- |
| `AnimationPreview/` | `AnimPreview::Model` (the mannequin, built from scratch, embedded from `Resources/AnimPreview/mannequin.bin`, written by `tools/build_preview_mannequin.py`), `Pose` (T7 body channels -> bones, BlenderTekkenDirect's mapping), `Skin`; `DecodeClip` (T7 0xC8 / 0x64 and T8 PANM -> T7 body channels); `ApplyDisplacements` (the move's known displacement properties on the root motion); `HitShapes` (approximate hitbox capsules on the mannequin) |
| `AnimExtractors/` | `AnimExtractor`: animation extraction per game (`AnimExtractor_t7.cpp` ... `_t8.cpp`); `RetailT8Animations` (`Extract`, `ExtractArchive`) |
| `AddressScanner/` | `ExeSignatures`: scan the exe file for address patterns (`ScanGame`, `FindT8Executable`), `ApplyScan` (`AddressUpdate.cpp`) |
| `Online/` | `Online` base (shared memory, DLL injection thread), `OnlineT7`, `OnlineT8` (loader commands such as Tag) |
| `Structures/` | Record structs: `Structs_t7.h` (`StructsT7`, `StructsT7_gameAddr`), `Structs_t8.h` (retail `StructsT8`), `Structs_t8cnt.h`, `Structs_ttt2.h`, `Structs_t6.h`, `Structs_t5.h`, `Structs_t5dr.h` |

## `TKMovesets2/Injected/`

Code built into `TKMovesetLoader.dll`, which is injected into the game.

| File | Contents |
| --- | --- |
| `MovesetLoader.cpp` | `MovesetLoader`: base (shared memory, function hooks with PolyHook) |
| `MovesetLoaders/MovesetLoader_t7*.cpp` | `MovesetLoaderT7`: persistent and online play, import inside the game, networking |
| `MovesetLoaders/MovesetLoader_t8.cpp` | `MovesetLoaderT8`: on retail only the Tag mode runs |
| `MovesetLoaders/TagT8.cpp`, `TagT8Grow.cpp` | Retail T8 Practice Tag mode; growing a running battle to 4 slots |
| `SharedMemory/` | `SharedMemBase`, `SharedMemT7`, `SharedMemT8` |
| `InjectionUtils.cpp`, `steamHelper.cpp` | Module helpers; Steam networking helper |

## `TKMovesets2/LocalStorage/`

| File | Contents |
| --- | --- |
| `MovesetStructs.h` | `TKMovesetHeader` (the `.tkmvst` outer header), `TKMovesetProperty` |
| `MovesetFile.cpp` | `MovesetFile`: read and decompress a file; `Validate`; `ConvertToT7`; `ConvertToT8` |
| `LocalStorage.cpp` | `LocalStorage` (a `ThreadedClass`): moveset folder scan, `movesetInfo` list |
| `FileJob.cpp` | `FileJob`, `RunFileJob`: cancellable background convert/save with validation, backup and atomic install |
| `ImportProfile.cpp` | `ImportProfile`: per-file import profile (identity, gender, story flag, modes, ...), `Read`, `WithProfile`, `RequirementOverride`, `StripsProperty`, `ConversionSettings` |
| `MovesetFolders.cpp`, `OpenRequests.cpp` | Moveset folder rules; open requests from Explorer, forwarded to a running instance |

## `TKMovesets2/Resources/` and `TKMovesets2/ResourcesClasses/`

| Path | Contents |
| --- | --- |
| `Resources/game_addresses.ini` | Game addresses and pointer paths per game key, `val:global_addr_version`, the generated conversion rules section. Embedded and downloaded |
| `Resources/*.txt` | Translations (en-US, fr-FR, de-DE, es-ES, ja-JP, ko-KR) |
| `Resources/TKMInterface/` | Editor labels for T7 and retail T8 requirements/properties/parameters, `t8-retail_names.json` (name table) |
| `ResourcesClasses/GameAddressesFile.cpp` | `GameAddressesFile`: load from file or embedded copy (skips the rules section), values and pointer paths, shared-memory copy for the DLL |
| `ResourcesClasses/GameAddressesWrapper.cpp` | `GameAddressesWrapper`: per-game address access |
| `ResourcesClasses/GameStaticData.cpp` | `GameStaticData`: optional display data (character names/codes, move names) |
| `ResourcesClasses/EditorLabel.cpp` | `EditorLabel`: requirement/property labels for the editors |
| `ResourcesClasses/Localization.cpp`, `Settings.cpp`, `Keybinds.cpp` | Translations, settings, keybinds |

## `TKMovesets2/Rules/`

`t7_t8_conversion_rules.ini`: the T7 <-> T8 conversion rules, merged into `game_addresses.ini` by the build
([conversion-rules.md](conversion-rules.md)).

## `TKMovesets2/Updater/`

| File | Contents |
| --- | --- |
| `UpdateCheck.cpp` | Checks the latest GitHub release; downloads a newer `game_addresses.ini` from `master` |
| `UpdateApply.cpp` | Replaces the exe with a downloaded update |
| `Http.cpp` | `Http`: libcurl requests, download install, SHA-256 |

## `TKMovesets2/Utils/`

| File | Contents |
| --- | --- |
| `Compression.cpp` | `CompressionUtils`: codecs none/LZ4/LZMA/Compact+Zstd, file expand and compress |
| `CompactZstd.cpp`, `CompactPanm.cpp` | Compression type 3: compact PANM stream plus Zstd (built with strict floating point) |
| `FileIO.cpp` | `FileIO`: atomic writes (stage then install) |
| `Animations*.cpp`, `AnimationReader.hpp` | `TAnimUtils`: T7-style animation reading, bounded decoding, layouts |
| `helpers.cpp` | `Helpers`: file reading, CRC32, string conversion, alignment |
| `ThreadedClass.cpp` | `ThreadedClass`: worker thread base |
| `MovesetLimits.hpp`, `constants.h`, `debug.hpp`, `GameTypes.h`, `imgui_extras.cpp`, `AppTheme.hpp` | Size budgets, constants (`GAME_ADDRESSES_FILE`, ...), logging, basic types, ImGui helpers, theme |

## `TKMovesets2/External/`

`embedfile.cpp` (the `FileEmbedder` tool).

## `tests/`

| File | Contents |
| --- | --- |
| `MovesetRegression.cpp` | `MovesetRegressionTests` `main` and T7/T5/legacy checks; runs the other suites |
| `T7ToRetailT8Tests.cpp` | T7 -> T8 (standalone and strict) |
| `T8ToT7Tests.cpp` | T8 -> T7, packed PANM decoder, round trip, determinism, conversion rules (`RunConversionRulesTests`) |
| `RetailPanmTests.cpp` | PANM encode/decode and T7 <-> PANM animation round trips |
| `RetailSnapshotTests.cpp` | T8RS snapshots, retail export and import (simulated process) |
| `RetailEditorTests.cpp`, `EditorFormTests.cpp` | Retail and T7 editor forms (compiled into the exe for `editor-smoke`) |
| `EditorMutationTests.cpp`, `EditorWorkflowTests.cpp`, `SpecializedEditorTests.cpp` | Offline editor edits, save and reload |
| `SemanticValidationTests.cpp`, `ExportGraphTests.cpp` | T7 semantic validation; pointer-graph exports |
| `ProcessWriteTests.cpp`, `FileJobTests.cpp` | Write batches and rollback; file jobs |
| `AnimationTests.cpp`, `CompactCompressionTests.cpp` | Animation decoding; Compact+Zstd |
| `AddressScannerTests.cpp`, `GameStaticDataTests.cpp`, `MovesetListSortTests.cpp`, `TagT8GrowTests.cpp` | Address scanner, static data, list sorting, Tag grow |
| `headless_regressions.py`, `retail_record_cli.py` | Headless command-line tests |
| `game_static_data.py`, `editor_reference_data.py`, `native_asset_inspection.py`, `retail_asset_comparison.py` | Tests of the Python tools |

How to run them: [headless.md](headless.md#regression-tests).

## `tools/`

Python scripts (standard library) for generation, research and packaging. None of them is needed at run time.

| Script | Purpose |
| --- | --- |
| `generate_retail_t8_conversion_aliases.py` | Packages the alias catalog `RetailT8Aliases.inc` (`--check` verifies) |
| `import_retail_t8_alias_handoff.py` | Vendors offline alias evidence |
| `generate_t7_t8_cross_votes.py` | Pooled cross-character ID votes `T7ToRetailT8Votes.inc` / `T8ToT7Votes.inc` |
| `generate_editor_reference_data.py`, `import_retail_code_labels.py`, `generate_retail_param_labels.py` | Editor labels and parameter meanings |
| `generate_game_static_data.py` | Retail name data |
| `inspect_t7_assets.py`, `inspect_retail_t8_handlers.py`, `compare_retail_t8_assets.py` | Offline asset and handler inspection |
| `panm_layout.py`, `panm_mutate.py`, `panm_rebuild.py` | PANM layout research |
| `retail_panm_live_regression.py` | In-game PANM import check (needs a running game) |
| `check_legacy_t7_export.py` | Legacy T7 importer harness for `--check-legacy-export` |
| `scrape_moveset_reference_sheets.py` | Archives public moveset reference sheets |
| `package_release.py`, `release-notes/` | Release build, test and packaging |

## Other folders at the root

`research/` (evidence JSON and research projects), `.docs/` (background notes), `docs/` (this reference),
`editor_library/`, `imgui_markdown/`, `steamapi sdk/`.
