// main\main.cpp
/**
 * @file main.cpp
 * @brief Consolidated application managing display, LVGL, SD card, and displaying a grid of animated pet sprites.
 *
 * @note This project is developed for an ESP32 n16r8 using ESP-IDF v5.4 and the LVGL v9.3 graphics library.
 */

// --- NECESSARY HEADERS ---
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>
#include <cassert>
#include <cstring>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <vector> // For std::vector

// ESP-IDF standard components
#include "nvs_flash.h" // For NVS initialization

// Configuration Headers
#include "config/app_config.h"     // For SCREEN_WIDTH, SCREEN_HEIGHT, etc.
#include "config/board_config.h"   // For BUTTON_OK_PIN, BUTTON_CANCEL_PIN, BUTTON_LEFT_PIN

// Controller Headers
#include "controllers/sd_card_manager/sd_card_manager.h"   // For SD card operations
#include "controllers/screen_manager/screen_manager.h"     // For screen initialization and LVGL setup
#include "controllers/lvgl_vfs_driver/lvgl_fs_driver.h"   // For LVGL VFS driver
#include "controllers/button_manager/button_manager.h"     // For button handling

// Model Headers
#include "models/asset_config.h"    // For asset paths
#include "models/pet_data_model.h"  // For PetId enum

// Component Headers
#include "components/memory_monitor_component/memory_monitor_component.h" // For memory display

// extern "C" is necessary to include C headers in a C++ file
extern "C" {
#include "lvgl.h"
}

// --- GLOBAL VARIABLES ---
static const char *TAG_MAIN = "MAIN_APP";
static const char *TAG_MEMORY = "MEMORY";

// Struct to hold state for each animated pet instance
struct AnimatedPet {
    lv_obj_t* img_obj;
    int animation_frame; // Current animation frame (0-3)
    PetId pet_id;        // The specific pet ID for this instance
};

static std::vector<AnimatedPet> s_animated_pets;

// Grid and sprite constants
static const int GRID_COLS = 7;
static const int GRID_ROWS = 7;
// MAX_ANIMATED_PETS defines the maximum number of pets that *can* fit in the grid.
static const int MAX_ANIMATED_PETS = GRID_COLS * GRID_ROWS; // Total number of animated pets (7x7 = 49)
static const lv_coord_t SPRITE_SIZE = 32; // Each sprite is 32x32 pixels

// Variable to control the number of animated pets displayed.
// Change this value to adjust the total number of sprites shown on screen.
static const int s_num_display_pets = 25; // Example: display 25 pets (filling a 5x5 area within the 7x7 grid)

// Variables for animation frame control
static const int MIN_ANIMATION_FRAMES = 2; // Minimum frames to ensure some animation
static const int MAX_ANIMATION_FRAMES = 9; // Max frames (assuming sprite_1.png to sprite_9.png exist)
static int s_active_animation_frames = 4; // Initial number of frames (sprite_1 to sprite_4)

// Variables for button-controlled pet cycling (changing pet type)
// This index will now point to the *next* pet to be changed by BUTTON_RIGHT,
// and after decrementing, to the *current* pet to be reverted by BUTTON_LEFT.
static int s_current_pet_idx_to_change = 0;
static PetId s_current_pet_id_cycle = PetId::PET_0001; // The next PetId to assign (from PET_0001 to PET_0009)

// --- MEMORY LOGGING FUNCTIONS ---
// Function to log ONLY system memory (RAM/PSRAM). It is safe to call at any time.
static void log_system_memory(const char* context) {
    ESP_LOGI(TAG_MEMORY, "--- System Memory Status: %s ---", context);
    size_t total_ram = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t free_ram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t used_ram = total_ram - free_ram;
    float ram_usage_percent = (total_ram > 0) ? ((float)used_ram / total_ram * 100.0f) : 0;
    ESP_LOGI(TAG_MEMORY, "Internal RAM: Used %zu of %zu bytes (%.2f%%)", used_ram, total_ram, ram_usage_percent);

    size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (total_psram > 0) {
        size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t used_psram = total_psram - free_psram;
        float psram_usage_percent = (total_psram > 0) ? ((float)used_psram / total_psram * 100.0f) : 0;
        ESP_LOGI(TAG_MEMORY, "PSRAM:        Used %zu of %zu bytes (%.2f%%)", used_psram, total_psram, psram_usage_percent);
    }
    ESP_LOGI(TAG_MEMORY, "---------------------------------------------");
}

