#ifndef PET_DATA_MODEL_H
#define PET_DATA_MODEL_H

#include <cstdint>
#include <time.h>
#include <string>

/**
 * @brief The different species of pets available in the game.
 */
enum class PetType : uint8_t {
    FLUFFLE,   // A fluffy, cloud-like creature
    ROCKY,     // A sturdy golem-like pet
    SPROUT,    // A plant-based creature
    EMBER,     // A small fire elemental
    AQUABUB,   // A water bubble pet
    COUNT      // The total number of pet types
};

/**
 * @brief The stages of a pet's life cycle.
 */
enum class PetStage : uint8_t {
    NONE,      // No active pet
    EGG,
    BABY,
    TEEN,
    ADULT
};

/**
 * @brief Holds the state of the currently active pet.
 * This structure is saved to and loaded from persistent storage.
 */
struct PetState {
    PetType type;
    PetStage stage;
    uint32_t care_points;
    std::string custom_name;       // The name given by the user (or default).
    time_t cycle_start_timestamp;  // The exact moment the cycle (egg) was created.
    time_t cycle_end_timestamp;    // The calculated timestamp for the end of the cycle (always a Sunday).
};

/**
 * @brief Represents an entry in the player's pet collection.
 */
struct PetCollectionEntry {
    PetType type;
    bool discovered; // Has the player at least encountered this pet? (Reached Teen stage)
    bool collected;  // Has the player successfully raised this pet to the end of Adulthood?
};

#endif // PET_DATA_MODEL_H