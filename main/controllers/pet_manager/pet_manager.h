#ifndef PET_MANAGER_H
#define PET_MANAGER_H

#include "models/pet_data_model.h"
#include "models/pet_asset_data.h"
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

    static PetManager& get_instance();
    void init();
    void update_state();
    void add_care_points(uint32_t points);
    
    /**
     * @brief Requests a new egg after a cycle has finished.
     * This moves the state from AWAITING_NEW_CYCLE to a new egg stage.
     */
    void request_new_egg();

    PetState get_current_pet_state() const;
    uint32_t get_current_stage_care_goal() const;
    std::string get_pet_display_name(const PetState& state) const;
    std::string get_pet_name(PetId id) const;
    const PetData* get_pet_data(PetId id) const;
    
    std::vector<PetCollectionEntry> get_collection() const;
    std::string get_current_pet_sprite_path() const;
    std::string get_sprite_path_for_id(PetId id) const;
    
    time_t get_time_to_next_stage(const PetState& state) const;
    PetId get_final_evolution(PetId base_id) const;

    bool is_in_egg_stage() const;
    /**
     * @brief Checks if the manager is waiting for the user to request a new egg.
     * @return true if a cycle has ended and the system is idle.
     */
    bool is_awaiting_new_cycle() const;
    time_t get_time_to_hatch() const;


private:
    PetManager() = default; // Private constructor for singleton
    
    // --- State ---
    PetState s_pet_state;
    std::vector<PetCollectionEntry> s_collection;
    bool s_is_initialized = false;
    
    // --- Persistence ---
    void load_state();
    void save_state() const;
    void save_collection() const;

    // --- Logic ---
    void start_new_cycle();
    void hatch_egg();
    void finalize_cycle();
    void fail_cycle();
    PetId select_random_hatchable_pet();
    void set_awaiting_new_cycle_state();
};

#endif // PET_MANAGER_H