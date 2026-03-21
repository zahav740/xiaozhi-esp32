#include "roki_lcd_display.h"
#include "lvgl_theme.h"
#include "assets/lang_config.h"
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <font_awesome.h>

#define TAG "RokiLcdDisplay"

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(BUILTIN_ICON_FONT);

RokiLcdDisplay::RokiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                               int width, int height, int offset_x, int offset_y,
                               bool mirror_x, bool mirror_y, bool swap_xy)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy) {}

void RokiLcdDisplay::SetupUI() {
    if (setup_ui_called_) return;
    Display::SetupUI();
    DisplayLockGuard lock(this);

    auto t = static_cast<LvglTheme*>(current_theme_);
    auto tf = t->text_font()->font();
    auto ic = t->icon_font()->font();
    auto scr = lv_screen_active();

    lv_obj_set_style_text_font(scr, tf, 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFF5E0), 0);

    container_ = lv_obj_create(scr);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_radius(container_, 0, 0);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, lv_color_hex(0xFFF5E0), 0);

    // Animated kawaii face
    face_ = std::make_unique<RokiFace>();
    face_->Create(scr, width_, height_);

    // Top bar
    top_bar_ = lv_obj_create(scr);
    lv_obj_set_size(top_bar_, LV_HOR_RES, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(top_bar_, 0, 0);
    lv_obj_set_style_bg_opa(top_bar_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_bar_, 0, 0);
    lv_obj_set_style_pad_all(top_bar_, 0, 0);
    lv_obj_set_style_pad_top(top_bar_, t->spacing(2), 0);
    lv_obj_set_style_pad_bottom(top_bar_, t->spacing(2), 0);
    lv_obj_set_style_pad_left(top_bar_, t->spacing(4), 0);
    lv_obj_set_style_pad_right(top_bar_, t->spacing(4), 0);
    lv_obj_set_flex_flow(top_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_bar_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(top_bar_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(top_bar_, LV_ALIGN_TOP_MID, 0, 0);

    network_label_ = lv_label_create(top_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, ic, 0);
    lv_obj_set_style_text_color(network_label_, lv_color_hex(0x333333), 0);

    auto ri = lv_obj_create(top_bar_);
    lv_obj_set_size(ri, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(ri, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ri, 0, 0);
    lv_obj_set_style_pad_all(ri, 0, 0);
    lv_obj_set_flex_flow(ri, LV_FLEX_FLOW_ROW);

    mute_label_ = lv_label_create(ri);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, ic, 0);
    lv_obj_set_style_text_color(mute_label_, lv_color_hex(0x333333), 0);

    battery_label_ = lv_label_create(ri);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, ic, 0);
    lv_obj_set_style_text_color(battery_label_, lv_color_hex(0x333333), 0);
    lv_obj_set_style_margin_left(battery_label_, t->spacing(2), 0);

    // Status bar
    status_bar_ = lv_obj_create(scr);
    lv_obj_set_size(status_bar_, LV_HOR_RES, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_bg_opa(status_bar_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_scrollbar_mode(status_bar_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_layout(status_bar_, LV_LAYOUT_NONE, 0);
    lv_obj_align(status_bar_, LV_ALIGN_TOP_MID, 0, 0);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_width(notification_label_, LV_HOR_RES * 0.75);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(notification_label_, lv_color_hex(0x333333), 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_align(notification_label_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_width(status_label_, LV_HOR_RES * 0.75);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(status_label_, lv_color_hex(0x333333), 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    lv_obj_align(status_label_, LV_ALIGN_CENTER, 0, 0);

    // Bottom bar
    bottom_bar_ = lv_obj_create(scr);
    lv_obj_set_size(bottom_bar_, LV_HOR_RES, tf->line_height + t->spacing(8));
    lv_obj_set_style_radius(bottom_bar_, 0, 0);
    lv_obj_set_style_bg_color(bottom_bar_, lv_color_hex(0xFFF5E0), 0);
    lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_80, 0);
    lv_obj_set_style_border_width(bottom_bar_, 0, 0);
    lv_obj_set_scrollbar_mode(bottom_bar_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(bottom_bar_, LV_ALIGN_BOTTOM_MID, 0, 0);

    chat_message_label_ = lv_label_create(bottom_bar_);
    lv_label_set_text(chat_message_label_, "");
    lv_obj_set_width(chat_message_label_, LV_HOR_RES - t->spacing(8));
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(chat_message_label_, lv_color_hex(0x333333), 0);
    lv_obj_align(chat_message_label_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);

    // Low battery
    low_battery_popup_ = lv_obj_create(scr);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, tf->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, -t->spacing(4));
    lv_obj_set_style_bg_color(low_battery_popup_, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_radius(low_battery_popup_, t->spacing(4), 0);
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);

    // Dummy emoji objects for base class
    emoji_image_ = lv_img_create(scr);
    lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
    emoji_label_ = lv_label_create(scr);
    lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "Kawaii face UI ready (animated)");
}

void RokiLcdDisplay::SetEmotion(const char* e) {
    if (face_) face_->SetEmotion(e);
}

void RokiLcdDisplay::SetSpeaking(bool s) {
    if (face_) face_->SetSpeaking(s);
}

void RokiLcdDisplay::FeedAudioAmplitude(const int16_t* d, size_t n) {
    if (face_) face_->FeedAmplitude(d, n);
}
