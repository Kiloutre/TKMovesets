# Set C++20, mostly for std::format
set(CMAKE_CXX_STANDARD 20)
add_compile_definitions(IMGUI_DEFINE_MATH_OPERATORS)
# This is needed or CMake will expect a .lib file instead of .dll for our shared libs
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Function to compile a folder into a .c
function(create_resources dir output)
    # Create empty output file
    file(WRITE ${output} "")
    # Collect input files
    file(GLOB bins CONFIGURE_DEPENDS ${dir}/*.*)
    # Iterate through input files
    foreach(bin ${bins})
        # Get short filename
        string(REGEX MATCH "([^/]+)$" filename ${bin})
        # Replace filename spaces & extension separator for C compatibility
        string(REGEX REPLACE "\\.| |-" "_" filename ${filename})
        # Read hex data from file
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${bin}")
        file(READ ${bin} filedata HEX)
        # Convert hex data for C compatibility
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," filedata ${filedata})
        # Append data to output file
        file(APPEND ${output} "const unsigned char ${filename}[] = {${filedata}0x00};\nconst unsigned ${filename}_size = sizeof(${filename});\n")
    endforeach()
endfunction()


set(INCLUDE_FOLDERS
                    "GameInteractions"
                    "GameInteractions/GameProcess"

                    "GameSpecific"
                    "GameSpecific/Structures"
                    "GameSpecific/Editors"
                    "GameSpecific/EditorVisuals"
                    "GameSpecific/Extractors"
                    "GameSpecific/Importers"
                    "GameSpecific/Online"
                    "GameSpecific/MovesetConverters"
                    "GameSpecific/AnimExtractors"

                    
                    "GameSpecific/EditorVisuals/EditorVisuals_t7"
                    "GameSpecific/Editors/Editor_t7"
                    
                    "Layouts"
                    "Layouts/Submenus"
                    "Layouts/EditorVisuals"

                    "LocalStorage"
                    "ResourcesClasses"
                    "Utils"

                    "Injected"
                    "Injected/SharedMemory"
                    "Injected/MovesetLoaders"

                    "${CMAKE_SOURCE_DIR}/Sig/include"
                    "${CMAKE_SOURCE_DIR}/steamapi sdk/public/steam"
                    "${CMAKE_SOURCE_DIR}/imgui_markdown"
)

# This will be included in the .exe
set(BINARY_SOURCES
                    # Main sources
                    "main/main.cpp"
                    "main/headless.cpp"
                    "main/run_gui.cpp"
                    "main/arguments_handling.cpp"

                    "Updater/UpdateApply.cpp"
                    "Updater/UpdateCheck.cpp"
                    "Updater/Http.cpp"

                    "GameInteractions/GameInteraction.cpp"
                    "GameInteractions/GameExtract.cpp"
                    "GameInteractions/GameImport.cpp"
                    "GameInteractions/GameSharedMem.cpp"
                    "GameInteractions/GameProcess/GameData.cpp"
                    "GameInteractions/GameProcess/GameProcess.cpp"  
                    "GameInteractions/GameProcess/RemoteWriteBatch.cpp"

                    "LocalStorage/LocalStorage.cpp"
                    "LocalStorage/MovesetFolders.cpp"
                    "LocalStorage/OpenRequests.cpp"

                    # Main Layout
                    "Layouts/MainWindow.cpp"
                    "Layouts/NavigationMenu.cpp"
                    "Layouts/SideMenu.cpp"
                    # Layout: submenus
                    "Layouts/Submenus/Submenu_Extract.cpp"
                    "Layouts/Submenus/T8DiskExtraction.cpp"
                    "Layouts/Submenus/Submenu_Import.cpp"
                    "Layouts/Submenus/Submenu_Edition.cpp"
                    "Layouts/Submenus/Submenu_OnlinePlay.cpp"
                    "Layouts/Submenus/Submenu_PersistentPlay.cpp"
                    "Layouts/Submenus/Submenu_About.cpp"
                    "Layouts/Submenus/Submenu_Addresses.cpp"
                    "Layouts/Submenus/Submenu_Extra.cpp"
                    # Layout: editor window
                    "Layouts/EditorVisuals/EditorVisuals.cpp"
                    "Layouts/EditorVisuals/EditorForm.cpp"
                    "Layouts/EditorVisuals/EditorFormList.cpp"

                    # Helpers and such
                    "Utils/Animations.cpp"
                    "Utils/Animations_T_x64.cpp"
                    "Utils/Animations_Bounded.cpp"
                    "Utils/helpers.cpp"
                    "Utils/imgui_extras.cpp"
                    "Utils/ThreadedClass.cpp"

                    # Resources-accessing
                    "ResourcesClasses/Keybinds.cpp"
                    "ResourcesClasses/Settings.cpp"
                    "ResourcesClasses/Localization.cpp"
                    "ResourcesClasses/EditorLabel.cpp"
                    "ResourcesClasses/GameAddressesFile.cpp"
                    "ResourcesClasses/GameAddressesWrapper.cpp"
)

# Everything game-specific is in here
set(GAMESPECIFIC_SOURCES
                    "GameSpecific/AddressScanner/ExeSignatures.cpp"
                    "GameSpecific/AddressScanner/AddressUpdate.cpp"
                    "GameSpecific/Importers/Importer_t8/RetailPreview.cpp"
                    "GameSpecific/AnimExtractors/AnimExtractor_t8.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t8Retail/EditorVisuals_t8Retail_Browser.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t8Retail/EditorVisuals_t8Retail_CancelGraph.cpp"
                    "GameSpecific/Editors/Editor_t8Retail/Editor_t8Retail.cpp"
                    "GameSpecific/Editors/Editor_t8Retail/RetailEditorData.cpp"
                    "GameSpecific/Editors/Editor_t8Retail/RetailEditorOperations.cpp"
                    "GameSpecific/Editors/Editor_t8Retail/RetailPanmOperations.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t8Retail/EditorVisuals_t8Retail.cpp"
                    "ResourcesClasses/GameStaticData.cpp"
                    "GameSpecific/Editors/RecordEdit.cpp"
                    "GameSpecific/Editors/CancelGraph.cpp"
                    # Animation preview of the move forms (model, poses, OpenGL view)
                    "GameSpecific/AnimationPreview/PreviewModel.cpp"
                    "GameSpecific/AnimationPreview/PreviewClip.cpp"
                    "GameSpecific/AnimationPreview/PreviewDisplacement.cpp"
                    "GameSpecific/AnimationPreview/PreviewHitbox.cpp"
                    "Layouts/EditorVisuals/AnimationPreview.cpp"
                    "LocalStorage/MovesetFile.cpp"
                    "LocalStorage/ImportProfile.cpp"
                    "LocalStorage/FileJob.cpp"
                    # List of games
                    "GameSpecific/Games.cpp"
                    "GameSpecific/BaseAddresses.cpp"
                    # Base classes for our game-specific stuff
                    "GameSpecific/Extractors/Extractor.cpp"
                    "GameSpecific/Importers/Importer.cpp"
                    "GameSpecific/Editors/EditorLogic.cpp"
                    "GameSpecific/Online/Online.cpp"
                    "GameSpecific/MovesetConverters/Aliases.cpp"
                    "GameSpecific/MovesetConverters/MovesetFileConverter.cpp"
                    "GameSpecific/MovesetConverters/T7ToRetailT8.cpp"
                    "GameSpecific/MovesetConverters/T7ToRetailT8Standalone.cpp"
                    "GameSpecific/MovesetConverters/T7AnimToRetailT8.cpp"
                    "GameSpecific/MovesetConverters/RetailT8AnimToT7.cpp"
                    "GameSpecific/MovesetConverters/T8ToT7.cpp"
                    "GameSpecific/MovesetConverters/ConversionRules.cpp"
                    "GameSpecific/Importers/Importer_t8/RetailLiveEdit.cpp"
                    "GameSpecific/MovesetConverters/RetailPanm.cpp"
                    "GameSpecific/AnimExtractors/AnimExtractors.cpp"
                    
                    # Game-specific
                    "GameSpecific/Editors/TEditor.cpp"

                    ## T8
                    "GameSpecific/Online/Online_t8.cpp"
                    "GameSpecific/Extractors/Extractor_t8/Extractor_t8cnt.cpp"
                    "GameSpecific/Extractors/Extractor_t8/Extractor_t8.cpp"
                    "GameSpecific/Extractors/Extractor_t8/RetailSnapshot.cpp"
                    "GameSpecific/Extractors/Extractor_t8/RetailGameData.cpp"
                    "GameSpecific/Extractors/Extractor_t8/RetailDiskMoveset.cpp"
                    "GameSpecific/Extractors/Extractor_t7/T7DiskMoveset.cpp"
                    "GameSpecific/Importers/Importer_t8/RetailImport.cpp"
                    "GameSpecific/Importers/Importer_t8/Importer_t8.cpp"
                    "GameSpecific/Importers/Importer_t8/RetailResourceLoader.cpp"
                    "GameSpecific/Importers/Importer_t8/RetailAnimationArchive.cpp"
                    ### Editor logic
                    "GameSpecific/Editors/Editor_t8/Editor_t8cnt.cpp"
                    "GameSpecific/Editors/Editor_t8/Editor_t8cnt_Forms.cpp"
                    "GameSpecific/Editors/Editor_t8/Editor_t8cnt_Forms_DisplayableMovelist.cpp"
                    "GameSpecific/Editors/Editor_t8/Editor_t8cnt_Creations_DisplayableMovelist.cpp"
                    "GameSpecific/Editors/Editor_t8/Editor_t8cnt_Commands.cpp"
                    "GameSpecific/Editors/Editor_t8/Editor_t8cnt_Creations.cpp"
                    "GameSpecific/Editors/Editor_t8/Editor_t8cnt_Deletion.cpp"
                    "GameSpecific/Editors/Editor_t8/Editor_t8cnt_Duplication.cpp"
                    "GameSpecific/Editors/Editor_t8/Editor_t8cnt_ListCreations.cpp"
                    "GameSpecific/Editors/Editor_t8/Editor_t8cnt_LiveEdition.cpp"
                    "GameSpecific/Editors/Editor_t8/Editor_t8cnt_LiveEdition_Displayable.cpp"
                    "GameSpecific/Editors/Editor_t8/Editor_t8cnt_Utils.cpp"
                    "GameSpecific/Editors/Editor_t8/Editor_t8cnt_References.cpp"

                    ## T7
                    "GameSpecific/Online/Online_t7.cpp"
                    "GameSpecific/Extractors/Extractor_t7/Extractor_t7.cpp"
                    "GameSpecific/Extractors/Extractor_t7/T7ExportSnapshot.cpp"
                    "GameSpecific/Importers/Importer_t7/Importer_t7.cpp"
                    "GameSpecific/Importers/Importer_t7/Importer_t7_Utils.cpp"
                    "GameSpecific/Importers/Importer_t7/Importer_t7_OtherGames.cpp"
                    "GameSpecific/AnimExtractors/AnimExtractor_t7.cpp"
                    ### Editor logic
                    "GameSpecific/Editors/Editor_t7/Editor_t7.cpp"
                    "GameSpecific/Editors/Editor_t7/Editor_t7_Forms.cpp"
                    "GameSpecific/Editors/Editor_t7/Editor_t7_Forms_DisplayableMovelist.cpp"
                    "GameSpecific/Editors/Editor_t7/Editor_t7_Creations_DisplayableMovelist.cpp"
                    "GameSpecific/Editors/Editor_t7/Editor_t7_Commands.cpp"
                    "GameSpecific/Editors/Editor_t7/Editor_t7_Creations.cpp"
                    "GameSpecific/Editors/Editor_t7/Editor_t7_Deletion.cpp"
                    "GameSpecific/Editors/Editor_t7/Editor_t7_Duplication.cpp"
                    "GameSpecific/Editors/Editor_t7/Editor_t7_ListCreations.cpp"
                    "GameSpecific/Editors/Editor_t7/Editor_t7_LiveEdition.cpp"
                    "GameSpecific/Editors/Editor_t7/Editor_t7_LiveEdition_Displayable.cpp"
                    "GameSpecific/Editors/Editor_t7/Editor_t7_Utils.cpp"
                    "GameSpecific/Editors/Editor_t7/Editor_t7_References.cpp"
                    "GameSpecific/Editors/Editor_t7/Editor_t7_CancelGraph.cpp"
                    ### Editor layout
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/EditorVisuals_t7.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/EditorVisuals_t7_Rendering.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Subwindows/TEditor_MoveReferences.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Subwindows/TEditor_CancelReferences.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/TEditorMove.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/TEditorVoiceclip.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/TEditorExtraproperties.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/TEditorCancels.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/TEditorGroupedCancels.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/TEditorRequirements.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/TEditorHitConditions.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/TEditorReactions.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/TEditorPushback.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/TEditorPushbackExtra.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/TEditorMoveStartProperty.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/TEditorMoveEndProperty.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/TEditorInputSequence.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/TEditorInputStruct.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/TEditorProjectile.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/TEditorThrowCamera.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/DisplayableMovelist/TEditorMovelistDisplayable.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/DisplayableMovelist/TEditorMovelistPlayable.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/DisplayableMovelist/TEditorMovelistInput.cpp"
                    "GameSpecific/EditorVisuals/EditorVisuals_t7/Structures/Subwindows/TEditorMove_Animations.cpp"
                    "Layouts/EditorVisuals/CancelGraphView.cpp"

                    # TTT2
                    "GameSpecific/Extractors/Extractor_ttt2/Extractor_ttt2.cpp"
                    "GameSpecific/MovesetConverters/Converter_ttt2_to_t7.cpp"
                    "GameSpecific/MovesetConverters/Converter_ttt2_to_t7_Aliases.cpp"
                    "GameSpecific/AnimExtractors/AnimExtractor_ttt2.cpp"

                    # TREV
                    "GameSpecific/MovesetConverters/Converter_trev_to_t7_Aliases.cpp"

                    # T6
                    "GameSpecific/Extractors/Extractor_t6/Extractor_t6.cpp"
                    "GameSpecific/MovesetConverters/Converter_t6_to_t7.cpp"
                    "GameSpecific/MovesetConverters/Converter_t6_to_t7_Aliases.cpp"
                    "GameSpecific/AnimExtractors/AnimExtractor_t6.cpp"

                    # T5
                    "GameSpecific/Extractors/Extractor_t5/Extractor_t5.cpp"
                    "GameSpecific/MovesetConverters/Converter_t5_to_t7.cpp"
                    "GameSpecific/MovesetConverters/Converter_t5_to_t7_Aliases.cpp"
                    "GameSpecific/AnimExtractors/AnimExtractor_t5.cpp"

                    # T5 DR
                    "GameSpecific/Extractors/Extractor_t5/Extractor_t5dr.cpp"
                    "GameSpecific/AnimExtractors/AnimExtractor_t5dr.cpp"
                    "GameSpecific/MovesetConverters/Converter_t5dr_to_t7_Aliases.cpp"
                    "GameSpecific/MovesetConverters/Converter_t5dr_to_t7.cpp"
)                   

# This will be included in the .dll and should only be related to the injected DLL
set(MOVESETLOADER_SOURCES
    "Injected/MovesetLoader.cpp"


    "Injected/InjectionUtils.cpp"
    "Injected/steamHelper.cpp"

    #T8
    "Injected/MovesetLoaders/MovesetLoader_t8.cpp"
    "Injected/MovesetLoaders/TagT8.cpp"
    "Injected/MovesetLoaders/TagT8Grow.cpp"

    #T7
    "Injected/MovesetLoaders/MovesetLoader_t7.cpp"
    "Injected/MovesetLoaders/MovesetLoader_t7_Importer.cpp"
    "Injected/MovesetLoaders/MovesetLoader_t7_Networking.cpp"

    "ResourcesClasses/GameAddressesFile.cpp"
    "ResourcesClasses/GameAddressesWrapper.cpp"
    "Utils/helpers.cpp"
    
    "GameSpecific/MovesetConverters/Aliases.cpp"

    # Converters
    "GameSpecific/MovesetConverters/Converter_ttt2_to_t7.cpp"
    "GameSpecific/MovesetConverters/Converter_ttt2_to_t7_Aliases.cpp"
    "GameSpecific/Importers/Importer_t7/Importer_t7_Utils.cpp"

    "GameSpecific/MovesetConverters/Converter_trev_to_t7_Aliases.cpp"

    "GameSpecific/MovesetConverters/Converter_t6_to_t7.cpp"
    "GameSpecific/MovesetConverters/Converter_t6_to_t7_Aliases.cpp"

    "GameSpecific/MovesetConverters/Converter_t5_to_t7.cpp"
    "GameSpecific/MovesetConverters/Converter_t5_to_t7_Aliases.cpp"
)

## T7 <-> T8 conversion rules ##
# Edited in Rules/t7_t8_conversion_rules.ini; merged into a marked section of Resources/game_addresses.ini, the one file
# the program embeds, reads and updates (docs/conversion-rules.md). Runs at configure time, before the resources are
# embedded; editing the rules file re-runs it. A rules change must come with a higher val:global_addr_version, or users
# would never download it: the merge refuses otherwise.
set(RULES_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Rules/t7_t8_conversion_rules.ini")
set(ADDRESSES_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Resources/game_addresses.ini")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${RULES_SOURCE}" "${ADDRESSES_SOURCE}")
file(READ "${RULES_SOURCE}" rules_text)
file(READ "${ADDRESSES_SOURCE}" addresses_text)
string(REPLACE "\r\n" "\n" rules_text "${rules_text}")
if (NOT rules_text MATCHES "\n$")
    set(rules_text "${rules_text}\n")
endif()
string(SHA256 rules_hash "${rules_text}")
string(REGEX MATCH "val:global_addr_version[ \t]*=[ \t]*([0-9]+)" _ "${addresses_text}")
set(addr_version "${CMAKE_MATCH_1}")
if (addr_version STREQUAL "")
    message(FATAL_ERROR "game_addresses.ini has no val:global_addr_version")
endif()
set(rules_begin "# BEGIN T7 T8 CONVERSION RULES")
set(rules_end "# END T7 T8 CONVERSION RULES")
string(FIND "${addresses_text}" "${rules_begin}" rules_at)
string(FIND "${addresses_text}" "${rules_end}" rules_end_at)
if (rules_at EQUAL -1)
    # First merge: append the section after the existing addresses (never drop them).
    set(before "${addresses_text}")
    if (NOT before MATCHES "\n$")
        set(before "${before}\n")
    endif()
    set(before "${before}\n")
    set(after "\n")
else()
    if (rules_end_at LESS rules_at)
        message(FATAL_ERROR "game_addresses.ini: conversion rules section has no end marker")
    endif()
    string(SUBSTRING "${addresses_text}" 0 ${rules_at} before)
    string(SUBSTRING "${addresses_text}" ${rules_at} -1 section)
    string(LENGTH "${rules_end}" end_length)
    math(EXPR after_at "${rules_end_at} + ${end_length}")
    string(SUBSTRING "${addresses_text}" ${after_at} -1 after)
    # The previous merge records the rules hash and the address version it shipped with.
    string(REGEX MATCH "hash=([0-9a-f]+) addr_version=([0-9]+)" _ "${section}")
    if (NOT "${CMAKE_MATCH_1}" STREQUAL "${rules_hash}" AND "${CMAKE_MATCH_2}" STREQUAL "${addr_version}")
        message(FATAL_ERROR "Rules/t7_t8_conversion_rules.ini changed: raise val:global_addr_version in Resources/game_addresses.ini (now ${addr_version}) so the updater ships it")
    endif()
endif()
set(merged "${before}${rules_begin} hash=${rules_hash} addr_version=${addr_version}\n# Generated from Rules/t7_t8_conversion_rules.ini by the build: edit that file, not this section.\n${rules_text}${rules_end}${after}")
if (NOT merged STREQUAL addresses_text)
    file(WRITE "${ADDRESSES_SOURCE}" "${merged}")
    message(STATUS "Merged T7 <-> T8 conversion rules into game_addresses.ini")
endif()

## Setup embedded resources ##
set(EMBEDDED_RESOURCES "${CMAKE_CURRENT_BINARY_DIR}/../EmbeddedResources.c")
create_resources("${CMAKE_CURRENT_SOURCE_DIR}/Resources" ${EMBEDDED_RESOURCES})
# Build file embedder .EXE. This file embedder applies lz4 compression and can be used on files generated at compile them
add_library(MovesetIO STATIC "Utils/Compression.cpp" "Utils/FileIO.cpp" "Utils/CompactPanm.cpp" "Utils/CompactZstd.cpp")
# Floating-point prediction is serialized data: preserve binary64 operation order and the FP environment.
if(MSVC)
    set_source_files_properties("Utils/CompactPanm.cpp" PROPERTIES COMPILE_OPTIONS "/fp:strict")
else()
    set_source_files_properties("Utils/CompactPanm.cpp" PROPERTIES COMPILE_OPTIONS "-fno-fast-math;-ffp-contract=off;-frounding-math")
endif()
target_include_directories(MovesetIO PUBLIC ${INCLUDE_FOLDERS})
if (${CMAKE_TKM_BUILD_TYPE} MATCHES "Debug")
    target_compile_definitions(MovesetIO PUBLIC BUILD_TYPE_DEBUG)
endif()
add_executable(FileEmbedder "External/embedfile.cpp")
target_include_directories(FileEmbedder PRIVATE ${INCLUDE_FOLDERS})

# Build injected TKTKMovesetLoader.dll
add_library(TKMovesetLoader SHARED ${MOVESETLOADER_SOURCES} ${EMBEDDED_RESOURCES})
target_include_directories(TKMovesetLoader PRIVATE ${INCLUDE_FOLDERS})
target_compile_definitions(TKMovesetLoader PUBLIC DLL_EXPORTING)
add_dependencies(TKMovesetLoader FileEmbedder)
if (${CMAKE_TKM_BUILD_TYPE} MATCHES "Debug")
    target_compile_definitions(TKMovesetLoader PUBLIC BUILD_TYPE_DEBUG)
endif()


## Embedded TKMovesetLoader for main binary ##
if (NOT ${CMAKE_TKM_BUILD_TYPE} MATCHES "Debug")
    set(MOVESET_LOADER_EMBEDDED "${CMAKE_CURRENT_BINARY_DIR}/../TKMovesetLoader.c")
    set(embed_file_exe_path "${CMAKE_CURRENT_BINARY_DIR}/FileEmbedder.exe")
    # Convert moveset loader into .c that the main program will build
    add_custom_command(OUTPUT ${MOVESET_LOADER_EMBEDDED}
        COMMAND "${embed_file_exe_path}" "${CMAKE_CURRENT_BINARY_DIR}/TKMovesetLoader.dll" "TKMovesetLoader"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/.."
        COMMENT "Converting moveset loader data into .c ..."
        DEPENDS TKMovesetLoader FileEmbedder
    )
else()
    # Don't embed if in debug mode
    set(MOVESET_LOADER_EMBEDDED "")
endif()

## Embedded TKMInterface files (names, editor labels): the exe is standalone and reads them from memory
## (ResourcesClasses/EmbeddedInterface.cpp); only the loader DLL is unpacked. LZMA, same FileEmbedder as the DLL.
file(GLOB TKM_INTERFACE_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/TKMInterface/*.*")
set(TKM_INTERFACE_EMBEDDED "")
set(tkm_interface_decls "")
set(tkm_interface_rows "")
foreach(interface_file ${TKM_INTERFACE_FILES})
    get_filename_component(interface_name "${interface_file}" NAME)
    string(REGEX REPLACE "[\\. \\-]+" "_" interface_symbol "TKMInterface_${interface_name}")
    set(interface_c "${CMAKE_CURRENT_BINARY_DIR}/../${interface_symbol}.c")
    add_custom_command(OUTPUT "${interface_c}"
        COMMAND "${CMAKE_CURRENT_BINARY_DIR}/FileEmbedder.exe" "${interface_file}" "${interface_symbol}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/.."
        COMMENT "Embedding TKMInterface/${interface_name} ..."
        DEPENDS FileEmbedder "${interface_file}"
    )
    list(APPEND TKM_INTERFACE_EMBEDDED "${interface_c}")
    string(APPEND tkm_interface_decls "extern const char ${interface_symbol}[];\nextern const size_t ${interface_symbol}_len;\nextern const size_t ${interface_symbol}_orig_len;\n")
    string(APPEND tkm_interface_rows "    { \"${interface_name}\", ${interface_symbol}, &${interface_symbol}_len, &${interface_symbol}_orig_len },\n")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/../TKMInterfaceTable.c.in"
    "#include <stdlib.h>\n${tkm_interface_decls}\nconst struct TKMEmbeddedFile { const char* name; const char* data; const size_t* len; const size_t* origLen; } TKMInterfaceFiles[] = {\n${tkm_interface_rows}    { 0, 0, 0, 0 }\n};\n")
configure_file("${CMAKE_CURRENT_BINARY_DIR}/../TKMInterfaceTable.c.in" "${CMAKE_CURRENT_BINARY_DIR}/../TKMInterfaceTable.c" COPYONLY)
list(APPEND TKM_INTERFACE_EMBEDDED "${CMAKE_CURRENT_BINARY_DIR}/../TKMInterfaceTable.c")

## Animation preview model (Resources/AnimPreview, from tools/build_preview_mannequin.py). Separate from
## EMBEDDED_RESOURCES so that the injected DLL does not carry it.
set(ANIM_PREVIEW_RESOURCES "${CMAKE_CURRENT_BINARY_DIR}/../AnimPreviewResources.c")
create_resources("${CMAKE_CURRENT_SOURCE_DIR}/Resources/AnimPreview" ${ANIM_PREVIEW_RESOURCES})

# Build lib containing per-game Import/Export movesets functions
add_library(GameSpecific STATIC ${GAMESPECIFIC_SOURCES} ${EMBEDDED_RESOURCES} ${TKM_INTERFACE_EMBEDDED} ${ANIM_PREVIEW_RESOURCES} "ResourcesClasses/EmbeddedInterface.cpp")
target_include_directories(GameSpecific PRIVATE ${INCLUDE_FOLDERS} )
target_compile_definitions(GameSpecific PUBLIC DLL_EXPORTING)
if (${CMAKE_TKM_BUILD_TYPE} MATCHES "Debug")
    target_compile_definitions(GameSpecific PUBLIC BUILD_TYPE_DEBUG)
endif()

# Build program .exe
enable_language("RC") # Icon and more resources later, if needed
if (${CMAKE_TKM_BUILD_TYPE} MATCHES "Debug")
    set(RC_FILE "app_debug.rc")
else()
    set(RC_FILE "app.rc")
endif()
add_executable (TKMovesets2 WIN32 ${BINARY_SOURCES} ${EMBEDDED_RESOURCES} ${MOVESET_LOADER_EMBEDDED} ${RC_FILE})
target_link_libraries(TKMovesets2 PRIVATE ole32 uuid comctl32)
if(DEFINED TKM_EXECUTABLE_NAME)
    set_target_properties(TKMovesets2 PROPERTIES OUTPUT_NAME "${TKM_EXECUTABLE_NAME}")
endif()
if (${CMAKE_TKM_BUILD_TYPE} MATCHES "Debug")
    target_compile_definitions(TKMovesets2 PUBLIC BUILD_TYPE_DEBUG)
endif()

target_include_directories(TKMovesets2 PRIVATE ${INCLUDE_FOLDERS})
add_dependencies(TKMovesets2 GameSpecific)
add_dependencies(TKMovesets2 TKMovesetLoader)
target_link_libraries(TKMovesets2 PRIVATE GameSpecific)

# Copy addresses file to build folder, even if it's already embedded '#
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Resources/game_addresses.ini" "game_addresses.ini" COPYONLY)
# Copies TKMInterface folder containg extra, optional interface data (editor labels, maybe more later down the line)
add_custom_command(TARGET TKMovesets2 PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Resources/TKMInterface" "${CMAKE_CURRENT_BINARY_DIR}/TKMInterface")
# Refresh generated display data even when no C++ source needs relinking.
add_custom_target(GameStaticDataResources
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/TKMInterface"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/Resources/TKMInterface/t8-retail_names.json" "${CMAKE_CURRENT_BINARY_DIR}/TKMInterface/t8-retail_names.json")
add_dependencies(TKMovesets2 GameStaticDataResources)


## Extra dependencies ##
# Link the other libraries that we use #

# OpenGL (Renderer)
find_package(glad CONFIG REQUIRED)
target_link_libraries(TKMovesets2 PRIVATE glad::glad)
target_link_libraries(GameSpecific PUBLIC glad::glad opengl32) # animation preview (Layouts/EditorVisuals/AnimationPreview.cpp)

# GLFW (Renderer)
find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(TKMovesets2 PRIVATE glfw)

# ImGUI (Interface)
find_package(imgui CONFIG REQUIRED)
target_link_libraries(TKMovesets2 PRIVATE imgui::imgui)
target_link_libraries(GameSpecific PRIVATE imgui::imgui) # Only used for the color-defining includes

# Existing storage codecs and the lossless compact PANM + Zstd codec.
## LZ4
find_package(lz4 CONFIG REQUIRED)
## LZMA
find_package(liblzma CONFIG REQUIRED)
target_link_libraries(MovesetIO PUBLIC lz4::lz4 liblzma::liblzma)
target_link_libraries(TKMovesets2 PRIVATE MovesetIO)
target_link_libraries(FileEmbedder PRIVATE MovesetIO)
target_link_libraries(TKMovesetLoader PRIVATE MovesetIO)

# Hook (Polyhook)
find_package(PolyHook_2 CONFIG REQUIRED)
target_link_libraries(TKMovesetLoader PRIVATE PolyHook_2::PolyHook_2)

# HTTP requests
find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(GameSpecific PRIVATE nlohmann_json::nlohmann_json)
# tkdata.bin reader (RetailGameData.cpp): zstd (AES-256-ECB through Windows bcrypt: OpenSSL would embed build paths)
find_package(zstd CONFIG REQUIRED)
find_package(ZLIB REQUIRED) # Tekken 7 tkdata.bin (T7DiskMoveset.cpp)
target_link_libraries(MovesetIO PUBLIC zstd::libzstd ZLIB::ZLIB)
target_link_libraries(GameSpecific PUBLIC zstd::libzstd ZLIB::ZLIB bcrypt)
target_link_libraries(TKMovesets2 PRIVATE CURL::libcurl nlohmann_json::nlohmann_json bcrypt)


if(BUILD_TESTING)
    add_executable(CompactCompressionTests "../tests/CompactCompressionTests.cpp")
    target_link_libraries(CompactCompressionTests PRIVATE MovesetIO nlohmann_json::nlohmann_json)
    add_test(NAME compact-compression COMMAND CompactCompressionTests)
    set_tests_properties(compact-compression PROPERTIES TIMEOUT 120)
    # In-place battle grow (Tag mode) against a fake battle built in this process: no game needed
    add_executable(TagT8GrowTests "../tests/TagT8GrowTests.cpp" "Injected/MovesetLoaders/TagT8Grow.cpp")
    target_compile_definitions(TagT8GrowTests PRIVATE TAGT8GROW_NO_RESOLVER)
    target_include_directories(TagT8GrowTests PRIVATE "Injected/MovesetLoaders")
    add_test(NAME tag-t8-grow COMMAND TagT8GrowTests)
    target_sources(TKMovesets2 PRIVATE "../tests/EditorFormTests.cpp" "../tests/RetailEditorTests.cpp" "../tests/AnimationPreviewTests.cpp")
    target_compile_definitions(TKMovesets2 PRIVATE TKM_EDITOR_FORM_TESTS)
    add_test(NAME headless-editor-smoke COMMAND TKMovesets2 --headless editor-smoke)
    add_executable(MovesetRegressionTests
        "../tests/T7ToRetailT8Tests.cpp"
        "../tests/T8ToT7Tests.cpp"
        "../tests/MovesetListSortTests.cpp"
        "../tests/RetailPanmTests.cpp"
        "../tests/MovesetRegression.cpp"
        "../tests/EditorMutationTests.cpp"
        "../tests/SpecializedEditorTests.cpp"
        "../tests/SemanticValidationTests.cpp"
        "../tests/ExportGraphTests.cpp"
        "../tests/RetailSnapshotTests.cpp"
        "../tests/GameStaticDataTests.cpp"
        "../tests/EditorWorkflowTests.cpp"
        "../tests/FileJobTests.cpp"
        "../tests/ProcessWriteTests.cpp"
        "../tests/AnimationTests.cpp"
        "../tests/AddressScannerTests.cpp"
        "Utils/helpers.cpp"
        "GameInteractions/GameProcess/GameProcess.cpp"
        "GameInteractions/GameProcess/RemoteWriteBatch.cpp"
        "GameSpecific/Importers/Importer_t7/Importer_t7_Utils.cpp")
    target_sources(MovesetRegressionTests PRIVATE
        "LocalStorage/MovesetFolders.cpp"
        "GameInteractions/GameImport.cpp"
        "Injected/MovesetLoaders/MovesetLoader_t7_Importer.cpp"
        "Updater/Http.cpp"
        "Utils/Animations.cpp"
        "Utils/Animations_T_x64.cpp"
                    "Utils/Animations_Bounded.cpp"
        "ResourcesClasses/Localization.cpp"
        "ResourcesClasses/Settings.cpp"
        "ResourcesClasses/Keybinds.cpp"
        "GameInteractions/GameInteraction.cpp"
        "GameInteractions/GameProcess/GameData.cpp"
        "GameSpecific/Importers/Importer.cpp"
        "ResourcesClasses/GameAddressesFile.cpp"
        "ResourcesClasses/GameAddressesWrapper.cpp"
        "Utils/ThreadedClass.cpp"
        ${EMBEDDED_RESOURCES} ${MOVESET_LOADER_EMBEDDED})
    add_dependencies(MovesetRegressionTests GameStaticDataResources)
    target_include_directories(MovesetRegressionTests PRIVATE ${INCLUDE_FOLDERS})
    target_link_libraries(MovesetRegressionTests PRIVATE PolyHook_2::PolyHook_2 CURL::libcurl bcrypt GameSpecific imgui::imgui MovesetIO)
    add_test(NAME moveset-regressions COMMAND MovesetRegressionTests)
    add_test(NAME retail-import-preparation COMMAND MovesetRegressionTests --test-retail-import)
    add_test(NAME game-static-data COMMAND MovesetRegressionTests --test-game-static-data)
    add_test(NAME address-scanner COMMAND MovesetRegressionTests --test-address-scanner)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/headless-smoke")
    add_test(NAME headless-ui-smoke COMMAND TKMovesets2 --headless ui-smoke)
    set_tests_properties(headless-ui-smoke PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/headless-smoke" TIMEOUT 30)
    # Dear ImGui Test Engine (vcpkg imgui[test-engine]) drives the retail editor: popups, tabs, typing, hover.
    add_test(NAME retail-editor-ui COMMAND TKMovesets2 --headless retail-editor-ui-test)
    # Animation preview: mannequin pose and skinning against Blender (tools/build_preview_mannequin.py writes the reference).
    add_test(NAME animation-preview-pose COMMAND TKMovesets2 --headless anim-preview-check "${CMAKE_SOURCE_DIR}/tests/data/anim_preview_blender_reference.json")
    set_tests_properties(retail-editor-ui PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/headless-smoke" TIMEOUT 120)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME game-static-data-generation COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/game_static_data.py")
        add_test(NAME retail-record-conversion COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/retail_record_cli.py" "$<TARGET_FILE:TKMovesets2>" "$<TARGET_FILE:MovesetRegressionTests>")
        add_test(NAME editor-reference-data COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/editor_reference_data.py")
        add_test(NAME native-asset-inspection COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/native_asset_inspection.py")
        add_test(NAME headless-regressions COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/tests/headless_regressions.py" "$<TARGET_FILE:TKMovesets2>" "$<TARGET_FILE:MovesetRegressionTests>")
        set_tests_properties(headless-regressions PROPERTIES TIMEOUT 120)
    endif()
    add_test(NAME headless-help COMMAND TKMovesets2 --headless --help)
    add_test(NAME headless-missing-file COMMAND TKMovesets2 --headless validate "${CMAKE_CURRENT_BINARY_DIR}/missing.tkmvst")
    set_tests_properties(headless-missing-file PROPERTIES WILL_FAIL TRUE TIMEOUT 10)
    add_test(NAME headless-update-addresses-missing-exe COMMAND TKMovesets2 --headless update-addresses-t8 "${CMAKE_CURRENT_BINARY_DIR}/missing.exe" --ini "${CMAKE_CURRENT_BINARY_DIR}/unused.ini")
    set_tests_properties(headless-update-addresses-missing-exe PROPERTIES WILL_FAIL TRUE TIMEOUT 10)
    add_test(NAME headless-games COMMAND TKMovesets2 --headless games)
    set_tests_properties(headless-games PROPERTIES PASS_REGULAR_EXPRESSION "\"key\":\"t8-retail\"" TIMEOUT 10)
    add_test(NAME headless-export-unknown-game COMMAND TKMovesets2 --headless export no-such-game "${CMAKE_CURRENT_BINARY_DIR}/unused" 0)
    set_tests_properties(headless-export-unknown-game PROPERTIES WILL_FAIL TRUE TIMEOUT 10)
endif()
