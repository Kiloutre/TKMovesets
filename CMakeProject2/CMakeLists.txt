
add_executable (CMakeProject2 "CMakeProject2.cpp" "UseImGui.cpp" "UseImGui.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET CMakeProject2 PROPERTY CXX_STANDARD 20)
endif()

find_package(glad CONFIG REQUIRED)
target_link_libraries(CMakeProject2 PRIVATE glad::glad)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(CMakeProject2 PRIVATE glfw)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(CMakeProject2 PRIVATE imgui::imgui)