// Function that consolidates ALL memory logs. ONLY should be called AFTER lv_init() has been executed.
static void log_full_memory_status(const char* context) {
    ESP_LOGI(TAG_MEMORY, "--- FULL Memory Status: %s ---", context);
    // 1. Internal RAM and PSRAM information
    size_t total_ram = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t free_ram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t used_ram = total_ram - free_ram;
    float ram_usage_percent = (total_ram > 0) ? ((float)used_ram / total_ram * 100.0f) : 0;
    ESP_LOGI(TAG_MEMORY, "Internal RAM: Used %zu of %zu bytes (%.2f%%)", used_ram, total_ram, ram_usage_percent);
    size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (total_psram > 0) {
        size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        size_t used_psram = total_psram - free_psram;
        float psram_usage_percent = (total_psram > 0) ? ((float)used_psram / total_psram * 100.0f) : 0;
        ESP_LOGI(TAG_MEMORY, "PSRAM:        Used %zu of %zu bytes (%.2f%%)", used_psram, total_psram, psram_usage_percent);
    }
    // 2. LVGL memory pool information
    lv_mem_monitor_t monitor;
    lv_mem_monitor(&monitor);
    ESP_LOGI(TAG_MEMORY, "LVGL Pool:    Used %u of %u bytes (%d%%), Frag: %d%%",
             (unsigned int)(monitor.total_size - monitor.free_size),
             (unsigned int)monitor.total_size,
             monitor.used_pct,
             monitor.frag_pct);
    ESP_LOGI(TAG_MEMORY, "---------------------------------------------");
}

// --- PET ANIMATION LOGIC ---
/**
 * @brief LVGL timer callback to animate multiple pet sprites.
 * It cycles through sprite_1.png, sprite_2.png, ..., sprite_N.png for each pet,
 * where N is `s_active_animation_frames`.
 * @param timer Pointer to the LVGL timer object.
 */
static void animate_pet_sprite_cb(lv_timer_t* timer) {
    if (s_animated_pets.empty()) return;

    for (auto& pet : s_animated_pets) {
        // Increment frame counter and wrap around based on s_active_animation_frames
        pet.animation_frame = (pet.animation_frame + 1) % s_active_animation_frames;

        // Determine the sprite filename based on the current frame (1-indexed)
        char frame_filename[32]; // Sufficiently large buffer
        snprintf(frame_filename, sizeof(frame_filename), "sprite_%d.png", pet.animation_frame + 1);

        // Construct the full path to the sprite on the SD card
        char sprite_path[256];
        snprintf(sprite_path, sizeof(sprite_path), "%s%s%s%s%s%04d/%s",
                 LVGL_VFS_SD_CARD_PREFIX,
                 SD_CARD_ROOT_PATH,
                 ASSETS_BASE_SUBPATH,
                 ASSETS_SPRITES_SUBPATH,
                 SPRITES_PETS_SUBPATH,
                 (int)pet.pet_id,
                 frame_filename);

        lv_image_set_src(pet.img_obj, sprite_path);
    }
}

// --- BUTTON HANDLERS ---
/**
 * @brief Handles the OK button (GPIO_NUM_5) press to increase animation frames.
 */
static void handle_ok_button_press_cb(void* user_data) {
    s_active_animation_frames++;
    if (s_active_animation_frames > MAX_ANIMATION_FRAMES) {
        s_active_animation_frames = MIN_ANIMATION_FRAMES; // Wrap around to MIN
    }
    ESP_LOGI(TAG_MAIN, "Increased animation frames to %d.", s_active_animation_frames);
}

/**
 * @brief Handles the Cancel button (GPIO_NUM_6) press to decrease animation frames.
 */
static void handle_cancel_button_press_cb(void* user_data) {
    s_active_animation_frames--;
    if (s_active_animation_frames < MIN_ANIMATION_FRAMES) {
        s_active_animation_frames = MAX_ANIMATION_FRAMES; // Wrap around to MAX
    }
    ESP_LOGI(TAG_MAIN, "Decreased animation frames to %d.", s_active_animation_frames);
}

/**
 * @brief Handles the Right button (GPIO_NUM_4) press, cycling through pet sprites (species) for the current pet.
 */
