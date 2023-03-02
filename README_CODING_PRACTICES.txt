Syntax norms and whatnot to make things consistent and clear throughout the project

// -- Coding practice -- //


// -- File structure -- //
- include groups: first (#include <>) for librairies that aren't ours, then our (#include ".hpp"), then our (#include ".h")
- Class .cpp files should be structured as such: 
  -- Static helpers --
  -- Private methods --
  -- Public methods --
- Class .hpp files should be structured as such:
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
- Functions, structs, classes, methods and members contain a explanation of their purpose right above their declaration
- Generally should add comment explaining a function's code step by step unless it's a really small and obvious one
- Enums should have a comment for each of their entry

// -- Naming -- //
- Functions should start with an upper case
- Helpers functions, static or not should start with a lower cases
- camelCase
- Constant should be prefixed c_
- Globals should be suffixed g_
Classes:
- Class names should start with an upper case
- Private class members should be prefixed m_
- Methods should start with an upper case
Game structs:
- snake_case for members