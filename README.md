# WIP
This project is a work in progress, it is not meant to be used yet

If you have any contribution to bring, feel free, there is room for improvement in many places

# Todo lists:

https://github.com/Kiloutre/TKMovesets/projects?query=is%3Aopen

# (Developer) Setting up the project locally

## Infos
This project uses CMake, so if you're new to this, your best way to get started is to install **Visual Studio Community**, and making sure to install the **C++ modules**. If you already have Visual Studio but without those modules, you can still install them.

## Dependencies
This project requires external libraries to be downloaded & built to function. Thankfully, this can be done quite quickly by downloading [vcpkg](https://vcpkg.io/en/getting-started.html).
- Download the vcpkg by running `git clone https://github.com/Microsoft/vcpkg.git`, or simply [downloading the .zip file](https://github.com/microsoft/vcpkg/archive/refs/heads/master.zip)
- Place the folder anywhere you want in your system, decide on a good place now because moving it later will break stuff
- Run `bootstrap-vcpkg.bat` off the vcpkg folder
- Add the path of vcpkg to your PATH system environment variable
- Also create a system environment variable named `VCPKG_DEFAULT_TRIPLET` and set it to `x64-windows`
- **If you have visual studio installed**: As admin, open a command prompt and execute `vcpkg integrate install`.
-- The command prompt will show you a `DCMAKE_TOOLCHAIN_FILE=xxx` value.
-- Take that `xxx` value and create a new system environment variable named `CMAKE_TOOLCHAIN_FILE` (yes, without the D at the start), and set it to that `xxx` value
-- If you have visual studio running, restart it now.
- **If you do not have visual studio installed:** go into the project folder and run `vcpkg install` to install the dependencies
vcpkg is now installed.

## Building
- **If you have visual studio installed**: Make sure to close Visual Studio and delete the folders `.vs` and `out` before trying your first build, they may contain bad cache if you attempted building the project with an incomplete environment
-- Open the project in visual studio, and if prompted to **Generate** the cmake config, say yes. It probably already started doing it on its own.
-- In the list of targets t the top of Visual Studio (next to the green arrow icon), select `TKMovesets2.exe` to build the main tool
-- There is also a secondary target named `ExportImport.dll` which is the library containing the moveset import/export functions for every supported game
