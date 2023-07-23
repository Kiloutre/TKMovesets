![Editor screenshot](https://dist.tekkenmods.com/dist-cache/1920/340/mods/2429/thumbnails/4abb6005b885731d-1282x721.jpg)

Tekken mods page:
https://tekkenmods.com/mod/2429/tkmovesets2
Releases page / Download link(s):
https://github.com/Kiloutre/TKMovesets/releases

# TKMovesets2 0.9 - Moveset export / import /edit

This project is a work in progress and is still lacking many features.


Tool aimed at replacing and surpassing the old TekkenMovesetExtractor. Extract movesets, edit them to your liking and re-import them on the characters you want.
The importation is memory based: it does not modify the game files, does not stay after closing the game, and **causes desyncs online if the opponent does not have this tool also running.**

For moveset importation, only T7 is supported. Online play is supported, but requires the opponent to have this tool.
The tool is tailored for the latest Tekken 7 version. It can be modified to work with older versions (*game_addresses.ini*), but won't support them right out of the box.

# Feature set:
## - Extractor
- Extract movesets from memory (game running, characters loaded) (Tekken 7, Tekken Tag2, Tekken Revolution, Tekken 6, Tekken 5)
- Chose whether or not to carry over camera animations, face animations, hands animations...
- Movesets are compressed on disk in order for them not to take too much space

## - Importer
- Import moveset to any character in memory (game running, characters loaded) (Tekken 7)
- Unused movesets are automatically freed from memory, but advanced users may disable that option to play with multiple movesets at once

## - Online play
- 'Persistent play' submenu that allows to choose one/two movesets to apply on the players and to keep even after loading screens
- 'Online play' menu that allows playing with custom movesets without syncing moveset files with the opponent : The moveset files are automatically sent through the steam networking API to the other player. You only need to select your own moveset. Only works if the opponent also has the tool running. Maximum moveset size to be sent/received is 50MB.

## - Editor
- Access movelist, change frame data, create new transitions
- Modify displayed move list in training mode
- Import animations from other characters
- Import a moveset and edit it live: small changes made to the moveset are instantly mirrored in-game
- Keybinds for pre-configured actions such as quick import, more in the future

#### Other
- Support for multiple translations (help wanted!), for now English, Japanese & French are supported


# Developer readme

## Todo lists:

https://github.com/Kiloutre/TKMovesets/projects?query=is%3Aopen

## (Developer) Setting up the project locally

### Infos
This project uses CMake, so if you're new to this, your best way to get started is to install **Visual Studio Community**, and making sure to install the **C++CMake modules**. If you already have Visual Studio but without those modules, you can still install them.

### Dependencies
This project requires external libraries to be downloaded & built to function.

#### [vcpkg](https://vcpkg.io/en/getting-started.html)-obtainable dependencies
Some libraries can be quickly obtained by simply using vcpkg, a package manager that can be tied to Visual Studio and download & build vcpkg dependencies automatically for you.
This is the recommended way of building this project.
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

#### Submodules dependencies

Some dependencies have to be obtained from other git repos. To get them, make sure to also clone the submodules contained within this project.

If cloning with git from the command line, use this command to also download the required submodules:

`git clone --recurse-submodules https://github.com/Kiloutre/TKMovesets.git`

If you already have the repository cloned and want to bring the submodules over, execute those two commands inside of TKMovesets's repository:

`git submodule init ; git submodule update`

If downloading the project manually (.zip or such), you will also have to manually download the dependencies.
If using a GUI program to download this project, there will most likely be an option to bring the submodules over.

#### Steamworks dependency
The steamworks API's headers, which i do use in various parts of this software, does not have a license that would allow me to package it with my own code.
You may obtain the steamworks files using two possible ways:

- From the official steamworks website (that will require creating a steamworks account): https://partner.steamgames.com/downloads/list . Do make sure to download the version that came right before the Tekken build you'll be using the software on, for minimum problems.
- From this link which may go down at any time (v1.56) : https://mega.nz/folder/htpVHSCa#O24Zz0PfxcyejV5ChqDAgw

Make sure the folder is extracted at the root of the project (inside the TKMovesets folder), and that it is named `steamapi sdk`

### Building
- **If you have visual studio installed**: Make sure to close Visual Studio and delete the folders `.vs` and `out` before trying your first build, they may contain bad cache if you attempted building the project with an incomplete environment
- Open the project in visual studio, and if prompted to **Generate** the cmake config, say yes. It probably already started doing it on its own.
- In the list of targets at the top of Visual Studio (next to the green arrow icon), select `TKMovesets2.exe` to build the main tool
