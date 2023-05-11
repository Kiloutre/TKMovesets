# Set C++20, mostly for std::format
set(CMAKE_CXX_STANDARD 20)
# This is needed or CMake will expect a .lib file instead of .dll for our shared libs
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Function to compile a folder into a .c
function(create_resources dir output)
    # Create empty output file
    file(WRITE ${output} "")
    # Collect input files
    file(GLOB bins ${dir}/*.*)
    # Iterate through input files
    foreach(bin ${bins})
        # Get short filename
        string(REGEX MATCH "([^/]+)$" filename ${bin})
        # Replace filename spaces & extension separator for C compatibility
        string(REGEX REPLACE "\\.| |-" "_" filename ${filename})
        # Read hex data from file
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
                    "GameSpecific/Editors/Editor_t7"
                    "GameSpecific/Extractors"
                    "GameSpecific/Importers"
                    "GameSpecific/Online"
                    
                    "Layouts"
                    "Layouts/Submenus"
                    "Layouts/Editor"
                    "Layouts/Editor/EditorWindow"
                    "Layouts/Editor/TStructures"
                    "Layouts/Editor/TStructures/Subwindows"
                    "Layouts/Editor/TStructures/DisplayableMovelist"

                    "LocalStorage"
                    "ResourcesClasses"
                    "Utils"

                    "Injected"
                    "Injected/SharedMemory"
                    "Injected/MovesetLoaders"

                    "${CMAKE_SOURCE_DIR}/Sig/include"
                    "${CMAKE_SOURCE_DIR}/steamapi sdk/public/steam"
)

# This will be included in the .exe
set(BINARY_SOURCES
                    # Main sources
                    "main.cpp"

                    "Updater/UpdateApply.cpp"
                    "Updater/UpdateCheck.cpp"

                    "GameInteractions/GameInteraction.cpp"
                    "GameInteractions/GameExtract.cpp"
                    "GameInteractions/GameImport.cpp"
                    "GameInteractions/GameSharedMem.cpp"
                    "GameInteractions/GameProcess/GameData.cpp"
                    "GameInteractions/GameProcess/GameProcess.cpp"  

                    "LocalStorage/LocalStorage.cpp"

                    # Main Layout
                    "Layouts/MainWindow.cpp"
                    "Layouts/NavigationMenu.cpp"
                    # Layout: submenus
                    "Layouts/Submenus/Submenu_Extract.cpp"
                    "Layouts/Submenus/Submenu_Import.cpp"
                    "Layouts/Submenus/Submenu_Edition.cpp"
                    "Layouts/Submenus/Submenu_OnlinePlay.cpp"
                    "Layouts/Submenus/Submenu_PersistentPlay.cpp"
                    "Layouts/Submenus/Submenu_About.cpp"
                    # Layout: editor window
                    "Layouts/Editor/EditorWindow/EditorWindow.cpp"
                    "Layouts/Editor/EditorWindow/EditorWindow_Rendering.cpp"
                    "Layouts/Editor/EditorForm.cpp"
                    "Layouts/Editor/EditorFormList.cpp"
                    # Layout: Structures
                    "Layouts/Editor/TStructures/EditorMove.cpp"
                    "Layouts/Editor/TStructures/EditorVoiceclip.cpp"
                    "Layouts/Editor/TStructures/EditorExtraproperties.cpp"
                    "Layouts/Editor/TStructures/EditorCancels.cpp"
                    "Layouts/Editor/TStructures/EditorCancelExtra.cpp"
                    "Layouts/Editor/TStructures/EditorGroupedCancels.cpp"
                    "Layouts/Editor/TStructures/EditorRequirements.cpp"
                    "Layouts/Editor/TStructures/EditorHitConditions.cpp"
                    "Layouts/Editor/TStructures/EditorReactions.cpp"
                    "Layouts/Editor/TStructures/EditorPushback.cpp"
                    "Layouts/Editor/TStructures/EditorPushbackExtra.cpp"
                    "Layouts/Editor/TStructures/EditorMoveStartProperty.cpp"
                    "Layouts/Editor/TStructures/EditorMoveEndProperty.cpp"
                    "Layouts/Editor/TStructures/EditorInputSequence.cpp"
                    "Layouts/Editor/TStructures/EditorInputStruct.cpp"
                    "Layouts/Editor/TStructures/EditorProjectile.cpp"
                    "Layouts/Editor/TStructures/EditorThrowCamera.cpp"
                    "Layouts/Editor/TStructures/EditorCameraData.cpp"
                    # Layout: editor window: displayable movelist
                    "Layouts/Editor/TStructures/DisplayableMovelist/EditorMovelistDisplayable.cpp"
                    "Layouts/Editor/TStructures/DisplayableMovelist/EditorMovelistPlayable.cpp"
                    "Layouts/Editor/TStructures/DisplayableMovelist/EditorMovelistInput.cpp"
                    # Layout: editor subwindows
                    "Layouts/Editor/TStructures/Subwindows/EditorMove_Animations.cpp"

                    # Helpers and such
                    "Utils/Animations.cpp"
                    "Utils/helpers.cpp"
                    "Utils/imgui_extras.cpp"
                    "Utils/ThreadedClass.cpp"
                    "Utils/Compression.cpp"

                    # Resources-accessing
                    "ResourcesClasses/Localization.cpp"
                    "ResourcesClasses/EditorLabel.cpp"
                    "ResourcesClasses/GameAddressesFile.cpp"
                    "ResourcesClasses/GameAddressesWrapper.cpp"
)

# Everything game-specific is in here
set(GAMESPECIFIC_SOURCES
                    # List of games
                    "GameSpecific/Games.cpp"
                    # Base classes for our game-specific stuff
                    "GameSpecific/Extractors/Extractor.cpp"
                    "GameSpecific/Importers/Importer.cpp"
                    "GameSpecific/Editors/Editor.cpp"
                    "GameSpecific/Online/Online.cpp"
                    
                    # Game-specific
                    ## T7
                    "GameSpecific/Online/Online_t7.cpp"
                    "GameSpecific/Extractors/Extractor_t7.cpp"
                    "GameSpecific/Importers/Importer_t7.cpp"
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
)

# This will be included in the .dll and should only be related to the injected DLL
set(MOVESETLOADER_SOURCES
    "Injected/MovesetLoader.cpp"
    "Injected/MovesetLoaders/MovesetLoader_t7.cpp"
    "Injected/MovesetLoaders/MovesetLoader_t7_Importer.cpp"
    "Injected/MovesetLoaders/MovesetLoader_t7_Networking.cpp"
    "Injected/InjectionUtils.cpp"
    "Injected/steamHelper.cpp"

    "ResourcesClasses/GameAddressesFile.cpp"
    "ResourcesClasses/GameAddressesWrapper.cpp"
    "Utils/helpers.cpp"
    "Utils/Compression.cpp"
)

## Setup embedded resources ##
set(EMBEDDED_RESOURCES "${CMAKE_CURRENT_BINARY_DIR}/../EmbeddedResources.c")
create_resources("${CMAKE_CURRENT_SOURCE_DIR}/Resources" ${EMBEDDED_RESOURCES})
# Build file embedder .EXE. This file embedder applies lz4 compression and can be used on files generated at compile them
add_executable (FileEmbedder "External/embedfile.cpp")

# Build injected TKTKMovesetLoader.dll
add_library(TKMovesetLoader SHARED ${MOVESETLOADER_SOURCES} ${EMBEDDED_RESOURCES})
target_include_directories(TKMovesetLoader PRIVATE ${INCLUDE_FOLDERS})
target_compile_definitions(TKMovesetLoader PUBLIC DLL_EXPORTING)
add_dependencies(TKMovesetLoader FileEmbedder)
if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    target_compile_definitions(TKMovesetLoader PUBLIC BUILD_TYPE_DEBUG)
endif()

## Embedded TKMovesetLoader for main binary ##
set(MOVESET_LOADER_EMBEDDED "${CMAKE_CURRENT_BINARY_DIR}/../TKMovesetLoader.c")
set(embed_file_exe_path "${CMAKE_CURRENT_BINARY_DIR}/FileEmbedder.exe")

# Convert moveset loader into .c that the main program will build
add_custom_command(OUTPUT ${MOVESET_LOADER_EMBEDDED}
    COMMAND "${embed_file_exe_path}" "${CMAKE_CURRENT_BINARY_DIR}/TKMovesetLoader.dll" "TKMovesetLoader"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/.."
    COMMENT "Converting moveset loader data into .c ..."
    DEPENDS TKMovesetLoader FileEmbedder
)

# Build lib containing per-game Import/Export movesets functions
add_library(GameSpecific STATIC ${GAMESPECIFIC_SOURCES} ${EMBEDDED_RESOURCES})
target_include_directories(GameSpecific PRIVATE ${INCLUDE_FOLDERS} )
target_compile_definitions(GameSpecific PUBLIC DLL_EXPORTING)
if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    target_compile_definitions(GameSpecific PUBLIC BUILD_TYPE_DEBUG)
endif()

# Build program .exe
enable_language("RC") # Icon and more resources later, if needed
add_executable (TKMovesets2 WIN32 ${BINARY_SOURCES} ${EMBEDDED_RESOURCES} ${MOVESET_LOADER_EMBEDDED} "app.rc")
if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
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

## Extra dependencies ##
# Link the other libraries that we use #

# OpenGL (Renderer)
find_package(glad CONFIG REQUIRED)
target_link_libraries(TKMovesets2 PRIVATE glad::glad)

# GLFW (Renderer)
find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(TKMovesets2 PRIVATE glfw)

# ImGUI (Interface)
find_package(imgui CONFIG REQUIRED)
target_link_libraries(TKMovesets2 PRIVATE imgui::imgui)
target_link_libraries(GameSpecific PRIVATE imgui::imgui) # Only used for the color-defining includes

# Compression. I tried LZMA but it was too slow even on the fastest preset, so we're using LZ4 for now
find_package(lz4 CONFIG REQUIRED)
target_link_libraries(TKMovesets2 PRIVATE lz4::lz4)
target_link_libraries(FileEmbedder PRIVATE lz4::lz4)
target_link_libraries(TKMovesetLoader PRIVATE lz4::lz4)

# Hook (Polyhook)
find_package(PolyHook_2 CONFIG REQUIRED)
target_link_libraries(TKMovesetLoader PRIVATE PolyHook_2::PolyHook_2)

# HTTP requests
find_package(unofficial-curlpp CONFIG REQUIRED)
target_link_libraries(TKMovesets2 PRIVATE unofficial::curlpp::curlpp)
