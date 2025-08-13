#include "inline_audio_player_component.h"
#include "controllers/audio_manager/audio_manager.h"
#include "config/app_config.h" // For MAX_VOLUME_PERCENTAGE
#include "esp_log.h"
#include <math.h>

static const char* TAG = "INLINE_AUDIO_PLAYER";

InlineAudioPlayerComponent::InlineAudioPlayerComponent(lv_obj_t* parent, const std::string& file_path)
    : m_file_path(file_path) {
    ESP_LOGI(TAG, "Creating for file: %s", m_file_path.c_str());
    
    init_styles();
    setup_ui(parent);
    
    if (audio_manager_play(m_file_path.c_str())) {
        m_update_timer = lv_timer_create(static_update_ui_timer_cb, 50, this);
        update_playback_state_indicator();
    } else {
        ESP_LOGE(TAG, "Failed to start audio playback for %s", m_file_path.c_str());
        if (m_on_close_callback) {
            m_on_close_callback();
        }
    }
}

InlineAudioPlayerComponent::~InlineAudioPlayerComponent() {
    ESP_LOGI(TAG, "Destructing for file: %s", m_file_path.c_str());
    if (m_update_timer) {
        lv_timer_delete(m_update_timer);
        m_update_timer = nullptr;
    }

    if (audio_manager_is_playing() && m_file_path == audio_manager_get_current_file()) {
        audio_manager_stop();
    }

    reset_styles();
}

