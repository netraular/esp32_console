idf_component_register(SRCS "main.cpp" 
                           "./controllers/screen_manager/screen_manager.cpp" 
                           "./controllers/button_manager/button_manager.cpp"
                           "./controllers/sd_card_manager/sd_card_manager.cpp"
                           "./controllers/audio_manager/audio_manager.cpp"
                           "./controllers/audio_recorder/audio_recorder.cpp"
                           "./controllers/wifi_manager/wifi_manager.cpp"
                           "./controllers/wifi_streamer/wifi_streamer.cpp"
                           "./components/audio_visualizer/audio_visualizer.cpp"
                           "./components/file_explorer/file_explorer.cpp"
                           "./components/text_viewer/text_viewer.cpp"
                           "./views/view_manager.cpp"
                           "./views/menu_view/menu_view.cpp"
                           "./views/mic_test_view/mic_test_view.cpp"
                           "./views/speaker_test_view/speaker_test_view.cpp"
                           "./views/sd_test_view/sd_test_view.cpp"
                           "./views/image_test_view/image_test_view.cpp"
                           "./views/image_test_view/icon_1.c"
                           "./views/button_test_view/button_test_view.cpp"
                           "./views/multi_click_test_view/multi_click_test_view.cpp"
                           "./views/wifi_stream_view/wifi_stream_view.cpp"
                           "./views/pomodoro_view/pomodoro_view.cpp"
                           "./views/pomodoro_view/components/pomodoro_config_component.cpp"
                           "./views/pomodoro_view/components/pomodoro_timer_component.cpp"
                   INCLUDE_DIRS "."
                   )

idf_build_set_property(PARTITION_TABLE_FILENAME partitions.csv)