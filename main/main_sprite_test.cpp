// main\main.cpp
/**
 * @file main.cpp
 * @brief Consolidated application managing display, LVGL, SD card, and displaying a grid of animated pet sprites.
 *        Enhanced with sprite preloading to avoid SD card dependency during runtime.
 *
 * @note This project is developed for an ESP32 n16r8 using ESP-IDF v5.4 and the LVGL v9.4 graphics library.
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
#include <vector>     // For std::vector
#include <unordered_map> // For sprite cache

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
static const char *TAG_SPRITE_LOADER = "SPRITE_LOADER";

// Sprite cache structure to hold preloaded images
struct SpriteData {
    lv_image_dsc_t* img_dsc;  // LVGL image descriptor
    bool is_loaded;           // Flag to track if sprite is successfully loaded
};

// Global sprite cache: maps "petid_frame" -> SpriteData
static std::unordered_map<std::string, SpriteData> s_sprite_cache;

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
static const int s_num_display_pets = 49; // Example: display 25 pets (filling a 5x5 area within the 7x7 grid)

// Variables for animation frame control
static const int MIN_ANIMATION_FRAMES = 1; // Minimum frames to ensure some animation
static const int MAX_ANIMATION_FRAMES = 35; // Max frames (assuming sprite_1.png to sprite_35.png exist)
// Fix: Initialize to MAX_ANIMATION_FRAMES to show full animation by default
static int s_active_animation_frames = MAX_ANIMATION_FRAMES; // Initial number of frames (sprite_1 to sprite_9)
static int s_animation_frames_speed = 100; // Initial speed of frames in ms

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

// --- SPRITE PRELOADING FUNCTIONS ---
/**
 * @brief Creates a cache key for a specific pet and animation frame.
 * @param pet_id The pet ID (PET_0001 to PET_0009)
 * @param frame Animation frame (1 to MAX_ANIMATION_FRAMES)
 * @return String key for the cache map
 */
static std::string create_sprite_cache_key(PetId pet_id, int frame) {
    char key[32];
    snprintf(key, sizeof(key), "%04d_%d", (int)pet_id, frame);
    return std::string(key);
}

/**
 * @brief Loads a single sprite from SD card into memory and creates LVGL image descriptor.
 * @param pet_id The pet ID to load
 * @param frame The animation frame to load (1-indexed)
 * @return Pointer to LVGL image descriptor, or nullptr on failure
 */
static lv_image_dsc_t* load_sprite_into_memory(PetId pet_id, int frame) {
    // Construct the path to the sprite file
    char sprite_path[256];
    snprintf(sprite_path, sizeof(sprite_path), "%s%s%s%s%04d/sprite_%d.png",
             sd_manager_get_mount_point(),
             ASSETS_BASE_SUBPATH,
             ASSETS_SPRITES_SUBPATH,
             SPRITES_PETS_SUBPATH,
             (int)pet_id,
             frame);

    // Read the entire file into memory
    char* file_buffer = nullptr;
    size_t file_size = 0;
    
    if (!sd_manager_read_file(sprite_path, &file_buffer, &file_size)) {
        ESP_LOGE(TAG_SPRITE_LOADER, "Failed to read sprite file: %s", sprite_path);
        return nullptr;
    }

    // Allocate memory for LVGL image descriptor (using PSRAM if available)
    lv_image_dsc_t* img_dsc = (lv_image_dsc_t*)heap_caps_malloc(sizeof(lv_image_dsc_t), 
                                                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!img_dsc) {
        ESP_LOGE(TAG_SPRITE_LOADER, "Failed to allocate memory for image descriptor");
        free(file_buffer);
        return nullptr;
    }

    // Set up the image descriptor for PNG data
    img_dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
    img_dsc->header.flags = LV_IMAGE_FLAGS_ALLOCATED; // Mark as allocated so LVGL can free the 'data' later if needed
    img_dsc->data_size = file_size;
    img_dsc->data = (const uint8_t*)file_buffer; // LVGL will manage this memory

    ESP_LOGI(TAG_SPRITE_LOADER, "Successfully loaded sprite %s (%zu bytes)", sprite_path, file_size);
    return img_dsc;
}

