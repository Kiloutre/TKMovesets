#pragma once

#ifndef DLL_EXPORTING
#define DLLCONTENT __declspec(dllexport)
#else
#define DLLCONTENT __declspec(dllimport)
#endif

# define PROGRAM_TITLE           "TekkenTools"
# define PROGRAM_VERSION         "0.1"
# define PROGRAM_WIN_MIN_WIDTH   1280
# define PROGRAM_WIN_MIN_HEIGHT  720
# define PROGRAM_WIN_WIDTH       1280
# define PROGRAM_WIN_HEIGHT      720
# define PROGRAM_DEFAULT_LANG    "en_US"

# define MOVESET_DIRECTORY             "./extracted_chars"
# define MOVESET_FILENAME_EXTENSION    ".tkbin"
# define MOVESET_VERSION_STRING        "0.1"
# define MOVESET_HEADER_STRING_CHARSET "_-+!()[]{} "

# define GAME_INTERACTION_THREAD_SLEEP_MS (200)

# define TEXTBOX_HORIZ_PADDING   15
# define TEXTBOX_VERT_PADDING    10
# define TEXTBOX_BORDER_COLOR    IM_COL32(125, 152, 176, 255)
# define TEXTBOX_BORDER_WARNING  IM_COL32(255, 238, 0, 255)
# define TEXTBOX_BORDER_ERROR    IM_COL32(158, 33, 33, 255)
# define TEXTBOX_BORDER_TIP      IM_COL32(51, 160, 255, 255)