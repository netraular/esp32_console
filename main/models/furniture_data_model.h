#ifndef FURNITURE_DATA_MODEL_H
#define FURNITURE_DATA_MODEL_H

#include <string>
#include <vector>
#include <map>

/**
 * @brief Represents the x, y, z dimensions of a furniture item on the grid.
 */
struct FurnitureDimensions {
    int x = 1;
    int y = 1;
    float z = 1.0f;
};

/**
 * @brief Represents a single visual layer (sprite) of a furniture item.
 */
struct FurnitureAsset {
    std::string name;
    std::string source; // If it's an alias of another asset
    bool flip_h = false;
    int x_offset = 0;
    int y_offset = 0;
};

/**
 * @brief Represents the complete definition of a furniture type, parsed from JSON.
 * This data is static and loaded from the SD card.
 */
struct FurnitureData {
    std::string type_name;
    FurnitureDimensions dimensions;
    std::vector<int> directions; // Allowed rotation directions (e.g., 90, 180)
    int layer_count = 1;
    std::map<std::string, FurnitureAsset> assets;
};

/**
 * @brief Represents an instance of a furniture item placed in the room.
 * This is the dynamic data that will be saved in the room layout file on LittleFS.
 */
struct PlacedFurniture {
    std::string type_name; // Key to look up FurnitureData in the manager
    int grid_x = 0;
    int grid_y = 0;
    float grid_z = 0.0f;   // Base height on the floor or on top of other items
    int direction = 90;    // Rotation, e.g., 90 degrees
};

#endif // FURNITURE_DATA_MODEL_H