#ifndef ROOM_OBJECT_MANAGER_H
#define ROOM_OBJECT_MANAGER_H

#include "models/furniture_data_model.h"
#include <vector>
#include <string>

/**
 * @brief Manages the state of all furniture objects placed in a room.
 *
 * This class is responsible for loading the room layout from persistent storage,
 * providing methods to manipulate the objects in the room, and saving the
 * layout back to storage.
 */
class RoomObjectManager {
public:
    RoomObjectManager();
    ~RoomObjectManager() = default;

    /**
     * @brief Loads the room layout from a file on the LittleFS partition.
     */
    void load_layout();

    /**
     * @brief Saves the current room layout to a file on the LittleFS partition.
     */
    void save_layout();

    /**
     * @brief Gets a constant reference to the vector of all placed objects.
     */
    const std::vector<PlacedFurniture>& get_all_objects() const;
    
    /**
     * @brief Finds an object at a specific grid coordinate.
     * @param grid_x The X coordinate on the grid.
     * @param grid_y The Y coordinate on the grid.
     * @return A pointer to the PlacedFurniture object, or nullptr if no object is found.
     */
    const PlacedFurniture* get_object_at(int grid_x, int grid_y) const;
    
    /**
     * @brief Removes an object at a specific grid coordinate.
     * @param grid_x The X coordinate on the grid.
     * @param grid_y The Y coordinate on the grid.
     * @return True if an object was found and removed, false otherwise.
     */
    bool remove_object_at(int grid_x, int grid_y);

    /**
     * @brief Adds a new furniture object to the room.
     * @param object The PlacedFurniture object to add.
     * @return True on success, false if an object already exists at that position.
     */
    bool add_object(const PlacedFurniture& object);

private:
    std::vector<PlacedFurniture> m_placed_objects;
    std::string m_layout_file_path;
};

#endif // ROOM_OBJECT_MANAGER_H