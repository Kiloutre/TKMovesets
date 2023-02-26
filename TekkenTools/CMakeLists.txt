
set(INCLUDE_FOLDERS "Main" "Submenus")
set(MAIN_SOURCES "Main/main.cpp" "Main/MainProgram.cpp" "Main/NavigationMenu.cpp")
#set(OTHER_SOURCES "")
set(SUBMENU_SOURCES "Submenus/Submenu_Extract.cpp")
set(ALL_SOURCES ${MAIN_SOURCES} ${SUBMENU_SOURCES})

add_executable (TekkenTools ${ALL_SOURCES})
target_include_directories(TekkenTools PRIVATE ${INCLUDE_FOLDERS})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET TekkenTools PROPERTY CXX_STANDARD 20)
endif()

find_package(glad CONFIG REQUIRED)
target_link_libraries(TekkenTools PRIVATE glad::glad)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(TekkenTools PRIVATE glfw)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(TekkenTools PRIVATE imgui::imgui)