static void handle_right_button_press_cb(void* user_data) {
    if (s_animated_pets.empty()) return;

    // Get the pet to change (at the current index)
    AnimatedPet& pet_to_change = s_animated_pets[s_current_pet_idx_to_change];

    // Cycle to the next PetId from PET_0001 to PET_0009
    int next_id_val = (int)s_current_pet_id_cycle + 1;
    if (next_id_val > (int)PetId::PET_0009) {
        next_id_val = (int)PetId::PET_0001; // Wrap back to PET_0001
    }
    s_current_pet_id_cycle = (PetId)next_id_val;

    // Update the pet's ID and reset its animation frame to 0 (sprite_1.png)
    pet_to_change.pet_id = s_current_pet_id_cycle;
    pet_to_change.animation_frame = 0; // Reset animation frame

    // Manually set the image source to reflect the change immediately
    char new_sprite_path[256];
    snprintf(new_sprite_path, sizeof(new_sprite_path), "%s%s%s%s%s%04d/%s",
             LVGL_VFS_SD_CARD_PREFIX,
             SD_CARD_ROOT_PATH,
             ASSETS_BASE_SUBPATH,
             ASSETS_SPRITES_SUBPATH,
             SPRITES_PETS_SUBPATH,
             (int)pet_to_change.pet_id,
             "sprite_1.png"); // Always start with sprite_1.png of the new pet

    lv_image_set_src(pet_to_change.img_obj, new_sprite_path);
    ESP_LOGI(TAG_MAIN, "Changed pet at index %d to PetId %d.", s_current_pet_idx_to_change, (int)s_current_pet_id_cycle);

    // Move to the next pet in the vector for the next BUTTON_RIGHT press
    s_current_pet_idx_to_change = (s_current_pet_idx_to_change + 1) % s_animated_pets.size();
}

/**
 * @brief Handles the Left button (GPIO_NUM_7) press, reverting the previously changed sprite's species to PET_0001.
 */
static void handle_left_button_press_cb(void* user_data) {
    if (s_animated_pets.empty()) {
        ESP_LOGW(TAG_MAIN, "No pets to revert.");
        return;
    }

    // Move the pointer back to the pet that was last modified by BUTTON_RIGHT.
    // If it's currently at 0, it means the *last* change was made to the *last* pet in the list.
    // So, we wrap around to the end of the vector.
    s_current_pet_idx_to_change--;
    if (s_current_pet_idx_to_change < 0) {
        s_current_pet_idx_to_change = s_animated_pets.size() - 1;
    }

    // Now, pet_to_revert refers to the pet that was last modified by the Right button.
    AnimatedPet& pet_to_revert = s_animated_pets[s_current_pet_idx_to_change];

    // Set its ID back to PET_0001
    pet_to_revert.pet_id = PetId::PET_0001;
    pet_to_revert.animation_frame = 0; // Reset animation frame

    // Manually set the image source to reflect the change immediately
    char new_sprite_path[256];
    snprintf(new_sprite_path, sizeof(new_sprite_path), "%s%s%s%s%s%04d/%s",
             LVGL_VFS_SD_CARD_PREFIX,
             SD_CARD_ROOT_PATH,
             ASSETS_BASE_SUBPATH,
             ASSETS_SPRITES_SUBPATH,
             SPRITES_PETS_SUBPATH,
             (int)pet_to_revert.pet_id,
             "sprite_1.png"); // Always start with sprite_1.png

    lv_image_set_src(pet_to_revert.img_obj, new_sprite_path);
    ESP_LOGI(TAG_MAIN, "Reverted pet at index %d to PetId %d.", s_current_pet_idx_to_change, (int)pet_to_revert.pet_id);
}

