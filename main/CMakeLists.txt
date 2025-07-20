idf_component_register(SRCS 
        # --- Main Entry Point ---
        "main.cpp"

        # --- Controllers ---
        "./controllers/audio_manager/audio_manager.cpp"
        "./controllers/audio_recorder/audio_recorder.cpp"
        "./controllers/button_manager/button_manager.cpp"
        "./controllers/data_manager/data_manager.cpp"
        "./controllers/power_manager/power_manager.cpp"
        "./controllers/screen_manager/screen_manager.cpp"
        "./controllers/sd_card_manager/sd_card_manager.cpp"
        "./controllers/stt_manager/stt_manager.cpp"
        "./controllers/wifi_manager/wifi_manager.cpp"
        "./controllers/wifi_streamer/wifi_streamer.cpp"

        # --- Reusable UI Components ---
        "./components/audio_player_component/audio_player_component.cpp"
        "./components/audio_visualizer/audio_visualizer.cpp"
        "./components/file_explorer/file_explorer.cpp"
        "./components/status_bar_component/status_bar_component.cpp"
        "./components/text_viewer/text_viewer.cpp"

        # --- Views (Class-based) ---
        "./views/view_manager.cpp"
        "./views/standby_view/standby_view.cpp"
        "./views/menu_view/menu_view.cpp"
        "./views/multi_click_test_view/multi_click_test_view.cpp"
        "./views/click_counter_view/click_counter_view.cpp"
        "./views/click_counter_view/assets/coin_pile.c"
        "./views/image_test_view/image_test_view.cpp"
        "./views/image_test_view/assets/icon_1.c"
        "./views/sd_test_view/sd_test_view.cpp"
        "./views/speaker_test_view/speaker_test_view.cpp"
        "./views/mic_test_view/mic_test_view.cpp"
        "./views/pomodoro_view/pomodoro_view.cpp"                                   # <<< AÑADIDO
        "./views/pomodoro_view/components/pomodoro_config_component.cpp"           # <<< AÑADIDO
        "./views/pomodoro_view/components/pomodoro_timer_component.cpp"            # <<< AÑADIDO

    INCLUDE_DIRS "."
    EMBED_FILES "./server_certs/groq_api_ca.pem"
)

idf_build_set_property(PARTITION_TABLE_FILENAME partitions.csv)