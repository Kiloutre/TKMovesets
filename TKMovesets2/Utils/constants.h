#pragma once

#include "debug.hpp"

#ifdef DLL_EXPORTING
#define DLLCONTENT __declspec(dllexport)
#else
#define DLLCONTENT __declspec(dllimport)
#endif

# define PROGRAM_FILENAME        "TKMovesets2.exe"
# define PROGRAM_TITLE           "TKMovesets - kilo"
# define PROGRAM_VERSION         "0.9.9.3"
# define PROGRAM_WIN_WIDTH       1280
# define PROGRAM_WIN_HEIGHT      720
# define PROGRAM_DEFAULT_LOCALE  "en-US"
# define PROGRAM_DEBUG_LOG_FILE  "tkmovesets_logs.log"
# define UPDATE_REPO_URL         "https://github.com/Kiloutre/TKMovesets"

# define INTERFACE_DATA_DIR      "TKMInterface"
# define GAME_ADDRESSES_FILE     "game_addresses.ini"
# define SETTINGS_FILE           "TKMSettings.ini"

# define UPDATE_TMP_FILENAME     "TKMovesets2Updater"
# define UPDATE_MIN_FILESIZE     500000 // Update files smaller than this are most likely invalid (500KB)
# define HTTP_REQUEST_TIMEOUT    10

// Arbitrary max block size in case something bad happened during extraction and we get a very invalid block
# define MOVESET_MAX_BLOCK_SIZE_MB       (50)

# define MOVESET_EXTRACTED_NAME_PREFIX   "Tkm:"
# define MOVESET_CUSTOM_MOVE_NAME_PREFIX "tkm_"
# define MOVESET_DIRECTORY               "extracted_chars"
# define MOVESET_AUTO_BACKUPDIRECTORY    "editor_auto_backups"
# define EDITOR_LIB_DIRECTORY            "editor_library"
# define MOVESET_FILENAME_EXTENSION      ".tkmvst"
# define MOVESET_TMPFILENAME_EXTENSION   "._tmp_tkmvst"
// We might update extractors/importers without touching the main tool, so this is separated
# define MOVESET_VERSION_STRING          "1.0"
// Accepted chars in header strings are alphanum + those specified here
// Anything else indicate a malformed header : a bad file
# define MOVESET_HEADER_STRING_CHARSET   "_-+!()[]{}.,;: /"

# define MOVESET_LOADER_NAME             "TKMovesetLoader.dll"
# define MOVESET_LOADER_INIT_FUNC        "MovesetLoaderInit"
# define MOVESET_LOADER_RUN_FUNC         "MovesetLoaderRun"
# define MOVESET_LOADER_STOP_FUNC        "MovesetLoaderStop"

// Some unique number that is identical between the two clients
# define MOVESET_LOADER_P2P_CHANNEL      (48697987)

# define ANIMATION_EXTENSION             ".tkanim"
# define ANIMATION_FACE_EXTENSION        ".tkfaceanim"
# define ANIMATION_HAND_EXTENSION        ".tkhandanim"
# define ANIMATION_CAM_EXTENSION         ".tkcamanim"
# define ANIMATION_OTHER_EXTENSION       ".tkotheranim"

# define ONLINE_MOVESET_MAX_SIZE_BYTES   (30000000) // 30MB

# define GAME_FREEING_DELAY_SEC           (10)
# define GAME_INTERACTION_THREAD_SLEEP_MS (200)
# define EDITOR_BACKUP_FREQUENCY_MINUTES  (20)

# define MAX_FPS                       (60.0f)
# define SETTING_VSYNC_BUFFER_KEY      ("vsync_interval")
# define SETTING_VSYNC_BUFFER          (2) // 0 is no vsync, 1 is one screen refresh, etc
# define SETTING_LANG_KEY              ("language_id")
# define SETTING_LANG                  (-1) // -1 means autodetect
# define SETTING_AUTO_UPDATE_KEY       ("auto_update_check")
# define SETTING_AUTO_UPDATE           (true)
# define SETTING_AUTO_ADDR_UPDATE_KEY  ("auto_addresses_update_check")
# define SETTING_AUTO_ADDR_UPDATE      (true)
# define SETTING_EXPORT_OVERWRITE_KEY  ("export.overwrite_identical_name")
# define SETTING_EXPORT_OVERWRITE      (false)
# define SETTING_IMPORT_INSTANT_KEY    ("import.apply_instantly")
# define SETTING_IMPORT_INSTANT        (true)
# define SETTING_FREE_UNUSED_KEY       ("import.free_unused")
# define SETTING_FREE_UNUSED           (true)

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
# define FORM_SPECIAL_BTN        IM_COL32(156, 104, 33, 255)
# define FORM_LIST_NEW_ITEM      IM_COL32(156, 104, 33, 50)

# define GREEN_BTN               IM_COL32(19, 96, 18, 255)
# define RED_BTN                 IM_COL32(158, 47, 25, 255)

# define MOVE_GENERIC            IM_COL32(77, 131, 219, 50)
# define MOVE_CURRENT_GENERIC    IM_COL32(77, 131, 219, 5)
# define MOVE_CUSTOM             IM_COL32(119, 175, 58, 50)
# define MOVE_THROW              IM_COL32(119, 58, 199, 50)
# define MOVE_THROWREACTION      IM_COL32(210, 100, 222, 50)
# define MOVE_ATK                IM_COL32(186, 54, 54, 50)
# define MOVE_OTHERATK           IM_COL32(140, 0, 0, 50)

# define MOVEID_GROUP_CANCEL     IM_COL32(97, 150, 190, 20)
# define MOVEID_INPUT_SEQUENCE   IM_COL32(191, 94, 113, 35)

# define PROPID_THROW_CAM        IM_COL32(191, 94, 113, 35)
# define PROPID_PROJECTILE       IM_COL32(77, 131, 219, 50)

# define FORM_INPUT_HEX          IM_COL32(78, 51, 111, 255)
# define FORM_INPUT_FLOAT        IM_COL32(186, 54, 54, 75)
# define FORM_INPUT_REF          IM_COL32(56, 82, 124, 255)
# define FORM_INPUT_SIGNED       IM_COL32(16, 55, 112, 255)
# define FORM_LABEL_CLICKABLE    FORM_INPUT_REF
# define FORM_ITEM_HIGHLIGHT     IM_COL32(56, 82, 124, 100)

# define MVL_CATEGORY            IM_COL32(77, 131, 219, 50)
# define MVL_COMBO               IM_COL32(140, 177, 237, 50)
# define MVL_DISABLED            IM_COL32(33, 21, 51, 150)

# define MOVESET_LIST_MODIFIED   IM_COL32(19, 96, 18, 30)
# define MOVESET_INVALID         IM_COL32(186, 54, 54, 5)

# define SHARED_MEMORY_BUFSIZE      (4096) // Shared memory containing basic values for moveset loader (addr of moveset, character ID)
# define SHARED_ADDR_MEMORY_BUFSIZE (8192) // Shared memory containing compacted & compressed game_addresses.txt file