// --- MAIN APPLICATION ENTRY POINT ---
extern "C" void app_main(void) {
    ESP_LOGI(TAG_MAIN, "--- Starting application ---");

    log_system_memory("Start of app_main");

    // Initialize NVS (Non-Volatile Storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG_MAIN, "NVS initialized.");

    // Initialize SD Card Manager
    if (sd_manager_init() && sd_manager_mount()) {
        ESP_LOGI(TAG_MAIN, "SD Card initialized and mounted successfully.");
    } else {
        ESP_LOGE(TAG_MAIN, "Failed to initialize or mount SD card. Cannot load sprites.");
    }

    // Initialize Screen Manager (which includes LVGL initialization)
    screen_t* screen = screen_init();
    if (!screen) {
        ESP_LOGE(TAG_MAIN, "Failed to initialize screen. Halting.");
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    // Initialize LVGL VFS driver for SD card (using 'S' as drive letter)
    lvgl_fs_driver_init('S');

    // Initialize Button Manager (required before registering handlers)
    button_manager_init();

    log_full_memory_status("After LVGL, Filesystem, and Button Manager Init");

    // Calculate offsets for centering the 7x7 grid
    lv_coord_t total_grid_width = GRID_COLS * SPRITE_SIZE; // 7 * 32 = 224
    lv_coord_t total_grid_height = GRID_ROWS * SPRITE_SIZE; // 7 * 32 = 224

    lv_coord_t offset_x = (SCREEN_WIDTH - total_grid_width) / 2; // (240 - 224) / 2 = 8
    lv_coord_t offset_y = (SCREEN_HEIGHT - total_grid_height) / 2; // (240 - 224) / 2 = 8

    // Create a specified number of pet sprite objects in a grid
    // Iterating up to s_num_display_pets to control how many sprites are shown.
    for (int i = 0; i < s_num_display_pets; ++i) {
        if (i >= MAX_ANIMATED_PETS) {
            ESP_LOGW(TAG_MAIN, "Attempted to create more pets (%d) than MAX_ANIMATED_PETS (%d). Stopping.", s_num_display_pets, MAX_ANIMATED_PETS);
            break; // Stop if we exceed the physical grid capacity
        }

        int row = i / GRID_COLS;
        int col = i % GRID_COLS;

        AnimatedPet new_pet;
        new_pet.img_obj = lv_image_create(lv_screen_active());
        new_pet.animation_frame = 0;
        new_pet.pet_id = PetId::PET_0001; // All instances initially use Bulbasaur's sprites

        if (!new_pet.img_obj) {
            ESP_LOGE(TAG_MAIN, "Failed to create LVGL image object for pet at grid position (%d,%d).", row, col);
            continue;
        }

        // Set the initial sprite for the pet
        char initial_sprite_path[256];
        snprintf(initial_sprite_path, sizeof(initial_sprite_path), "%s%s%s%s%s%04d/%s",
                 LVGL_VFS_SD_CARD_PREFIX,
                 SD_CARD_ROOT_PATH,
                 ASSETS_BASE_SUBPATH,
                 ASSETS_SPRITES_SUBPATH,
                 SPRITES_PETS_SUBPATH,
                 (int)new_pet.pet_id,
                 "sprite_1.png"); // Initial frame

        lv_image_set_src(new_pet.img_obj, initial_sprite_path);
        lv_image_set_antialias(new_pet.img_obj, false); // Disable antialiasing for pixel art

        // Calculate position for the current sprite
        lv_coord_t pos_x = offset_x + col * SPRITE_SIZE;
        lv_coord_t pos_y = offset_y + row * SPRITE_SIZE;
        lv_obj_set_pos(new_pet.img_obj, pos_x, pos_y);

        s_animated_pets.push_back(new_pet);
        ESP_LOGI(TAG_MAIN, "Pet sprite %d created at grid (%d,%d) at screen (%ld, %ld).", i, row, col, pos_x, pos_y);
    }

    // Create and position the Memory Monitor component AFTER all sprites
    // This ensures it's drawn on top of all the pets.
    lv_obj_t* mem_monitor = memory_monitor_create(lv_screen_active());
    // Position it at the bottom-right, with some padding
    lv_obj_align(mem_monitor, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
    ESP_LOGI(TAG_MAIN, "Memory monitor component created and aligned.");

    log_full_memory_status("After loading initial pet sprites and creating UI overlays");

    // Create and start the animation timer (will now update all pets)
    lv_timer_create(animate_pet_sprite_cb, 300, NULL); // 300ms period
    ESP_LOGI(TAG_MAIN, "Pet animation timer started.");

    // Register button handlers
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_right_button_press_cb, false, NULL);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_ok_button_press_cb, false, NULL);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_button_press_cb, false, NULL);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_left_button_press_cb, false, NULL);
    ESP_LOGI(TAG_MAIN, "Button handlers registered.");

    ESP_LOGI(TAG_MAIN, "Entering main LVGL loop.");
    while (true) {
        lv_timer_handler(); // Run LVGL tasks
        vTaskDelay(pdMS_TO_TICKS(10)); // Yield CPU to other tasks
    }
}