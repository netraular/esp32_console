#include "sprite_cache_manager.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <cstdio>

// Use printf for immediate, unfiltered output during debugging
#define LOG_LOAD(path, size, addr) printf("[CACHE_LOAD] Path: %s, Size: %zu bytes, PSRAM Addr: %p\n", path, size, addr)
#define LOG_FREE(path, addr) printf("[CACHE_FREE] Path: %s, PSRAM Addr: %p\n", path, addr)
#define LOG_REF_INC(path, count) printf("[REF_INC] Path: %s, New RefCount: %d\n", path, count)
#define LOG_REF_DEC(path, count) printf("[REF_DEC] Path: %s, New RefCount: %d\n", path, count)

SpriteCacheManager& SpriteCacheManager::get_instance() {
    static SpriteCacheManager instance;
    return instance;
}

SpriteCacheManager::~SpriteCacheManager() {
    std::lock_guard<std::mutex> lock(s_cache_mutex);
    printf("Destroying SpriteCacheManager. Releasing all %zu cached sprites.\n", s_cache.size());
    for (auto& pair : s_cache) {
        printf("WARN: Sprite '%s' had ref_count %d at destruction. Force releasing.\n", pair.first.c_str(), pair.second.ref_count);
        free_sprite_data(pair.first, pair.second);
    }
    s_cache.clear();
}

const lv_image_dsc_t* SpriteCacheManager::get_sprite(const std::string& full_path) {
    std::lock_guard<std::mutex> lock(s_cache_mutex);

    auto it = s_cache.find(full_path);
    if (it != s_cache.end()) {
        it->second.ref_count++;
        LOG_REF_INC(full_path.c_str(), it->second.ref_count);
        return it->second.dsc;
    }

    lv_image_dsc_t* new_dsc = load_from_sd(full_path);

    if (new_dsc) {
        CachedSprite new_entry;
        new_entry.dsc = new_dsc;
        new_entry.ref_count = 1;
        s_cache[full_path] = new_entry;
        LOG_REF_INC(full_path.c_str(), 1);
        return new_dsc;
    }

    return nullptr;
}

void SpriteCacheManager::release_sprite(const std::string& full_path) {
    std::lock_guard<std::mutex> lock(s_cache_mutex);

    auto it = s_cache.find(full_path);
    if (it == s_cache.end()) {
        printf("WARN: Attempted to release a non-cached sprite: %s\n", full_path.c_str());
        return;
    }

    it->second.ref_count--;
    LOG_REF_DEC(full_path.c_str(), it->second.ref_count);

    if (it->second.ref_count <= 0) {
        if (it->second.ref_count < 0) {
            printf("ERROR: Ref count for '%s' dropped below zero! This indicates a double-release bug.\n", full_path.c_str());
        }
        free_sprite_data(it->first, it->second);
        s_cache.erase(it);
    }
}

void SpriteCacheManager::release_sprite_group(const std::vector<std::string>& paths) {
    for (const auto& path : paths) {
        release_sprite(path);
    }
}

lv_image_dsc_t* SpriteCacheManager::load_from_sd(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        printf("ERROR: Failed to open file: %s\n", path.c_str());
        return nullptr;
    }

    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size == 0) {
        printf("ERROR: File is empty: %s\n", path.c_str());
        fclose(f);
        return nullptr;
    }

    // Allocate memory in PSRAM to hold the raw PNG file data.
    uint8_t* png_data_buffer = (uint8_t*)heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!png_data_buffer) {
        printf("ERROR: Failed to allocate %zu bytes in PSRAM for sprite '%s'\n", file_size, path.c_str());
        fclose(f);
        return nullptr;
    }
    
    size_t bytes_read = fread(png_data_buffer, 1, file_size, f);
    fclose(f);

    if (bytes_read != file_size) {
        printf("ERROR: Failed to read full file content for '%s'. Expected %zu, got %zu.\n", path.c_str(), file_size, bytes_read);
        free(png_data_buffer);
        return nullptr;
    }

    // Allocate memory for the descriptor itself (this can be in internal RAM).
    lv_image_dsc_t* img_dsc = (lv_image_dsc_t*)malloc(sizeof(lv_image_dsc_t));
    if (!img_dsc) {
        printf("ERROR: Failed to allocate memory for image descriptor\n");
        free(png_data_buffer);
        return nullptr;
    }

    // Configure the descriptor to point to the raw data.
    // LVGL's PNG decoder will see this and handle it.
    img_dsc->data = png_data_buffer;
    img_dsc->data_size = file_size;
    img_dsc->header.cf = LV_COLOR_FORMAT_UNKNOWN; // Let LVGL's decoder figure it out.
    img_dsc->header.w = 0; // Not known until decoded
    img_dsc->header.h = 0; // Not known until decoded
    
    LOG_LOAD(path.c_str(), file_size, png_data_buffer);

    return img_dsc;
}

void SpriteCacheManager::free_sprite_data(const std::string& path, CachedSprite& sprite) {
    if (sprite.dsc) {
        // Crucial step: Tell LVGL to drop its decoded version of this image from its cache.
        lv_image_cache_drop(sprite.dsc);

        if (sprite.dsc->data) {
            // Free the raw PNG data buffer we allocated in PSRAM.
            LOG_FREE(path.c_str(), sprite.dsc->data);
            free((void*)sprite.dsc->data);
        }
        // Free the descriptor struct itself.
        free(sprite.dsc);
        sprite.dsc = nullptr;
    }
}