/**
 * @brief Preloads all required sprites into memory cache.
 * This should be called once during initialization while SD card is available.
 * @return true if all sprites were loaded successfully, false otherwise
 */
static bool preload_all_sprites() {
    ESP_LOGI(TAG_SPRITE_LOADER, "Starting sprite preloading process...");
    
    int total_sprites = 0;
    int successful_loads = 0;
    
    // Load sprites for all pet IDs (PET_0001 to PET_0009)
    for (int pet_id_val = (int)PetId::PET_0001; pet_id_val <= (int)PetId::PET_0009; pet_id_val++) {
        PetId pet_id = (PetId)pet_id_val;
        
        // Load all animation frames (1 to MAX_ANIMATION_FRAMES)
        for (int frame = 1; frame <= MAX_ANIMATION_FRAMES; frame++) {
            total_sprites++;
            
            std::string cache_key = create_sprite_cache_key(pet_id, frame);
            
            // Load the sprite into memory
            lv_image_dsc_t* img_dsc = load_sprite_into_memory(pet_id, frame);
            
            // Store in cache
            SpriteData sprite_data;
            sprite_data.img_dsc = img_dsc;
            sprite_data.is_loaded = (img_dsc != nullptr);
            
            s_sprite_cache[cache_key] = sprite_data;
            
            if (sprite_data.is_loaded) {
                successful_loads++;
            } else {
                ESP_LOGW(TAG_SPRITE_LOADER, "Failed to load sprite for pet %04d frame %d", 
                         (int)pet_id, frame);
            }
        }
    }
    
    ESP_LOGI(TAG_SPRITE_LOADER, "Sprite preloading complete: %d/%d sprites loaded successfully", 
             successful_loads, total_sprites);
    
    return (successful_loads == total_sprites);
}

/**
 * @brief Gets a preloaded sprite from the cache.
 * @param pet_id The pet ID
 * @param frame The animation frame (1-indexed)
 * @return Pointer to LVGL image descriptor, or nullptr if not found
 */
static const lv_image_dsc_t* get_cached_sprite(PetId pet_id, int frame) {
    std::string cache_key = create_sprite_cache_key(pet_id, frame);
    
    auto it = s_sprite_cache.find(cache_key);
    if (it != s_sprite_cache.end() && it->second.is_loaded) {
        return it->second.img_dsc;
    }
    
    ESP_LOGE(TAG_SPRITE_LOADER, "Sprite not found in cache: pet %04d frame %d", (int)pet_id, frame);
    return nullptr;
}

/**
 * @brief Cleans up all cached sprites and frees memory.
 * Should be called during application shutdown.
 */
static void cleanup_sprite_cache() {
    ESP_LOGI(TAG_SPRITE_LOADER, "Cleaning up sprite cache...");
    
    for (auto& pair : s_sprite_cache) {
        if (pair.second.is_loaded && pair.second.img_dsc) {
            // Free the image data buffer
            if (pair.second.img_dsc->data) {
                free((void*)pair.second.img_dsc->data);
            }
            // Free the image descriptor
            free(pair.second.img_dsc);
        }
    }
    
    s_sprite_cache.clear();
    ESP_LOGI(TAG_SPRITE_LOADER, "Sprite cache cleanup complete");
}

// --- PET ANIMATION LOGIC ---
/**
 * @brief LVGL timer callback to animate multiple pet sprites.
 * It cycles through sprite_1.png, sprite_2.png, ..., sprite_N.png for each pet,
 * where N is `s_active_animation_frames`.
 * Now uses preloaded sprites from memory instead of SD card.
 * @param timer Pointer to the LVGL timer object.
 */
