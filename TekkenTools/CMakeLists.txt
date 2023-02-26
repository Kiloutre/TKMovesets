

set(MAIN_SOURCES "main.cpp" "Layouts/MainWindow.cpp" "Layouts/NavigationMenu.cpp" )
set(LAYOUT_SOURCES "Layouts/MainWindow.cpp" "Layouts/NavigationMenu.cpp" )
set(SUBMENU_SOURCES "Layouts/Submenus/Submenu_Extract.cpp")
set(OTHER_SOURCES "Utils/imgui_extras.cpp" "Utils/Localization.cpp")
set(ALL_SOURCES ${MAIN_SOURCES} ${LAYOUT_SOURCES} ${SUBMENU_SOURCES} ${OTHER_SOURCES})

set(INCLUDE_FOLDERS "Layouts" "Layouts/Submenus" "Utils")

add_executable (TekkenTools ${ALL_SOURCES})
target_include_directories(TekkenTools PRIVATE ${INCLUDE_FOLDERS})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET TekkenTools PROPERTY CXX_STANDARD 20)
endif()

# Translations #
add_custom_target(copy-runtime-files ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/Translations ${CMAKE_CURRENT_BINARY_DIR}/Translations
    DEPENDS TekkenTools)

# Libraries #

find_package(glad CONFIG REQUIRED)
target_link_libraries(TekkenTools PRIVATE glad::glad)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(TekkenTools PRIVATE glfw)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(TekkenTools PRIVATE imgui::imgui)

