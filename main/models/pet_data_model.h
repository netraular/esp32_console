#ifndef PET_DATA_MODEL_H
#define PET_DATA_MODEL_H

#include <cstdint>
#include <time.h>
#include <string>

/**
 * @brief Unique identifier for each pet species, analogous to a Pokedex number.
 * This allows for a structured, ID-based system for managing pets.
 */
enum class PetId : uint16_t {
    NONE = 0,

    // Kanto Starters & early pets
    PET_0001 = 1,    // Bulbasaur
    PET_0002 = 2,    // Ivysaur
    PET_0003 = 3,    // Venusaur
    PET_0004 = 4,    // Charmander
    PET_0005 = 5,    // Charmeleon
    PET_0006 = 6,    // Charizard
    PET_0007 = 7,    // Squirtle
    PET_0008 = 8,    // Wartortle
    PET_0009 = 9,    // Blastoise
    PET_0010 = 10,   // Caterpie
    PET_0011 = 11,   // Metapod
    PET_0012 = 12,   // Butterfree
    PET_0013 = 13,   // Weedle
    PET_0014 = 14,   // Kakuna
    PET_0015 = 15,   // Beedrill

    // Various 3-stage evolution lines
    PET_0043 = 43,   // Oddish
    PET_0044 = 44,   // Gloom
    PET_0045 = 45,   // Vileplume
    PET_0060 = 60,   // Poliwag
    PET_0061 = 61,   // Poliwhirl
    PET_0062 = 62,   // Poliwrath
    PET_0063 = 63,   // Abra
    PET_0064 = 64,   // Kadabra
    PET_0065 = 65,   // Alakazam
    PET_0066 = 66,   // Machop
    PET_0067 = 67,   // Machoke
    PET_0068 = 68,   // Machamp
    PET_0069 = 69,   // Bellsprout
    PET_0070 = 70,   // Weepinbell
    PET_0071 = 71,   // Victreebel
    PET_0074 = 74,   // Geodude
    PET_0075 = 75,   // Graveler
    PET_0076 = 76,   // Golem
    PET_0092 = 92,   // Gastly
    PET_0093 = 93,   // Haunter
    PET_0094 = 94,   // Gengar
};

/**
 * @brief Holds the state of the currently active pet.
 * This structure is saved to and loaded from persistent storage.
 */
struct PetState {
    PetId base_pet_id;          // The ID of the first-stage pet in this cycle (e.g., PET_0001).
    PetId current_pet_id;       // The ID of the pet's current form (e.g., PET_0002).
    uint32_t stage_care_points; // Care points accumulated for the current evolution stage.
    std::string custom_name;    // The name given by the user (or default).
    time_t cycle_start_timestamp; // The exact moment the cycle (egg) was created.
    time_t cycle_end_timestamp;   // The calculated timestamp for the end of the cycle.
};

/**
 * @brief Represents an entry in the player's pet collection.
 * The status is tracked by the base form of the evolution line.
 */
struct PetCollectionEntry {
    PetId base_id;
    bool discovered; // Has the player at least evolved the pet to its second stage?
    bool collected;  // Has the player successfully raised the pet to the end of its final stage?
};

#endif // PET_DATA_MODEL_H