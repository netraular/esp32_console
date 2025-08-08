#ifndef PET_ASSET_DATA_H
#define PET_ASSET_DATA_H

#include "pet_data_model.h"
#include <cstddef>

// --- Default/Fallback Sprite Filenames ---
// These are used when a specific pet does not have a custom sprite for a given stage.
constexpr const char* DEFAULT_EGG_SPRITE   = "egg_default.png";
constexpr const char* DEFAULT_BABY_SPRITE  = "baby_default.png";
constexpr const char* DEFAULT_TEEN_SPRITE  = "teen_default.png";
constexpr const char* DEFAULT_ADULT_SPRITE = "adult_default.png";


/**
 * @brief Defines the assets (name and sprites) for a specific pet type.
 * If a sprite filename is nullptr, a default sprite will be used as a fallback.
 */
struct PetAssetData {
    PetType type;
    const char* base_name;
    const char* baby_sprite_filename;
    const char* teen_sprite_filename;
    const char* adult_sprite_filename;
};

/**
 * @brief A compile-time table that maps each PetType to its specific assets.
 * This is the SINGLE SOURCE OF TRUTH for pet names and sprite files.
 * The order of entries in this array MUST match the order of the PetType enum.
 */
static const PetAssetData PET_ASSET_DEFINITIONS[] = {
    // PetType::FLUFFLE
    { PetType::FLUFFLE, "Fluffle", "fluffle_baby.png", "fluffle_teen.png", "fluffle_adult.png" },

    // PetType::ROCKY
    { PetType::ROCKY,   "Rocky",   "rocky_baby.png",   "rocky_teen.png",   "rocky_adult.png" },

    // PetType::SPROUT
    { PetType::SPROUT,  "Sprout",  "sprout_baby.png",  "sprout_teen.png",  "sprout_adult.png" },
    
    // PetType::EMBER
    // Example of using defaults: no specific assets defined for Ember yet.
    { PetType::EMBER,   "Ember",   nullptr,            nullptr,            nullptr }, 
    
    // PetType::AQUABUB
    { PetType::AQUABUB, "Aquabub", "aquabub_baby.png", "aquabub_teen.png", "aquabub_adult.png" },
};

// Compile-time check to ensure the definitions array matches the enum size.
static_assert(
    (sizeof(PET_ASSET_DEFINITIONS) / sizeof(PetAssetData)) == (size_t)PetType::COUNT,
    "The number of entries in PET_ASSET_DEFINITIONS must match the number of pets in the PetType enum."
);


#endif // PET_ASSET_DATA_H