# Set C++20, mostly for std::format
set(CMAKE_CXX_STANDARD 20)
# This is needed or CMake will expect a .lib file instead of .dll for our own libs
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(INCLUDE_FOLDERS
                    "imgui"
)

# This will be included in the .exe
set(BINARY_SOURCES
                    # Main sources
                    "main_test.cpp"


                    "imgui/imgui.cpp"
                    "imgui/imgui_draw.cpp"
                    "imgui/imgui_impl_opengl3.cpp"
                    "imgui/imgui_impl_glfw.cpp"
                    
                    "imgui/imgui_stdlib.cpp"
                    "imgui/imgui_tables.cpp"
                    "imgui/imgui_widgets.cpp"
)


# Build program .exe
add_executable (TKMovesets2 ${BINARY_SOURCES})
target_include_directories(TKMovesets2 PRIVATE ${INCLUDE_FOLDERS})

## Extra dependencies ##
# Link the other libraries that we use #
find_package(glad CONFIG REQUIRED)
target_link_libraries(TKMovesets2 PRIVATE glad::glad)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(TKMovesets2 PRIVATE glfw)
