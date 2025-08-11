#include "sprite_cache_manager.h"
#include "controllers/sd_card_manager/sd_card_manager.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char* TAG = "SPRITE_CACHE";

SpriteCacheManager& SpriteCacheManager::get_instance() {
    static SpriteCacheManager instance;
    return instance;
}

SpriteCacheManager::~SpriteCacheManager() {
    std::lock_guard<std::mutex> lock(s_cache_mutex);
    ESP_LOGI(TAG, "Destroying SpriteCacheManager. Releasing all %zu cached sprites.", s_cache.size());
    for (auto& pair : s_cache) {
        ESP_LOGW(TAG, "Sprite '%s' had ref_count %d at destruction. Force releasing.", pair.first.c_str(), pair.second.ref_count);
        free_sprite_data(pair.second);
    }
    s_cache.clear();
}

const lv_image_dsc_t* SpriteCacheManager::get_sprite(const std::string& full_path) {
    std::lock_guard<std::mutex> lock(s_cache_mutex);

    auto it = s_cache.find(full_path);
    if (it != s_cache.end()) {
        // Sprite is already in cache, increment ref_count and return it
        it->second.ref_count++;
        ESP_LOGD(TAG, "Cache hit for '%s', ref_count now %d", full_path.c_str(), it->second.ref_count);
        return it->second.dsc;
    }

    // Sprite is not in cache, try to load it
    ESP_LOGD(TAG, "Cache miss for '%s'. Loading from SD card.", full_path.c_str());
    lv_image_dsc_t* new_dsc = load_from_sd(full_path);

    if (new_dsc) {
        CachedSprite new_entry;
        new_entry.dsc = new_dsc;
        new_entry.ref_count = 1;
        s_cache[full_path] = new_entry;
        ESP_LOGI(TAG, "Loaded and cached '%s', ref_count is 1.", full_path.c_str());
        return new_dsc;
    }

    return nullptr;
}

void SpriteCacheManager::release_sprite(const std::string& full_path) {
    std::lock_guard<std::mutex> lock(s_cache_mutex);

    auto it = s_cache.find(full_path);
    if (it == s_cache.end()) {
        ESP_LOGW(TAG, "Attempted to release a non-cached sprite: %s", full_path.c_str());
        return;
    }

    it->second.ref_count--;
    ESP_LOGD(TAG, "Released sprite '%s', ref_count now %d", full_path.c_str(), it->second.ref_count);

    if (it->second.ref_count <= 0) {
        ESP_LOGI(TAG, "Ref count for '%s' is zero. Deallocating memory.", full_path.c_str());
        free_sprite_data(it->second);
        s_cache.erase(it);
    }
}

void SpriteCacheManager::release_sprite_group(const std::vector<std::string>& paths) {
    for (const auto& path : paths) {
        release_sprite(path);
    }
}

lv_image_dsc_t* SpriteCacheManager::load_from_sd(const std::string& path) {
    char* file_buffer = nullptr;
    size_t file_size = 0;

    if (!sd_manager_read_file(path.c_str(), &file_buffer, &file_size)) {
        ESP_LOGE(TAG, "Failed to read sprite file: %s", path.c_str());
        return nullptr;
    }

    // Note: The file_buffer (image data) MUST be in a memory region that LVGL can access.
    // SPIRAM is great for this.
    lv_image_dsc_t* img_dsc = (lv_image_dsc_t*)heap_caps_malloc(sizeof(lv_image_dsc_t), MALLOC_CAP_DEFAULT);
    if (!img_dsc) {
        ESP_LOGE(TAG, "Failed to allocate memory for image descriptor");
        free(file_buffer);
        return nullptr;
    }

    // The data buffer itself was allocated by sd_manager_read_file, which uses standard malloc.
    // If you need it in SPIRAM, you would need a custom read function. For now, this is fine.
    img_dsc->data = (const uint8_t*)file_buffer;
    img_dsc->data_size = file_size;
    img_dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
    img_dsc->header.flags = LV_IMAGE_FLAGS_ALLOCATED;

    return img_dsc;
}

void SpriteCacheManager::free_sprite_data(CachedSprite& sprite) {
    if (sprite.dsc) {
        if (sprite.dsc->data) {
            free((void*)sprite.dsc->data); // Free the image data buffer
        }
        free(sprite.dsc); // Free the descriptor struct itself
        sprite.dsc = nullptr;
    }
}