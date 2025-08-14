#include "view_manager.h"
#include "controllers/button_manager/button_manager.h"
#include "esp_log.h"
#include <map>
#include <functional>
#include <memory>

// Include all view headers
#include "view.h"
// Core Views
#include "views/core/standby_view/standby_view.h"
#include "views/core/menu_view/menu_view.h"
#include "views/core/settings_view/settings_view.h"
#include "views/core/daily_journal_view/daily_journal_view.h"
#include "views/core/daily_summary_view/daily_summary_view.h"
// Game Views
#include "views/game/pet_view/pet_view.h"
#include "views/game/pet_collection_view/pet_collection_view.h"
#include "views/game/pet_hub_view/pet_hub_view.h"
#include "views/game/room_view/room_view.h"
// Habit Views
#include "views/habits/habit_add_view/habit_add_view.h"
#include "views/habits/habit_category_manager_view/habit_category_manager_view.h"
#include "views/habits/habit_history_view/habit_history_view.h"
#include "views/habits/habit_manager_view/habit_manager_view.h"
#include "views/habits/track_habits_view/track_habits_view.h"
// Notification Views
#include "views/notifications/add_notification_view/add_notification_view.h"
#include "views/notifications/notification_history_view/notification_history_view.h"
// Pomodoro View
#include "views/pomodoro_view/pomodoro_view.h"
// Testing Views
#include "views/testing/click_counter_view/click_counter_view.h"
#include "views/testing/image_test_view/image_test_view.h"
#include "views/testing/littlefs_test_view/littlefs_test_view.h"
#include "views/testing/mic_test_view/mic_test_view.h"
#include "views/testing/multi_click_test_view/multi_click_test_view.h"
#include "views/testing/popup_test_view/popup_test_view.h"
#include "views/testing/sd_test_view/sd_test_view.h"
#include "views/testing/speaker_test_view/speaker_test_view.h"
#include "views/testing/volume_tester_view/volume_tester_view.h"
#include "views/testing/wifi_stream_view/wifi_stream_view.h"
// Voice Note Views
#include "views/voice_note/voice_note_view/voice_note_view.h"
#include "views/voice_note/voice_note_player_view/voice_note_player_view.h"


static const char *TAG = "VIEW_MGR";

// --- State Variables ---
static std::unique_ptr<View> s_current_view = nullptr;
static view_id_t s_current_view_id = VIEW_ID_COUNT; // Initialize to invalid

// The View Factory: Maps a view_id_t to a function that creates a new view instance.
static std::map<view_id_t, std::function<View*()>> s_view_factory;

// --- View Name Helpers ---

// Macro to generate string literals from the X-Macro list
#define GENERATE_STRING(STRING) #STRING,

// Array of string names, automatically generated from FOR_EACH_VIEW
static const char* s_view_names[] = {
    FOR_EACH_VIEW(GENERATE_STRING)
};

const char* view_manager_get_view_name(view_id_t view_id) {
    if (view_id < VIEW_ID_COUNT) {
        return s_view_names[view_id];
    }
    return "UNKNOWN_VIEW";
}

/**
 * @brief Populates the view factory with all available views.
 */
static void initialize_view_factory() {
    s_view_factory[VIEW_ID_STANDBY] = []() { return new StandbyView(); };
    s_view_factory[VIEW_ID_MENU] = []() { return new MenuView(); };
    s_view_factory[VIEW_ID_SETTINGS] = []() { return new SettingsView(); };
    s_view_factory[VIEW_ID_DAILY_JOURNAL] = []() { return new DailyJournalView(); };
    s_view_factory[VIEW_ID_DAILY_SUMMARY] = []() { return new DailySummaryView(); };
    s_view_factory[VIEW_ID_PET_VIEW] = []() { return new PetView(); };
    s_view_factory[VIEW_ID_PET_COLLECTION] = []() { return new PetCollectionView(); };
    s_view_factory[VIEW_ID_PET_HUB] = []() { return new PetHubView(); };
    s_view_factory[VIEW_ID_ROOM] = []() { return new RoomView(); };
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
    s_view_factory[VIEW_ID_POPUP_TEST] = []() { return new PopupTestView(); };
    s_view_factory[VIEW_ID_HABIT_MANAGER] = []() { return new HabitManagerView(); };
    s_view_factory[VIEW_ID_HABIT_CATEGORY_MANAGER] = []() { return new HabitCategoryManagerView(); };
    s_view_factory[VIEW_ID_HABIT_ADD] = []() { return new HabitAddView(); };
    s_view_factory[VIEW_ID_TRACK_HABITS] = []() { return new TrackHabitsView(); };
    s_view_factory[VIEW_ID_HABIT_HISTORY] = []() { return new HabitHistoryView(); };
    s_view_factory[VIEW_ID_ADD_NOTIFICATION] = []() { return new AddNotificationView(); };
    s_view_factory[VIEW_ID_NOTIFICATION_HISTORY] = []() { return new NotificationHistoryView(); };
}

void view_manager_init(void) {
    ESP_LOGI(TAG, "Initializing View Manager.");
    initialize_view_factory();
    // Start the application with the StandbyView
    view_manager_load_view(VIEW_ID_STANDBY);
}

void view_manager_load_view(view_id_t view_id) {
    static view_id_t s_initializing_view_id = VIEW_ID_COUNT; 
    if (view_id >= VIEW_ID_COUNT) {
        ESP_LOGE(TAG, "Invalid view ID: %d", view_id);
        return;
    }

    if ((s_current_view && s_current_view_id == view_id) || s_initializing_view_id == view_id) {
        ESP_LOGW(TAG, "Attempted to load the same view (%s) again. Ignoring.", view_manager_get_view_name(view_id));
        return;
    }

    ESP_LOGI(TAG, "Request to load view %s (ID: %d)", view_manager_get_view_name(view_id), view_id);
    s_initializing_view_id = view_id;
    lv_obj_t *scr = lv_screen_active();

    button_manager_unregister_view_handlers();

    if (s_current_view) {
        ESP_LOGD(TAG, "Destroying previous view (%s)", view_manager_get_view_name(s_current_view_id));
        s_current_view.reset();
    }
    
    // Deletes all children of the screen (i.e., the old view's container)
    lv_obj_clean(scr);

    // Reset all local styles on the screen object itself. This removes any styles
    // (like gradients) that a misbehaving view might have applied directly to the screen.
    lv_obj_remove_style(scr, NULL, (lv_style_selector_t)LV_PART_MAIN | (lv_style_selector_t)LV_STATE_ANY);

    // Now, apply a clean, default background to the screen. All views will be
    // placed on top of this. Views with transparent backgrounds will show this color.
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    auto it = s_view_factory.find(view_id);
    if (it != s_view_factory.end()) {
        s_current_view.reset(it->second());
        s_current_view_id = view_id;
        ESP_LOGD(TAG, "New view instance created for %s", view_manager_get_view_name(view_id));
        
        s_current_view->create(scr);
    } else {
        ESP_LOGE(TAG, "View %s (ID: %d) not found in factory!", view_manager_get_view_name(view_id), view_id);
        lv_obj_t* label = lv_label_create(scr);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text_fmt(label, "Error: View %s\nnot implemented.", view_manager_get_view_name(view_id));
        s_current_view.reset(); // Reset to null, but keep the ID to show the error
        s_current_view_id = view_id;
    }

    s_initializing_view_id = VIEW_ID_COUNT;
    ESP_LOGI(TAG, "View %s loaded successfully.", view_manager_get_view_name(view_id));
}

view_id_t view_manager_get_current_view_id(void) {
    return s_current_view_id;
}