static void animate_pet_sprite_cb(lv_timer_t* timer) {
    if (s_animated_pets.empty()) return;

    for (auto& pet : s_animated_pets) {
        // Increment frame counter and wrap around based on s_active_animation_frames
        pet.animation_frame = (pet.animation_frame + 1) % s_active_animation_frames;

        // Get the preloaded sprite from cache (frame is 1-indexed)
        const lv_image_dsc_t* cached_sprite = get_cached_sprite(pet.pet_id, pet.animation_frame + 1);
        
        if (cached_sprite) {
            // Set the image source directly to the cached sprite descriptor
            lv_image_set_src(pet.img_obj, cached_sprite);
        } else {
            // Fallback: this should not happen if preloading was successful
            ESP_LOGW(TAG_MAIN, "Missing cached sprite for pet %04d frame %d", 
                     (int)pet.pet_id, pet.animation_frame + 1);
        }
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

    // Set the image source using cached sprite (frame 1)
    const lv_image_dsc_t* cached_sprite = get_cached_sprite(pet_to_change.pet_id, 1);
    if (cached_sprite) {
        lv_image_set_src(pet_to_change.img_obj, cached_sprite);
    }
    
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

    // Set the image source using cached sprite (frame 1)
    const lv_image_dsc_t* cached_sprite = get_cached_sprite(pet_to_revert.pet_id, 1);
    if (cached_sprite) {
        lv_image_set_src(pet_to_revert.img_obj, cached_sprite);
    }
    
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
    bool sd_card_available = false;
    if (sd_manager_init() && sd_manager_mount()) {
        ESP_LOGI(TAG_MAIN, "SD Card initialized and mounted successfully.");
        sd_card_available = true;
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
    // Note: This is still needed for the initial sprite loading
    lvgl_fs_driver_init('S');

    // Initialize Button Manager (required before registering handlers)
    button_manager_init();

    log_full_memory_status("After LVGL, Filesystem, and Button Manager Init");

    // **CRITICAL STEP: Preload all sprites into memory**
    if (sd_card_available) {
        bool preload_success = preload_all_sprites();
        if (!preload_success) {
            ESP_LOGW(TAG_MAIN, "Some sprites failed to preload. Application may have missing animations.");
        } else {
            ESP_LOGI(TAG_MAIN, "All sprites successfully preloaded into memory.");
        }
        
        log_full_memory_status("After sprite preloading");
        
        // Optionally unmount SD card after preloading if you want to test disconnection safety
        // sd_manager_unmount();
        // ESP_LOGI(TAG_MAIN, "SD card unmounted after preloading sprites.");
    }

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

        // Set the initial sprite using cached sprite (frame 1)
        const lv_image_dsc_t* initial_sprite = get_cached_sprite(new_pet.pet_id, 1);
        if (initial_sprite) {
            lv_image_set_src(new_pet.img_obj, initial_sprite);
        } else {
            ESP_LOGE(TAG_MAIN, "Failed to get initial sprite for pet %d", i);
            lv_obj_del(new_pet.img_obj); // Clean up the object if we can't set its sprite
            continue;
        }
        
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

    // Create and start the animation timer (will now update all pets using cached sprites)
    lv_timer_create(animate_pet_sprite_cb, s_animation_frames_speed, NULL);
    ESP_LOGI(TAG_MAIN, "Pet animation timer started with cached sprites.");

    // Register button handlers
    button_manager_register_handler(BUTTON_RIGHT, BUTTON_EVENT_TAP, handle_right_button_press_cb, false, NULL);
    button_manager_register_handler(BUTTON_OK, BUTTON_EVENT_TAP, handle_ok_button_press_cb, false, NULL);
    button_manager_register_handler(BUTTON_CANCEL, BUTTON_EVENT_TAP, handle_cancel_button_press_cb, false, NULL);
    button_manager_register_handler(BUTTON_LEFT, BUTTON_EVENT_TAP, handle_left_button_press_cb, false, NULL);
    ESP_LOGI(TAG_MAIN, "Button handlers registered.");

    // Now you can safely disconnect the SD card - sprites are in memory!
    ESP_LOGI(TAG_MAIN, "Application is now running with preloaded sprites. SD card can be safely disconnected.");
    ESP_LOGI(TAG_MAIN, "Entering main LVGL loop.");
    
    while (true) {
        lv_timer_handler(); // Run LVGL tasks
        vTaskDelay(pdMS_TO_TICKS(10)); // Yield CPU to other tasks
    }

    // Cleanup (this won't normally be reached in the main loop, but good practice)
    cleanup_sprite_cache();
}