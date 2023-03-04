Syntax norms and whatnot to make things consistent and clear throughout the project

// -- Coding practice -- //


// -- File structure -- //
- include groups: first (#include <>) for librairies that aren't ours, then our (#include ".hpp"), then our (#include ".h")
- Class .cpp files should be structured as such: 
  -- Helpers (static or in namespace if used elsewhere) --
  -- Private methods --
  -- Public methods --
- Class .hpp files should be structured as such:
  - Helpers (in namespace)
  - Private members, then private methods
  - Protected members, then protected methods
  - Public members, then public methods
- Functions and methods should appear in the same order in the .cpp file than in the .hpp one
- Related functions should be close to each other
  
// -- Project structure -- //
- .h, .c files for code that is technically C
- .hpp, .cpp file for that uses any C++ feature
  
// -- Commenting -- //
- Static helpers should contain a explanation of their purpose right above their definitions
- Functions, structs, classes, methods and members contain a explanation of their purpose right above their declaration.h
- Generally should add comments explaining a function's code step by step unless it's a really small and obvious one
- Enums should have a comment for each of their entry if there's any useful detail to add

// -- Naming -- //
- Functions/methods should start with an upper case, unless it's a static/function in namespace 
- camelCase for everything
- Constant should be prefixed c_, globals prefixed by g_
Classes:
- Private/Protected members should be prefixed m_
Game structs:
- snake_case for members



// DOC //
https://github.com/ocornut/imgui/blob/v1.89.1/imgui.h#L330
https://github.com/ocornut/imgui/blob/docking/imgui.h
https://traineq.org/ImGuiBundle/emscripten/bin/demo_imgui_bundle.html
https://pthom.github.io/imgui_manual_online/manual/imgui_manual.html
https://github.com/ocornut/imgui/wiki/Glossary