void InlineAudioPlayerComponent::init_styles() {
    if (m_styles_initialized) return;

    // Style for the main slider bar (the timeline itself)
    lv_style_init(&m_style_slider_main);
    lv_style_set_height(&m_style_slider_main, 3);
    lv_style_set_bg_opa(&m_style_slider_main, LV_OPA_COVER);
    lv_style_set_bg_color(&m_style_slider_main, lv_palette_lighten(LV_PALETTE_GREY, 2));


    // Style for the knob when playing (circle)
    lv_style_init(&m_style_knob_playing);
    lv_style_set_bg_opa(&m_style_knob_playing, LV_OPA_COVER);
    lv_style_set_bg_color(&m_style_knob_playing, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_radius(&m_style_knob_playing, LV_RADIUS_CIRCLE);
    lv_style_set_pad_all(&m_style_knob_playing, 6);

    // Style for the knob when paused (thin vertical rectangle)
    lv_style_init(&m_style_knob_paused);
    lv_style_set_bg_opa(&m_style_knob_paused, LV_OPA_COVER);
    lv_style_set_bg_color(&m_style_knob_paused, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_radius(&m_style_knob_paused, 2); 
    lv_style_set_pad_ver(&m_style_knob_paused, 6); 
    lv_style_set_pad_hor(&m_style_knob_paused, 2);

    m_styles_initialized = true;
}

void InlineAudioPlayerComponent::reset_styles() {
    if (!m_styles_initialized) return;
    lv_style_reset(&m_style_slider_main);
    lv_style_reset(&m_style_knob_playing);
    lv_style_reset(&m_style_knob_paused);
    m_styles_initialized = false;
}

void InlineAudioPlayerComponent::setup_ui(lv_obj_t* parent) {
    m_container = lv_obj_create(parent);
    lv_obj_remove_style_all(m_container);
    lv_obj_set_size(m_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(m_container, LV_FLEX_FLOW_COLUMN);
    // Add vertical padding to ensure the slider knob is not clipped at the top
    lv_obj_set_style_pad_top(m_container, 8, 0);
    lv_obj_set_style_pad_hor(m_container, 0, 0);
    lv_obj_set_style_pad_bottom(m_container, 0, 0);
    lv_obj_set_style_pad_gap(m_container, 5, 0);
    lv_obj_set_align(m_container, LV_ALIGN_CENTER);

    // --- Progress Slider ---
    m_slider = lv_slider_create(m_container);
    lv_obj_remove_flag(m_slider, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_width(m_slider, LV_PCT(100));
    lv_obj_center(m_slider);
    lv_slider_set_range(m_slider, 0, 100);
    lv_slider_set_value(m_slider, 0, LV_ANIM_OFF);
    lv_obj_add_style(m_slider, &m_style_slider_main, LV_PART_MAIN);
    lv_obj_add_style(m_slider, &m_style_knob_playing, LV_PART_KNOB);

    // --- Bottom Row: Time Labels and Volume ---
    lv_obj_t* bottom_row = lv_obj_create(m_container);
    lv_obj_remove_style_all(bottom_row);
    lv_obj_set_width(bottom_row, LV_PCT(100));
    lv_obj_set_height(bottom_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bottom_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    m_time_current_label = lv_label_create(bottom_row);
    lv_label_set_text(m_time_current_label, "00:00");
    
    m_volume_label = lv_label_create(bottom_row);
    update_volume_display();

    m_time_total_label = lv_label_create(bottom_row);
    lv_label_set_text(m_time_total_label, "??:??");
}

void InlineAudioPlayerComponent::set_on_close_callback(std::function<void()> cb) {
    m_on_close_callback = cb;
}

void InlineAudioPlayerComponent::toggle_play_pause() {
    audio_player_state_t state = audio_manager_get_state();
    if (state == AUDIO_STATE_PLAYING) {
        audio_manager_pause();
    } else if (state == AUDIO_STATE_PAUSED) {
        audio_manager_resume();
    }
    update_playback_state_indicator();
}

void InlineAudioPlayerComponent::update_playback_state_indicator() {
    if (!m_slider) return;
    
    audio_player_state_t state = audio_manager_get_state();
    
    lv_obj_remove_style(m_slider, &m_style_knob_playing, LV_PART_KNOB);
    lv_obj_remove_style(m_slider, &m_style_knob_paused, LV_PART_KNOB);

    if (state == AUDIO_STATE_PLAYING) {
        lv_obj_add_style(m_slider, &m_style_knob_playing, LV_PART_KNOB);
    } else {
        lv_obj_add_style(m_slider, &m_style_knob_paused, LV_PART_KNOB);
    }
}

void InlineAudioPlayerComponent::update_volume_display() {
    if (!m_volume_label) return;
    
    uint8_t physical_vol = audio_manager_get_volume();
    uint8_t display_vol = (uint8_t)(roundf((float)(physical_vol * 100) / (float)MAX_VOLUME_PERCENTAGE));
    
    const char * icon;
    if (display_vol == 0) icon = LV_SYMBOL_MUTE;
    else if (display_vol < 50) icon = LV_SYMBOL_VOLUME_MID;
    else icon = LV_SYMBOL_VOLUME_MAX;
    
    lv_label_set_text_fmt(m_volume_label, "%s %d%%", icon, display_vol);
}

void InlineAudioPlayerComponent::update_ui_timer_cb() {
    audio_player_state_t state = audio_manager_get_state();
    
    if (state == AUDIO_STATE_STOPPED || state == AUDIO_STATE_ERROR) {
        if (m_on_close_callback) {
            m_on_close_callback();
        }
        return;
    }
    
    update_playback_state_indicator();
    
    uint32_t duration = audio_manager_get_duration_s();
    uint32_t progress = audio_manager_get_progress_s();
    
    if (duration > 0 && lv_slider_get_max_value(m_slider) != duration) {
        lv_slider_set_range(m_slider, 0, duration);
        lv_label_set_text_fmt(m_time_total_label, "%02lu:%02lu", duration / 60, duration % 60);
    }
    lv_label_set_text_fmt(m_time_current_label, "%02lu:%02lu", progress / 60, progress % 60);
    lv_slider_set_value(m_slider, progress, LV_ANIM_OFF);
}

lv_obj_t* InlineAudioPlayerComponent::get_container() {
    return m_container;
}

void InlineAudioPlayerComponent::static_update_ui_timer_cb(lv_timer_t* timer) {
    auto* instance = static_cast<InlineAudioPlayerComponent*>(lv_timer_get_user_data(timer));
    if (instance) {
        instance->update_ui_timer_cb();
    }
}