# Set C++20, mostly for std::format
set(CMAKE_CXX_STANDARD 20)

set(INCLUDE_FOLDERS
                    "Layouts"
                    "Layouts/Submenus"
                    "Game"
                    "GameInteractions"
                    "GameInteractions/Extractors"
                    "GameInteractions/Importers"
                    "LocalStorage"
                    "Utils"
)

set(BINARY_SOURCES
                    # Main sources
                    "main.cpp"
                    "Game/GameProcess.cpp"
                    "Game/GameData.cpp"
                    "GameInteractions/GameInteraction.cpp"
                    "GameInteractions/GameExtract.cpp"
                    "GameInteractions/GameImport.cpp"
                    "GameInteractions/Extractors/Extractor.cpp"
                    "GameInteractions/Importers/Importer.cpp"
                    "LocalStorage/LocalStorage.cpp"

                    # Game-specific
                    "GameInteractions/Extractors/Extractor_t7.cpp"
                    "GameInteractions/Importers/Importer_t7.cpp"

                    # Layout
                    "Layouts/MainWindow.cpp"
                    "Layouts/NavigationMenu.cpp"
                    "Layouts/StatusBar.cpp"
                    # Layout: submenus
                    "Layouts/Submenus/Submenu_Extract.cpp"
                    "Layouts/Submenus/Submenu_Import.cpp"
                    "Layouts/Submenus/Submenu_OnlinePlay.cpp"

                    # Helpers and such
                    "Utils/imgui_extras.cpp"
                    "Utils/helpers.cpp"
                    "Utils/Localization.cpp"
                    "Utils/GameAddressesFile.cpp"
                    "Utils/ThreadedClass.cpp"
)

set(IMPORTEXPORT_SOURCES
                    # Main sources
                    "Game/GameProcess.cpp"
                    "Game/GameData.cpp"
                    "GameInteractions/Extractors/Extractor.cpp"
                    "GameInteractions/Importers/Importer.cpp"

                    # Game-specific
                    "GameInteractions/Extractors/Extractor_t7.cpp"
                    "GameInteractions/Importers/Importer_t7.cpp"

                    # Helpers and such
                    "Utils/helpers.cpp"
                    "Utils/GameAddressesFile.cpp"
)

# Build .dll containing per-game Import/Export functions, that way no need to update the whole software #
add_library(ExportImport SHARED ${IMPORTEXPORT_SOURCES} )
target_include_directories(ExportImport PRIVATE ${INCLUDE_FOLDERS})
#target_compile_definitions(ExportImport PUBLIC DLL_EXPORTING)

# Build program .exe
add_executable (TekkenTools ${BINARY_SOURCES})
target_include_directories(TekkenTools PRIVATE ${INCLUDE_FOLDERS})
#add_dependencies(TekkenTools ExportImport)
#target_link_libraries(TekkenTools PRIVATE ExportImport)²

# Addresses file #
configure_file("game_addresses.txt" "game_addresses.txt" COPYONLY)

# Translations #
add_custom_command(TARGET TekkenTools PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Translations" "${CMAKE_CURRENT_BINARY_DIR}/Translations")

# Libraries #
find_package(glad CONFIG REQUIRED)
target_link_libraries(TekkenTools PRIVATE glad::glad)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(TekkenTools PRIVATE glfw)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(TekkenTools PRIVATE imgui::imgui)

