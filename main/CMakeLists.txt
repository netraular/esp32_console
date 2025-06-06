idf_component_register(SRCS "main.cpp" 
                           "./controllers/screen_manager/screen_manager.cpp" 
                           "./controllers/button_manager/button_manager.cpp"
                           "./views/view_manager.cpp"
                           "./views/menu_view/menu_view.cpp"
                           "./views/mic_test_view/mic_test_view.cpp"
                           "./views/speaker_test_view/speaker_test_view.cpp"
                           "./views/sd_test_view/sd_test_view.cpp"
                           "./views/image_test_view/image_test_view.cpp"
                           "./views/buzzer_test_view/buzzer_test_view.cpp"
                   INCLUDE_DIRS "."
                   )

idf_build_set_property(PARTITION_TABLE_FILENAME partitions.csv)