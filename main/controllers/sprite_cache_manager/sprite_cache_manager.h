#ifndef SPRITE_CACHE_MANAGER_H
#define SPRITE_CACHE_MANAGER_H

#include "lvgl.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

/**
 * @brief Manages loading, caching, and releasing sprite image data from the SD card.
 *
 * This singleton class implements a reference-counted cache to efficiently manage
 * memory for sprites. It reads the raw image file (e.g., PNG) into PSRAM once,
 * then provides a descriptor to LVGL. LVGL decodes the image and caches the
 * result internally. This manager ensures the raw data in PSRAM is freed only
 * when no longer in use.
 */
class SpriteCacheManager {
public:
    // Delete copy and move constructors and assignments
    SpriteCacheManager(const SpriteCacheManager&) = delete;
    SpriteCacheManager& operator=(const SpriteCacheManager&) = delete;

    /**
     * @brief Gets the singleton instance of the manager.
     */
    static SpriteCacheManager& get_instance();

    /**
     * @brief Gets a sprite descriptor for a given path, loading it if necessary.
     *
     * This function implements reference counting. Each successful call increments
     * the reference count for the sprite.
     *
     * @param full_path The absolute C-style path to the sprite file (e.g., "/sdcard/assets/sprites/pet/0001/idle.png").
     * @return A const pointer to the lv_image_dsc_t, or nullptr if loading fails.
     */
    const lv_image_dsc_t* get_sprite(const std::string& full_path);

    /**
     * @brief Releases a sprite, decrementing its reference count.
     *
     * If the reference count drops to zero, the sprite's memory is deallocated
     * and it is removed from the cache.
     *
     * @param full_path The absolute path to the sprite file that was previously loaded.
     */
    void release_sprite(const std::string& full_path);

    /**
     * @brief Releases a list of sprites.
     *
     * A convenience function to release multiple sprites at once, for example,
     * when a view is destroyed.
     *
     * @param paths A vector of full paths to the sprites to release.
     */
    void release_sprite_group(const std::vector<std::string>& paths);


private:
    SpriteCacheManager() = default; // Private constructor
    ~SpriteCacheManager();

    // The actual data stored in the cache
    struct CachedSprite {
        lv_image_dsc_t* dsc = nullptr;
        int ref_count = 0;
    };

    // Internal implementation to load from SD card
    lv_image_dsc_t* load_from_sd(const std::string& path);
    void free_sprite_data(const std::string& path, CachedSprite& sprite);

    std::unordered_map<std::string, CachedSprite> m_cache;
    std::mutex m_cache_mutex;
};

#endif // SPRITE_CACHE_MANAGER_H