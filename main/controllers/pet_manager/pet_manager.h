#ifndef PET_MANAGER_H
#define PET_MANAGER_H

#include "models/pet_data_model.h"
#include <string>
#include <vector>

/**
 * @brief Manages all game logic related to the pet's state and evolution.
 *
 * This class is a singleton that handles the pet's life cycle, care points,
 * and interaction with persistent storage. It is the central authority for
 * all pet-related data.
 */
class PetManager {
public:
    // Delete copy and assignment operators
    PetManager(const PetManager&) = delete;
    PetManager& operator=(const PetManager&) = delete;

    /**
     * @brief Gets the singleton instance of the PetManager.
     */
    static PetManager& get_instance();

    /**
     * @brief Initializes the manager, loading state from NVS.
     * Must be called once at startup.
     */
    void init();

    /**
     * @brief Updates the pet's state based on the current time.
     * This is the main logic function that handles cycle completion and
     * stage progression based on time passed.
     */
    void update_state();

    /**
     * @brief Adds care points to the current pet.
     * @param points The number of points to add.
     */
    void add_care_points(uint32_t points);

    /**
     * @brief Gets the current state of the active pet.
     * @return A copy of the current PetState struct.
     */
    PetState get_current_pet_state() const;

    /**
     * @brief Gets a display name for the pet. Uses custom name if set.
     * @param state The current state of the pet.
     * @return A string representing the pet's name.
     */
    std::string get_pet_display_name(const PetState& state) const;

    /**
     * @brief Gets the base name for a given pet type.
     * @param type The type of the pet.
     * @return The base name of the pet (e.g., "Fluffle").
     */
    std::string get_pet_base_name(PetType type) const;
    
    /**
     * @brief Gets the collection status for all pets.
     * @return A vector containing the collection status for each pet type.
     */
    std::vector<PetCollectionEntry> get_collection() const;

    /**
     * @brief Gets the full LVGL-compatible path to the current pet's sprite.
     * The path is ready to be used with `lv_image_set_src`.
     * @return A string with the path (e.g., "S:/sdcard/sprites/pets/baby.png").
     */
    std::string get_current_pet_sprite_path() const;

    /**
     * @brief Calculates the remaining time in seconds until the next evolution stage.
     * @param state The current state of the pet.
     * @return The number of seconds until the next stage, or 0 if the pet is at the final stage.
     */
    time_t get_time_to_next_stage(const PetState& state) const;

private:
    PetManager() = default; // Private constructor for singleton
    
    // --- State ---
    PetState s_pet_state;
    std::vector<PetCollectionEntry> s_collection;
    bool s_is_initialized = false;
    int s_current_egg_sprite_index = 0;

    // --- Persistence ---
    void load_state();
    void save_state() const;
    void save_collection() const;

    // --- Logic ---
    void start_new_cycle();
    void finalize_cycle();
    PetType select_random_new_pet();
};

#endif // PET_MANAGER_H