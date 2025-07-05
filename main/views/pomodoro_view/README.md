# Pomodoro Clock View

## Description
This view provides a fully functional Pomodoro timer. It is composed of a main "controller" view that manages two distinct components: a configuration screen and a timer screen. This modular design improves code clarity and user experience.

## Features
-   **Component-Based Architecture:** The logic is split into a configuration component and a timer component, managed by a parent view.
-   **Redesigned Configuration UI:** A clean, minimalist UI for setting work time, break time, and rounds. It's designed for small screens and button-based navigation.
    -   The currently selected field is highlighted with a yellow border.
    -   `OK` moves to the next field.
    -   `Left`/`Right` changes the value of the focused field.
    -   `Cancel` exits the entire Pomodoro feature.
-   **Focused Timer UI:** The running screen displays a large countdown timer, a visual progress arc, the current mode (WORK/BREAK), and the round number.
    -   `OK` toggles between Pause and Resume.
    -   `Cancel` stops the session and returns to the configuration screen.

## How to Use
This view is integrated into the main menu. Simply select "Pomodoro Clock" to launch it. The system will first present the configuration screen. After setting the desired parameters and pressing "START", it will transition to the timer screen.

## Dependencies
-   `view_manager` for view loading.
-   `button_manager` for input handling.
-   `lvgl` for the UI.