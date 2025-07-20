#ifndef POMODORO_COMMON_H
#define POMODORO_COMMON_H

#include <stdint.h>

// Structure to hold the configuration for a Pomodoro session.
// This is shared between the config component and the timer component.
typedef struct {
    uint32_t work_seconds;
    uint32_t break_seconds;
    uint32_t iterations;
} pomodoro_settings_t;

#endif // POMODORO_COMMON_H