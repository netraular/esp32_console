#ifndef PLAYER_DATA_MODEL_H
#define PLAYER_DATA_MODEL_H

#include <cstdint>

/**
 * @brief Represents the player's profile and progress.
 *
 * This struct holds all data related to the user's in-game character,
 * such as level, experience, and currency. It's designed for future
 * gamification features.
 */
struct PlayerData {
    uint32_t level;
    uint32_t xp;
    uint32_t currency;
};

#endif // PLAYER_DATA_MODEL_H