# Set C++20, mostly for std::format
set(CMAKE_CXX_STANDARD 20)
# This is needed or CMake will expect a .lib file instead of .dll for our own libs
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(INCLUDE_FOLDERS
                    "GameInteractions"
                    "GameInteractions/GameProcess"

                    "GameSpecific"
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
                    "Utils"

                    "Injection"
)

# This will be included in the .exe
set(BINARY_SOURCES
                    # Main sources
                    "main.cpp"

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
                    "Layouts/Submenus/Submenu_OnlinePlay.cpp"
                    "Layouts/Submenus/Submenu_Edition.cpp"
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
                    "Utils/helpers.cpp"
                    "Utils/imgui_extras.cpp"
                    "Utils/ThreadedClass.cpp"
                    "Utils/Localization.cpp"
                    "Utils/EditorLabel.cpp"
                    "Utils/GameAddressesFile.cpp"
)

# This will be included in the .dll and should only be related to extraction/importation/edition (anything game specific)
set(IMPORTEXPORT_SOURCES
                    # Keep a copy of these since Extractor & Importers are reliant on them
                    "GameInteractions/GameProcess/GameProcess.cpp"
                    "GameInteractions/GameProcess/GameData.cpp"
                    "Utils/GameAddressesFile.cpp"
                    "Utils/helpers.cpp"
                    

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
                    "GameSpecific/Editors/Editor_t7/Editor_t7_ListCreations.cpp"
                    "GameSpecific/Editors/Editor_t7/Editor_t7_LiveEdition.cpp"
                    "GameSpecific/Editors/Editor_t7/Editor_t7_LiveEdition_Displayable.cpp"
                    "GameSpecific/Editors/Editor_t7/Editor_t7_Utils.cpp"
)

# This will be included in the .dll and should only be related to the injected DLL
set(MOVESETLOADER_SOURCES
    "Injection/MovesetLoader.cpp"
    "Injection/MovesetLoader_t7.cpp"
)

## Build ##

# Build injected MovesetLoader .dll
add_library(MovesetLoader SHARED ${MOVESETLOADER_SOURCES})
target_include_directories(MovesetLoader PRIVATE ${INCLUDE_FOLDERS})
target_compile_definitions(MovesetLoader PUBLIC DLL_EXPORTING)
if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
target_compile_definitions(MovesetLoader PUBLIC BUILD_TYPE_DEBUG)
endif()

# Build .dll containing per-game Import/Export movesets functions, that there won't be 'need to update the whole software when they change#
add_library(ExportImport SHARED ${IMPORTEXPORT_SOURCES})
target_include_directories(ExportImport PRIVATE ${INCLUDE_FOLDERS})
target_compile_definitions(ExportImport PUBLIC DLL_EXPORTING)
if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
target_compile_definitions(ExportImport PUBLIC BUILD_TYPE_DEBUG)
endif()

# Build program .exe
if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
add_executable (TKMovesets2 ${BINARY_SOURCES})
target_compile_definitions(TKMovesets2 PUBLIC BUILD_TYPE_DEBUG)
else()
add_executable (TKMovesets2 WIN32 ${BINARY_SOURCES})
endif()
target_include_directories(TKMovesets2 PRIVATE ${INCLUDE_FOLDERS})
add_dependencies(TKMovesets2 ExportImport)
add_dependencies(TKMovesets2 MovesetLoader)
target_link_libraries(TKMovesets2 PRIVATE ExportImport)
target_link_libraries(TKMovesets2 PRIVATE MovesetLoader) # This is required for GetModuleHandle() to work, apparently

## Ressources ##
# Addresses file #
configure_file("Ressources/game_addresses.txt" "game_addresses.txt" COPYONLY)
# Interface data #
add_custom_command(TARGET TKMovesets2 PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Ressources/Interface" "${CMAKE_CURRENT_BINARY_DIR}/Interface")
# Debug
add_definitions(/FI"${CMAKE_CURRENT_SOURCE_DIR}/Utils/debug.hpp")

## Extra dependencies ##
# Link the other libraries that we use #
find_package(glad CONFIG REQUIRED)
target_link_libraries(TKMovesets2 PRIVATE glad::glad)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(TKMovesets2 PRIVATE glfw)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(TKMovesets2 PRIVATE imgui::imgui)
target_link_libraries(ExportImport PRIVATE imgui::imgui)

#find_package(liblzma CONFIG REQUIRED)
#target_link_libraries(TKMovesets2 PRIVATE liblzma::liblzma)
#target_link_libraries(ExportImport PRIVATE liblzma::liblzma)