#ifndef NOTIFICATION_DATA_MODEL_H
#define NOTIFICATION_DATA_MODEL_H

#include <string>
#include <time.h>

/**
 * @brief Represents a single notification event in the system.
 */
struct Notification {
    uint32_t id;          // Unique identifier for the notification
    std::string title;    // The title of the notification (for the popup)
    std::string message;  // The main content of the notification
    time_t timestamp;     // The time when the notification was created
    bool is_read;         // Flag to track if the user has dismissed the notification
};

#endif // NOTIFICATION_DATA_MODEL_H