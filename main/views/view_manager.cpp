#include "view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"
#include <map>
#include <functional>
#include <memory>

// Include all view headers
#include "view.h"
#include "standby_view/standby_view.h"
#include "menu_view/menu_view.h"
#include "multi_click_test_view/multi_click_test_view.h"
#include "click_counter_view/click_counter_view.h"
#include "image_test_view/image_test_view.h"
#include "littlefs_test_view/littlefs_test_view.h"
#include "sd_test_view/sd_test_view.h"
#include "speaker_test_view/speaker_test_view.h"
#include "mic_test_view/mic_test_view.h"
#include "pomodoro_view/pomodoro_view.h"
#include "voice_note_view/voice_note_view.h"
#include "voice_note_player_view/voice_note_player_view.h"
#include "volume_tester_view/volume_tester_view.h"
#include "wifi_stream_view/wifi_stream_view.h"
#include "habit_manager_view/habit_manager_view.h"
#include "habit_category_manager_view/habit_category_manager_view.h"
#include "habit_add_view/habit_add_view.h" // <-- ADDED


static const char *TAG = "VIEW_MGR";

// --- State Variables ---
static std::unique_ptr<View> s_current_view = nullptr;
static view_id_t s_current_view_id;

// The View Factory: Maps a view_id_t to a function that creates a new view instance.
static std::map<view_id_t, std::function<View*()>> s_view_factory;

/**
 * @brief Populates the view factory with all available views.
 */
static void initialize_view_factory() {
    s_view_factory[VIEW_ID_STANDBY] = []() { return new StandbyView(); };
    s_view_factory[VIEW_ID_MENU] = []() { return new MenuView(); };
    s_view_factory[VIEW_ID_MULTI_CLICK_TEST] = []() { return new MultiClickTestView(); };
    s_view_factory[VIEW_ID_CLICK_COUNTER_TEST] = []() { return new ClickCounterView(); };
    s_view_factory[VIEW_ID_IMAGE_TEST] = []() { return new ImageTestView(); };
    s_view_factory[VIEW_ID_LITTLEFS_TEST] = []() { return new LittlefsTestView(); };
    s_view_factory[VIEW_ID_SD_TEST] = []() { return new SdTestView(); };
    s_view_factory[VIEW_ID_SPEAKER_TEST] = []() { return new SpeakerTestView(); };
    s_view_factory[VIEW_ID_MIC_TEST] = []() { return new MicTestView(); };
    s_view_factory[VIEW_ID_POMODORO] = []() { return new PomodoroView(); };
    s_view_factory[VIEW_ID_VOICE_NOTE] = []() { return new VoiceNoteView(); };
    s_view_factory[VIEW_ID_VOICE_NOTE_PLAYER] = []() { return new VoiceNotePlayerView(); };
    s_view_factory[VIEW_ID_VOLUME_TESTER] = []() { return new VolumeTesterView(); };
    s_view_factory[VIEW_ID_WIFI_STREAM_TEST] = []() { return new WifiStreamView(); };
    s_view_factory[VIEW_ID_HABIT_MANAGER] = []() { return new HabitManagerView(); };
    s_view_factory[VIEW_ID_HABIT_CATEGORY_MANAGER] = []() { return new HabitCategoryManagerView(); };
    s_view_factory[VIEW_ID_HABIT_ADD] = []() { return new HabitAddView(); }; // <-- ADDED
}

void view_manager_init(void) {
    ESP_LOGI(TAG, "Initializing View Manager.");
    initialize_view_factory();
    // Start the application with the StandbyView
    view_manager_load_view(VIEW_ID_STANDBY);
}

void view_manager_load_view(view_id_t view_id) {
    // A special value to prevent reloading during initialization
    static view_id_t s_initializing_view_id = VIEW_ID_COUNT; 
    if (view_id >= VIEW_ID_COUNT) {
        ESP_LOGE(TAG, "Invalid view ID: %d", view_id);
        return;
    }

    // Don't reload the same view or a view that is currently being loaded
    if ((s_current_view && s_current_view_id == view_id) || s_initializing_view_id == view_id) {
        ESP_LOGW(TAG, "Attempted to load the same view (ID: %d) again. Ignoring.", view_id);
        return;
    }

    ESP_LOGI(TAG, "Request to load view %d", view_id);
    s_initializing_view_id = view_id; // Mark as being loaded
    lv_obj_t *scr = lv_screen_active();

    // --- View Transition Logic ---

    // 1. Unregister all view-specific button handlers from the previous view.
    button_manager_unregister_view_handlers();

    // 2. Destroy the current view.
    // The unique_ptr's reset() method calls `delete` on the managed object,
    // which in turn calls the view's destructor for automatic resource cleanup.
    if (s_current_view) {
        ESP_LOGD(TAG, "Destroying previous view (ID: %d)", s_current_view_id);
        s_current_view.reset();
    }

    // 3. Clean the LVGL screen object. This deletes all widgets from the old view.
    lv_obj_clean(scr);

    // 4. Create the new view using the factory.
    auto it = s_view_factory.find(view_id);
    if (it != s_view_factory.end()) {
        // Create a new instance and transfer ownership to the unique_ptr.
        s_current_view.reset(it->second());
        s_current_view_id = view_id;
        ESP_LOGD(TAG, "New view instance created for ID: %d", view_id);
        
        // 5. Initialize the new view's UI.
        s_current_view->create(scr);
    } else {
        // Fallback for an unregistered view ID.
        ESP_LOGE(TAG, "View ID %d not found in factory!", view_id);
        lv_obj_t* label = lv_label_create(scr);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text_fmt(label, "Error: View %d\nnot implemented.", view_id);
        s_current_view.reset(); // No view object to manage
        s_current_view_id = view_id; // Set ID to avoid reload attempts
    }

    s_initializing_view_id = VIEW_ID_COUNT; // Mark loading as complete
    ESP_LOGI(TAG, "View %d loaded successfully.", view_id);
}