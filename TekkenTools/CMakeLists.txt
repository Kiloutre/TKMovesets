

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

set(MAIN_SOURCES
                    "main.cpp"
                    "Game/GameProcess.cpp"
                    "Game/GameData.cpp"
                    "GameInteractions/GameInteraction.cpp"
                    "GameInteractions/GameExtract.cpp"
                    "GameInteractions/GameImport.cpp"
                    "GameInteractions/Extractors/Extractor.cpp"
                    "GameInteractions/Importers/Importer.cpp"
                    "LocalStorage/LocalStorage.cpp"
)

set(GAME_SPECIFIC_SOURCES
                    "GameInteractions/Extractors/Extractor_t7.cpp"
                    "GameInteractions/Importers/Importer_t7.cpp"
)

set(LAYOUT_SOURCES
                    "Layouts/MainWindow.cpp"
                    "Layouts/NavigationMenu.cpp"
                    "Layouts/StatusBar.cpp"
)

set(SUBMENU_SOURCES
                    "Layouts/Submenus/Submenu_Extract.cpp"
                    "Layouts/Submenus/Submenu_Import.cpp"
                    "Layouts/Submenus/Submenu_OnlinePlay.cpp"
)

set(OTHER_SOURCES
                    "Utils/imgui_extras.cpp"
                    "Utils/helpers.cpp"
                    "Utils/Localization.cpp"
                    "Utils/GameAddressesFile.cpp"
                    "Utils/ThreadedClass.cpp"
)

add_executable (TekkenTools ${MAIN_SOURCES} ${LAYOUT_SOURCES} ${SUBMENU_SOURCES} ${OTHER_SOURCES} ${GAME_SPECIFIC_SOURCES})
target_include_directories(TekkenTools PRIVATE ${INCLUDE_FOLDERS})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET TekkenTools PROPERTY CXX_STANDARD 20)
endif()

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

