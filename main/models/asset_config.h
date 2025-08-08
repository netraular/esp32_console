#ifndef ASSET_CONFIG_H
#define ASSET_CONFIG_H

/**
 * @file asset_config.h
 * @brief Centralized configuration for all game asset paths and system asset filenames.
 * 
 * This file defines the expected directory structure for assets on the SD card,
 * prefixes for different subsystems, and filenames for system-wide UI assets.
 */

// --- VFS & LVGL Path Prefixes ---
constexpr const char* LVGL_VFS_SD_CARD_PREFIX = "S:";      // Drive letter for LVGL to access the SD card VFS.
constexpr const char* SD_CARD_ROOT_PATH       = "/sdcard"; // The mount point for the SD card in the VFS.

// --- Base Asset Directories (relative to SD_CARD_ROOT_PATH) ---
constexpr const char* ASSETS_BASE_SUBPATH    = "/assets/";
constexpr const char* ASSETS_SPRITES_SUBPATH = "sprites/";
constexpr const char* ASSETS_SOUNDS_SUBPATH  = "sounds/";
constexpr const char* ASSETS_FONTS_SUBPATH   = "fonts/";

// --- Sprite Sub-directories ---
constexpr const char* SPRITES_PETS_SUBPATH = "pets/";
constexpr const char* SPRITES_EGGS_SUBPATH = "eggs/";
constexpr const char* SPRITES_UI_SUBPATH   = "ui/";

// --- Sound Sub-directories ---
constexpr const char* SOUNDS_EFFECTS_SUBPATH = "effects/";
constexpr const char* SOUNDS_MUSIC_SUBPATH   = "music/";

// --- System-wide UI Asset Filenames ---
// Filenames for common UI elements not tied to a specific complex data model (e.g., a pet).
constexpr const char* UI_SOUND_NOTIFICATION = "notification.wav";
constexpr const char* UI_SOUND_SUCCESS      = "success.wav";
constexpr const char* UI_SOUND_ERROR        = "error.wav";
constexpr const char* UI_SOUND_CLICK        = "click.wav";


#endif // ASSET_CONFIG_H