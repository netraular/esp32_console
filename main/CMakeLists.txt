idf_component_register(SRCS 
        # --- Main Entry Point ---
        "main.cpp"

        # --- Controllers ---
        "./controllers/audio_manager/audio_manager.cpp"
        "./controllers/audio_recorder/audio_recorder.cpp"
        "./controllers/button_manager/button_manager.cpp"
        "./controllers/data_manager/data_manager.cpp"
        "./controllers/littlefs_manager/littlefs_manager.cpp"
        "./controllers/power_manager/power_manager.cpp"
        "./controllers/screen_manager/screen_manager.cpp"
        "./controllers/sd_card_manager/sd_card_manager.cpp"
        "./controllers/habit_data_manager/habit_data_manager.cpp"
        "./controllers/stt_manager/stt_manager.cpp"
        "./controllers/wifi_manager/wifi_manager.cpp"
        "./controllers/wifi_streamer/wifi_streamer.cpp"
        "./controllers/lvgl_vfs_driver/lvgl_fs_driver.cpp"

        # --- Reusable UI Components ---
        "./components/audio_player_component/audio_player_component.cpp"
        "./components/audio_visualizer/audio_visualizer.cpp"
        "./components/file_explorer/file_explorer.cpp"
        "./components/status_bar_component/status_bar_component.cpp"
        "./components/text_viewer/text_viewer.cpp"
        "./components/popup_manager/popup_manager.cpp"

        # --- Views (Class-based) ---
        "./views/view_manager.cpp"
        "./views/standby_view/standby_view.cpp"
        "./views/menu_view/menu_view.cpp"
        "./views/multi_click_test_view/multi_click_test_view.cpp"
        "./views/click_counter_view/click_counter_view.cpp"
        "./views/click_counter_view/assets/coin_pile.c"
        "./views/image_test_view/image_test_view.cpp"
        "./views/littlefs_test_view/littlefs_test_view.cpp"
        "./views/sd_test_view/sd_test_view.cpp"
        "./views/speaker_test_view/speaker_test_view.cpp"
        "./views/mic_test_view/mic_test_view.cpp"
        "./views/pomodoro_view/pomodoro_view.cpp"
        "./views/pomodoro_view/components/pomodoro_config_component.cpp"
        "./views/pomodoro_view/components/pomodoro_timer_component.cpp"
        "./views/voice_note_view/voice_note_view.cpp"
        "./views/voice_note_player_view/voice_note_player_view.cpp"
        "./views/volume_tester_view/volume_tester_view.cpp"
        "./views/wifi_stream_view/wifi_stream_view.cpp"
        "./views/habit_manager_view/habit_manager_view.cpp"
        "./views/habit_category_manager_view/habit_category_manager_view.cpp"
        "./views/habit_add_view/habit_add_view.cpp"
        "./views/track_habits_view/track_habits_view.cpp"
        "./views/habit_history_view/habit_history_view.cpp"

    INCLUDE_DIRS "." "models"
    EMBED_FILES 
        "./server_certs/groq_api_ca.pem"
        "./littlefs/welcome.txt"
)

idf_build_set_property(PARTITION_TABLE_FILENAME partitions.csv)