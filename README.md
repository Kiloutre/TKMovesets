![Editor screenshot](https://dist.tekkenmods.com/dist-cache/1920/340/mods/2429/thumbnails/4abb6005b885731d-1282x721.jpg)

Tekken mods page:
https://tekkenmods.com/mod/2429/tkmovesets2


# WIP

This project is a work in progress and is still lacking critical features.

If you have any contribution to bring, feel free, there is room for improvement in many places

# (Developer) Todo lists:

https://github.com/Kiloutre/TKMovesets/projects?query=is%3Aopen

# (Developer) Setting up the project locally

## Infos
This project uses CMake, so if you're new to this, your best way to get started is to install **Visual Studio Community**, and making sure to install the **C++CMake modules**. If you already have Visual Studio but without those modules, you can still install them.

## Dependencies
This project requires external libraries to be downloaded & built to function.

### [vcpkg](https://vcpkg.io/en/getting-started.html)-obtainable dependencies
- Download vcpkg by running `git clone https://github.com/Microsoft/vcpkg.git`, or simply [downloading the .zip file](https://github.com/microsoft/vcpkg/archive/refs/heads/master.zip)
- Place the folder anywhere you want in your system, decide on a good place now because moving it later will break things.
- Run `bootstrap-vcpkg.bat` off the vcpkg folder
- Add the path of vcpkg to your PATH system environment variable
- Also create a system environment variable named `VCPKG_DEFAULT_TRIPLET` and set it to `x64-windows`
- **If you have visual studio installed**: As an admin, open a command prompt and execute `vcpkg integrate install`.
  - The command prompt will show you a `DCMAKE_TOOLCHAIN_FILE=xxx` value.
  - Take that `xxx` value and create a new system environment variable named `CMAKE_TOOLCHAIN_FILE` (yes, without the D at the start), and set it to that `xxx` value
- If you have visual studio running, restart it now.
- **If you do not have visual studio installed:** Go into the project folder and run `vcpkg install` to install the dependencies

vcpkg is now installed.

### Submodules dependencies

If cloning with git from the command line, use this command to also download the required submodules:

`git clone --recurse-submodules https://github.com/Kiloutre/TKMovesets.git`

If you already have the repository cloned and want to bring the submodules over, execute those two commands inside of TKMovesets's repository:

`git submodule init ; git submodule update`

If downloading the project manually (.zip or such), you will also have to manually download the dependencies.
If using a GUI program to download this project, there will most likely be an option to bring the submodules over.

### Steamworks dependency
The steamworks API's headers, which i do use in various parts of this software, does not have a license that would allow me to package it with my own code.
You may obtain the steamworks files using two possible ways:

- From the official steamworks website (that will require craeting a steamworks account): https://partner.steamgames.com/downloads/list . Do make sure to download the version that came right before the Tekken build you'll be using the software on, for minimum problems.
- From this link which may go down at any time (v1.56) : https://mega.nz/folder/htpVHSCa#O24Zz0PfxcyejV5ChqDAgw

## Building
- **If you have visual studio installed**: Make sure to close Visual Studio and delete the folders `.vs` and `out` before trying your first build, they may contain bad cache if you attempted building the project with an incomplete environment
- Open the project in visual studio, and if prompted to **Generate** the cmake config, say yes. It probably already started doing it on its own.
- In the list of targets at the top of Visual Studio (next to the green arrow icon), select `TKMovesets2.exe` to build the main tool
