#include "sprite_cache_manager.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "arpa/inet.h" // For ntohl
#include <cstdio>

// Use printf for immediate, unfiltered output during debugging
#define LOG_LOAD(path, size, addr) printf("[CACHE_LOAD] Path: %s, Size: %zu bytes, PSRAM Addr: %p\n", path, size, addr)
#define LOG_FREE(path, addr) printf("[CACHE_FREE] Path: %s, PSRAM Addr: %p\n", path, addr)
#define LOG_REF_INC(path, count) printf("[REF_INC] Path: %s, New RefCount: %d\n", path, count)
#define LOG_REF_DEC(path, count) printf("[REF_DEC] Path: %s, New RefCount: %d\n", path, count)
#define TAG "SPRITE_CACHE"

// --- Helper function to read PNG dimensions ---
static bool get_png_dimensions(const char* path, int32_t* width, int32_t* height) {
    uint8_t buf[24]; // PNG header chunk is within the first 24 bytes
    
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    
    size_t bytes_read = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    if (bytes_read != sizeof(buf)) {
        ESP_LOGE(TAG, "Could not read enough bytes for PNG header from %s", path);
        return false;
    }

    // Check for PNG signature
    if (buf[0] != 0x89 || buf[1] != 0x50 || buf[2] != 0x4E || buf[3] != 0x47 ||
        buf[4] != 0x0D || buf[5] != 0x0A || buf[6] != 0x1A || buf[7] != 0x0A) {
        ESP_LOGE(TAG, "Not a valid PNG file: %s", path);
        return false;
    }

    // Width is at offset 16, height is at offset 20. Both are 4-byte, network-byte-order integers.
    *width = ntohl(*(uint32_t*)(buf + 16));
    *height = ntohl(*(uint32_t*)(buf + 20));

    ESP_LOGD(TAG, "Read dimensions from %s: %ldx%ld", path, (long int)*width, (long int)*height);
    return true;
}


SpriteCacheManager& SpriteCacheManager::get_instance() {
    static SpriteCacheManager instance;
    return instance;
}

SpriteCacheManager::~SpriteCacheManager() {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    printf("Destroying SpriteCacheManager. Releasing all %zu cached sprites.\n", m_cache.size());
    for (auto& pair : m_cache) {
        printf("WARN: Sprite '%s' had ref_count %d at destruction. Force releasing.\n", pair.first.c_str(), pair.second.ref_count);
        free_sprite_data(pair.first, pair.second);
    }
    m_cache.clear();
}

const lv_image_dsc_t* SpriteCacheManager::get_sprite(const std::string& full_path) {
    std::lock_guard<std::mutex> lock(m_cache_mutex);

    auto it = m_cache.find(full_path);
    if (it != m_cache.end()) {
        it->second.ref_count++;
        LOG_REF_INC(full_path.c_str(), it->second.ref_count);
        return it->second.dsc;
    }

    lv_image_dsc_t* new_dsc = load_from_sd(full_path);

    if (new_dsc) {
        CachedSprite new_entry;
        new_entry.dsc = new_dsc;
        new_entry.ref_count = 1;
        m_cache[full_path] = new_entry;
        LOG_REF_INC(full_path.c_str(), 1);
        return new_dsc;
    }

    return nullptr;
}

void SpriteCacheManager::release_sprite(const std::string& full_path) {
    std::lock_guard<std::mutex> lock(m_cache_mutex);

    auto it = m_cache.find(full_path);
    if (it == m_cache.end()) {
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
        m_cache.erase(it);
    }
}

void SpriteCacheManager::release_sprite_group(const std::vector<std::string>& paths) {
    for (const auto& path : paths) {
        release_sprite(path);
    }
}

lv_image_dsc_t* SpriteCacheManager::load_from_sd(const std::string& path) {
    int32_t width = 0, height = 0;
    if (!get_png_dimensions(path.c_str(), &width, &height) || width == 0 || height == 0) {
        ESP_LOGE(TAG, "Failed to get valid PNG dimensions for: %s", path.c_str());
        return nullptr;
    }

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

    // Allocate and zero-initialize memory for the descriptor itself.
    lv_image_dsc_t* img_dsc = (lv_image_dsc_t*)calloc(1, sizeof(lv_image_dsc_t));
    if (!img_dsc) {
        printf("ERROR: Failed to allocate memory for image descriptor\n");
        free(png_data_buffer);
        return nullptr;
    }

    // Configure the descriptor with the pre-read, correct information.
    img_dsc->header.cf = LV_COLOR_FORMAT_UNKNOWN; // Let libpng determine color format
    img_dsc->header.w = width;
    img_dsc->header.h = height;
    img_dsc->data = png_data_buffer;
    img_dsc->data_size = file_size;
    
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