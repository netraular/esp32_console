#ifndef PET_ASSET_DATA_H
#define PET_ASSET_DATA_H

#include "pet_data_model.h"
#include <map>
#include <string>

/**
 * @brief Defines the core attributes for a single pet.
 * This struct is the schema for our central pet data map. Sprite paths are
 * derived from the ID, not stored here.
 */
struct PetData {
    PetId id;                   // The unique ID of this pet.
    std::string name;           // The official base name of the pet.
    PetId evolves_to;           // The ID of the pet it evolves into. PetId::NONE if it's a final stage.
    bool can_hatch;             // True if this is a base form that can be obtained from an egg.
    uint32_t care_points_needed; // Points needed to successfully evolve from THIS stage.
};

/**
 * @brief A compile-time map that serves as the central "Pet Registry" for the application.
 * This is the SINGLE SOURCE OF TRUTH for all pet data. To add a new pet
 * or evolution line, simply add its data here.
 */
static const std::map<PetId, PetData> PET_DATA_REGISTRY = {
    // --- Bulbasaur Line ---
    { PetId::PET_0001, { .id = PetId::PET_0001, .name = "Bulbasaur",  .evolves_to = PetId::PET_0002, .can_hatch = true,  .care_points_needed = 30 }},
    { PetId::PET_0002, { .id = PetId::PET_0002, .name = "Ivysaur",    .evolves_to = PetId::PET_0003, .can_hatch = false, .care_points_needed = 60 }},
    { PetId::PET_0003, { .id = PetId::PET_0003, .name = "Venusaur",   .evolves_to = PetId::NONE,     .can_hatch = false, .care_points_needed = 100 }},

    // --- Charmander Line ---
    { PetId::PET_0004, { .id = PetId::PET_0004, .name = "Charmander", .evolves_to = PetId::PET_0005, .can_hatch = true,  .care_points_needed = 40 }},
    { PetId::PET_0005, { .id = PetId::PET_0005, .name = "Charmeleon", .evolves_to = PetId::PET_0006, .can_hatch = false, .care_points_needed = 70 }},
    { PetId::PET_0006, { .id = PetId::PET_0006, .name = "Charizard",  .evolves_to = PetId::NONE,     .can_hatch = false, .care_points_needed = 100 }},

    // --- Squirtle Line ---
    { PetId::PET_0007, { .id = PetId::PET_0007, .name = "Squirtle",   .evolves_to = PetId::PET_0008, .can_hatch = true,  .care_points_needed = 35 }},
    { PetId::PET_0008, { .id = PetId::PET_0008, .name = "Wartortle",  .evolves_to = PetId::PET_0009, .can_hatch = false, .care_points_needed = 65 }},
    { PetId::PET_0009, { .id = PetId::PET_0009, .name = "Blastoise",  .evolves_to = PetId::NONE,     .can_hatch = false, .care_points_needed = 100 }},
    
    // --- Caterpie Line ---
    { PetId::PET_0010, { .id = PetId::PET_0010, .name = "Caterpie",   .evolves_to = PetId::PET_0011, .can_hatch = true,  .care_points_needed = 10 }},
    { PetId::PET_0011, { .id = PetId::PET_0011, .name = "Metapod",    .evolves_to = PetId::PET_0012, .can_hatch = false, .care_points_needed = 20 }},
    { PetId::PET_0012, { .id = PetId::PET_0012, .name = "Butterfree", .evolves_to = PetId::NONE,     .can_hatch = false, .care_points_needed = 50 }},
    
    // --- Weedle Line ---
    { PetId::PET_0013, { .id = PetId::PET_0013, .name = "Weedle",     .evolves_to = PetId::PET_0014, .can_hatch = true,  .care_points_needed = 15 }},
    { PetId::PET_0014, { .id = PetId::PET_0014, .name = "Kakuna",     .evolves_to = PetId::PET_0015, .can_hatch = false, .care_points_needed = 25 }},
    { PetId::PET_0015, { .id = PetId::PET_0015, .name = "Beedrill",   .evolves_to = PetId::NONE,     .can_hatch = false, .care_points_needed = 55 }},
    
    // --- Oddish Line ---
    { PetId::PET_0043, { .id = PetId::PET_0043, .name = "Oddish",     .evolves_to = PetId::PET_0044, .can_hatch = true,  .care_points_needed = 25 }},
    { PetId::PET_0044, { .id = PetId::PET_0044, .name = "Gloom",      .evolves_to = PetId::PET_0045, .can_hatch = false, .care_points_needed = 50 }},
    { PetId::PET_0045, { .id = PetId::PET_0045, .name = "Vileplume",  .evolves_to = PetId::NONE,     .can_hatch = false, .care_points_needed = 90 }},

    // --- Poliwag Line ---
    { PetId::PET_0060, { .id = PetId::PET_0060, .name = "Poliwag",    .evolves_to = PetId::PET_0061, .can_hatch = true,  .care_points_needed = 30 }},
    { PetId::PET_0061, { .id = PetId::PET_0061, .name = "Poliwhirl",  .evolves_to = PetId::PET_0062, .can_hatch = false, .care_points_needed = 60 }},
    { PetId::PET_0062, { .id = PetId::PET_0062, .name = "Poliwrath",  .evolves_to = PetId::NONE,     .can_hatch = false, .care_points_needed = 100 }},

    // --- Abra Line ---
    { PetId::PET_0063, { .id = PetId::PET_0063, .name = "Abra",       .evolves_to = PetId::PET_0064, .can_hatch = true,  .care_points_needed = 50 }},
    { PetId::PET_0064, { .id = PetId::PET_0064, .name = "Kadabra",    .evolves_to = PetId::PET_0065, .can_hatch = false, .care_points_needed = 80 }},
    { PetId::PET_0065, { .id = PetId::PET_0065, .name = "Alakazam",   .evolves_to = PetId::NONE,     .can_hatch = false, .care_points_needed = 100 }},

    // --- Machop Line ---
    { PetId::PET_0066, { .id = PetId::PET_0066, .name = "Machop",     .evolves_to = PetId::PET_0067, .can_hatch = true,  .care_points_needed = 30 }},
    { PetId::PET_0067, { .id = PetId::PET_0067, .name = "Machoke",    .evolves_to = PetId::PET_0068, .can_hatch = false, .care_points_needed = 60 }},
    { PetId::PET_0068, { .id = PetId::PET_0068, .name = "Machamp",    .evolves_to = PetId::NONE,     .can_hatch = false, .care_points_needed = 100 }},

    // --- Bellsprout Line ---
    { PetId::PET_0069, { .id = PetId::PET_0069, .name = "Bellsprout", .evolves_to = PetId::PET_0070, .can_hatch = true,  .care_points_needed = 25 }},
    { PetId::PET_0070, { .id = PetId::PET_0070, .name = "Weepinbell", .evolves_to = PetId::PET_0071, .can_hatch = false, .care_points_needed = 50 }},
    { PetId::PET_0071, { .id = PetId::PET_0071, .name = "Victreebel", .evolves_to = PetId::NONE,     .can_hatch = false, .care_points_needed = 90 }},
    
    // --- Geodude Line ---
    { PetId::PET_0074, { .id = PetId::PET_0074, .name = "Geodude",    .evolves_to = PetId::PET_0075, .can_hatch = true,  .care_points_needed = 40 }},
    { PetId::PET_0075, { .id = PetId::PET_0075, .name = "Graveler",   .evolves_to = PetId::PET_0076, .can_hatch = false, .care_points_needed = 70 }},
    { PetId::PET_0076, { .id = PetId::PET_0076, .name = "Golem",      .evolves_to = PetId::NONE,     .can_hatch = false, .care_points_needed = 100 }},

    // --- Gastly Line ---
    { PetId::PET_0092, { .id = PetId::PET_0092, .name = "Gastly",     .evolves_to = PetId::PET_0093, .can_hatch = true,  .care_points_needed = 45 }},
    { PetId::PET_0093, { .id = PetId::PET_0093, .name = "Haunter",    .evolves_to = PetId::PET_0094, .can_hatch = false, .care_points_needed = 75 }},
    { PetId::PET_0094, { .id = PetId::PET_0094, .name = "Gengar",     .evolves_to = PetId::NONE,     .can_hatch = false, .care_points_needed = 100 }},
};

#endif // PET_ASSET_DATA_H