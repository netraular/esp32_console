#ifndef ASSET_CONFIG_H
#define ASSET_CONFIG_H

/**
 * @file asset_config.h
 * @brief Centralized configuration for all asset paths, filenames, and filesystem structure.
 * 
 * This file is the single source of truth for locating any asset or data file
 * in the project, both on the external SD card and the internal LittleFS partition.
 */

// --- VFS & LVGL Path Prefixes ---
constexpr const char* LVGL_VFS_SD_CARD_PREFIX = "S:";      // Drive letter for LVGL to access the SD card VFS.
constexpr const char* SD_CARD_ROOT_PATH       = "/sdcard"; // The mount point for the SD card in the VFS.

// =================================================================================
// == INTERNAL FILESYSTEM (LittleFS) STRUCTURE
// =================================================================================

// --- Base Directories ---
constexpr const char* USER_DATA_BASE_PATH = "userdata/"; // For user-generated data and progress.
constexpr const char* GAME_DATA_BASE_PATH = "gamedata/"; // For game content that is large or may need to be updated without a full firmware flash (e.g., long text lists, NPC dialogues, shop inventories).

// --- User Data: Habits Sub-structure ---
constexpr const char* HABITS_SUBPATH             = "habits/";
constexpr const char* HABITS_HISTORY_SUBPATH     = "history/";
constexpr const char* HABITS_CATEGORIES_FILENAME = "categories.csv";
constexpr const char* HABITS_DATA_FILENAME       = "habits.csv";
constexpr const char* HABITS_ID_COUNTER_FILENAME = "id.txt";

// --- User Data: Notifications Sub-structure ---
constexpr const char* NOTIFICATIONS_SUBPATH      = "notifications/";
constexpr const char* NOTIFICATIONS_FILENAME     = "notifications.json";
constexpr const char* NOTIFICATIONS_TEMP_FILENAME = "notifications.json.tmp";

// --- User Data: Recordings & Notes Sub-structure ---
constexpr const char* RECORDINGS_SUBPATH = "recordings/"; // For mic test recordings
constexpr const char* VOICE_NOTES_SUBPATH = "notes/";      // For the voice notes feature

// --- Provisioned/Test Filenames (in LittleFS root) ---
constexpr const char* PROVISIONED_WELCOME_FILENAME = "welcome.txt";


// =================================================================================
// == SD CARD ASSETS STRUCTURE
// =================================================================================

// --- Base Read-Only Asset Directories (relative to SD_CARD_ROOT_PATH) ---
constexpr const char* ASSETS_BASE_SUBPATH    = "/assets/";
constexpr const char* ASSETS_SPRITES_SUBPATH = "sprites/";
constexpr const char* ASSETS_SOUNDS_SUBPATH  = "sounds/";
constexpr const char* ASSETS_FONTS_SUBPATH   = "fonts/";

// --- Sprite Sub-directories ---
constexpr const char* SPRITES_PETS_SUBPATH = "pets/"; // Base folder for all pet sprites. Structure: pets/<ID>/default.png
constexpr const char* SPRITES_EGGS_SUBPATH = "eggs/";
constexpr const char* SPRITES_UI_SUBPATH   = "ui/";

// --- Sound Sub-directories ---
constexpr const char* SOUNDS_EFFECTS_SUBPATH = "effects/";
constexpr const char* SOUNDS_MUSIC_SUBPATH   = "music/";

// --- System-wide UI Sound Filenames (in SD Card's effects subpath) ---
constexpr const char* UI_SOUND_NOTIFICATION = "notification.wav";
constexpr const char* UI_SOUND_SUCCESS      = "bright_earn.wav";
constexpr const char* UI_SOUND_ERROR        = "error.wav";
constexpr const char* UI_SOUND_CLICK        = "click.wav";
constexpr const char* UI_SOUND_TEST         = "test.wav";

// --- Default/Fallback Sprite Filenames ---
constexpr const char* DEFAULT_EGG_SPRITE   = "egg_default.png";

#endif // ASSET_CONFIG_H