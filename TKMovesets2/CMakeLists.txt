# Set C++20, mostly for std::format
set(CMAKE_CXX_STANDARD 20)
# This is needed or CMake will expect a .lib file instead of .dll for our own libs
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(INCLUDE_FOLDERS
                    "Layouts"
                    "Layouts/Submenus"
                    "Layouts/Editor"
                    "Layouts/Editor/Structures"
                    "Game"
                    "GameInteractions"
                    "GameInteractions/Extractors"
                    "GameInteractions/Importers"
                    "Editors"
                    "LocalStorage"
                    "Utils"
)

# This will be included in the .exe
set(BINARY_SOURCES
                    # Main sources
                    "main.cpp"
                    "Game/GameProcess.cpp"
                    "Game/GameData.cpp"
                    "GameInteractions/GameInteraction.cpp"
                    "GameInteractions/GameExtract.cpp"
                    "GameInteractions/GameImport.cpp"
                    "LocalStorage/LocalStorage.cpp"

                    # Layout
                    "Layouts/MainWindow.cpp"
                    "Layouts/NavigationMenu.cpp"
                    # Layout: editor window
                    "Layouts/Editor/EditorWindow.cpp"
                    "Layouts/Editor/EditorForm.cpp"
                    "Layouts/Editor/EditorFormList.cpp"
                    "Layouts/Editor/Structures/EditorMove.cpp"
                    "Layouts/Editor/Structures/EditorVoiceclip.cpp"
                    "Layouts/Editor/Structures/EditorExtraproperties.cpp"
                    "Layouts/Editor/Structures/EditorCancels.cpp"
                    "Layouts/Editor/Structures/EditorCancelExtra.cpp"
                    "Layouts/Editor/Structures/EditorRequirements.cpp"
                    # Layout: submenus
                    "Layouts/Submenus/Submenu_Extract.cpp"
                    "Layouts/Submenus/Submenu_Import.cpp"
                    "Layouts/Submenus/Submenu_OnlinePlay.cpp"
                    "Layouts/Submenus/Submenu_Edition.cpp"
                    "Layouts/Submenus/Submenu_About.cpp"

                    # Helpers and such
                    "Utils/imgui_extras.cpp"
                    "Utils/helpers.cpp"
                    "Utils/Localization.cpp"
                    "Utils/GameAddressesFile.cpp"
                    "Utils/ThreadedClass.cpp"
)

# This will be included in the .dll and should only be related to extraction/importation/edition (anything game specific)
set(IMPORTEXPORT_SOURCES
                    # Keep a copy of these since Extractor & Importers are reliant on them
                    "Game/GameProcess.cpp"
                    "Game/GameData.cpp"
                    # Same
                    "Utils/helpers.cpp"
                    "Utils/GameAddressesFile.cpp"
                    

                    # List of games
                    "GameInteractions/Games.cpp"
                    # Base classes for our extractor & importers
                    "GameInteractions/Extractors/Extractor.cpp"
                    "GameInteractions/Importers/Importer.cpp"
                    "Editors/Editor.cpp"

                    # Game-specific
                    "GameInteractions/Extractors/Extractor_t7.cpp"
                    "GameInteractions/Importers/Importer_t7.cpp"
                    "Editors/Editor_t7.cpp"
)

## Build ##
# Build .dll containing per-game Import/Export movesets functions, that there won't be 'need to update the whole software when they change#
add_library(ExportImport SHARED ${IMPORTEXPORT_SOURCES})
target_include_directories(ExportImport PRIVATE ${INCLUDE_FOLDERS})
target_compile_definitions(ExportImport PUBLIC DLL_EXPORTING)

# Build program .exe
add_executable (TKMovesets2 ${BINARY_SOURCES})
target_include_directories(TKMovesets2 PRIVATE ${INCLUDE_FOLDERS})
add_dependencies(TKMovesets2 ExportImport)
target_link_libraries(TKMovesets2 PRIVATE ExportImport)


## Ressources ##
# Addresses file #
configure_file("Ressources/game_addresses.txt" "game_addresses.txt" COPYONLY)
# Translations #
add_custom_command(TARGET TKMovesets2 PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Ressources/Translations" "${CMAKE_CURRENT_BINARY_DIR}/Translations")


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