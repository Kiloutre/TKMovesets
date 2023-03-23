#pragma once

#ifdef DLL_EXPORTING
#define DLLCONTENT __declspec(dllexport)
#else
#define DLLCONTENT __declspec(dllimport)
#endif

#ifndef DEBUGBUILD_TYPE
#define BUILD_TYPE Release
#endif

# define PROGRAM_TITLE           "TKMovesets - kilo"
# define PROGRAM_VERSION         "0.1"
# define PROGRAM_WIN_WIDTH       1280
# define PROGRAM_WIN_HEIGHT      720
# define PROGRAM_DEFAULT_LANG    "en-US"

# define INTERFACE_DATA_DIR      "Interface"

# define MOVESET_EXTRACTED_NAME_PREFIX   "Tkm:"
# define MOVESET_CUSTOM_MOVE_NAME_PREFIX "tkm_"
# define MOVESET_DIRECTORY               "./extracted_chars"
# define EDITOR_LIB_DIRECTORY            "./editor_library"
# define MOVESET_FILENAME_EXTENSION      ".tkbin"
# define MOVESET_TMPFILENAME_EXTENSION   "._tmp_tkbin"
# define MOVESET_VERSION_STRING          "0.1"  // We might update extractors/importers without touching the main tool, so this is separated
# define MOVESET_HEADER_STRING_CHARSET   "_-+!()[]{}.,;: /" // Accepted chars in header strings are alphanum + those specified here

# define GAME_INTERACTION_THREAD_SLEEP_MS (200)

# define TEXTBOX_HORIZ_PADDING   15
# define TEXTBOX_VERT_PADDING    10
# define TEXTBOX_BORDER_COLOR    IM_COL32(125, 152, 176, 255)
# define TEXTBOX_BORDER_WARNING  IM_COL32(255, 238, 0, 255)
# define TEXTBOX_BORDER_ERROR    IM_COL32(158, 33, 33, 255)
# define TEXTBOX_BORDER_TIP      IM_COL32(51, 160, 255, 255)

# define FORM_SAVE_BTN           IM_COL32(19, 96, 18, 255)
# define FORM_DELETE_BTN         IM_COL32(158, 47, 25, 255)
# define FORM_BG_1               IM_COL32(48, 60, 140, 40)
# define FORM_BG_2               IM_COL32(48, 60, 140, 15)

# define MOVE_GENERIC            IM_COL32(77, 131, 219, 50)
# define MOVE_CUSTOM             IM_COL32(119, 175, 58, 50)
# define MOVE_THROW              IM_COL32(119, 58, 199, 50)
# define MOVE_THROWREACTION      IM_COL32(210, 100, 222, 50)
# define MOVE_ATK                IM_COL32(186, 54, 54, 50)
# define MOVE_OTHERATK           IM_COL32(140, 0, 0, 50)

# define MOVEID_GROUP_CANCEL     IM_COL32(97, 150, 190, 20)
# define MOVEID_INPUT_SEQUENCE   IM_COL32(191, 94, 113, 35)

# define PROPID_THROW_CAM        IM_COL32(191, 94, 113, 35)

# define FORM_INPUT_HEX          IM_COL32(78, 51, 111, 255)
# define FORM_INPUT_FLOAT        IM_COL32(186, 54, 54, 75)
# define FORM_INPUT_REF          IM_COL32(56, 82, 124, 255)
# define FORM_INPUT_SIGNED       IM_COL32(16, 55, 112, 255)
# define FORM_LABEL_CLICKABLE    FORM_INPUT_REF