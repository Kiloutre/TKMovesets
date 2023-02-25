
add_executable (TekkenTools "main.cpp" "MainProgram.cpp" "MainProgram.h" "constants.h" "NavigationMenu.cpp" "NavigationMenu.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET TekkenTools PROPERTY CXX_STANDARD 20)
endif()

find_package(glad CONFIG REQUIRED)
target_link_libraries(TekkenTools PRIVATE glad::glad)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(TekkenTools PRIVATE glfw)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(TekkenTools PRIVATE imgui::imgui)