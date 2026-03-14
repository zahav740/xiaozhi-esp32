#include "face_animation.h"
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <cstdlib>

#define TAG "FaceAnimation"

FaceAnimation::FaceAnimation(lv_obj_t* parent, int width, int height)
    : display_w_(width), display_h_(height) {

    // Compute proportional sizes (tuned for 240x240, scales to other resolutions)
    eye_w_        = display_w_ * 22 / 100;  // 53px on 240 — large oval eyes
    eye_h_        = display_h_ * 25 / 100;  // 60px on 240 — tall oval
    pupil_size_   = eye_w_ * 55 / 100;      // 29px — big pupil
    eye_spacing_  = display_w_ * 18 / 100;  // 43px from center
    eye_y_        = -display_h_ * 8 / 100;  // -19px above center
    mouth_w_      = display_w_ * 30 / 100;  // 72px wide mouth
    mouth_min_h_  = display_h_ * 4 / 100;   // 10px resting
    mouth_max_h_  = display_h_ * 20 / 100;  // 48px max open
    mouth_y_      = display_h_ * 18 / 100;  // 43px below center
    blink_h_      = display_h_ * 2 / 100;   // 5px squint line

    // Container centered in parent
    container_ = lv_obj_create(parent);
    lv_obj_set_size(container_, width, height);
    lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_scrollbar_mode(container_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_center(container_);

    // --- Left eye: white sclera ---
    left_sclera_ = lv_obj_create(container_);
    lv_obj_set_size(left_sclera_, eye_w_, eye_h_);
    lv_obj_set_style_radius(left_sclera_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(left_sclera_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(left_sclera_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(left_sclera_, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(left_sclera_, 2, 0);
    lv_obj_set_scrollbar_mode(left_sclera_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(left_sclera_, 0, 0);
    lv_obj_align(left_sclera_, LV_ALIGN_CENTER, -eye_spacing_, eye_y_);

    // Left pupil (inside sclera)
    left_pupil_ = lv_obj_create(left_sclera_);
    lv_obj_set_size(left_pupil_, pupil_size_, pupil_size_);
    lv_obj_set_style_radius(left_pupil_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(left_pupil_, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(left_pupil_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(left_pupil_, 0, 0);
    lv_obj_set_scrollbar_mode(left_pupil_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_center(left_pupil_);

    // --- Right eye: white sclera ---
    right_sclera_ = lv_obj_create(container_);
    lv_obj_set_size(right_sclera_, eye_w_, eye_h_);
    lv_obj_set_style_radius(right_sclera_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(right_sclera_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(right_sclera_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(right_sclera_, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(right_sclera_, 2, 0);
    lv_obj_set_scrollbar_mode(right_sclera_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(right_sclera_, 0, 0);
    lv_obj_align(right_sclera_, LV_ALIGN_CENTER, eye_spacing_, eye_y_);

    // Right pupil
    right_pupil_ = lv_obj_create(right_sclera_);
    lv_obj_set_size(right_pupil_, pupil_size_, pupil_size_);
    lv_obj_set_style_radius(right_pupil_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(right_pupil_, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(right_pupil_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(right_pupil_, 0, 0);
    lv_obj_set_scrollbar_mode(right_pupil_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_center(right_pupil_);

    // --- Mouth: smile (resting) via clipped circle ---
    // A small clip container shows only the bottom arc of a large circle = smile curve
    int smile_h = mouth_w_ / 4;  // ~18px visible curve height
    mouth_smile_ = lv_obj_create(container_);
    lv_obj_set_size(mouth_smile_, mouth_w_ + 4, smile_h);
    lv_obj_set_style_bg_opa(mouth_smile_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mouth_smile_, 0, 0);
    lv_obj_set_style_pad_all(mouth_smile_, 0, 0);
    lv_obj_set_scrollbar_mode(mouth_smile_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(mouth_smile_, LV_ALIGN_CENTER, 0, mouth_y_);

    // Large circle inside clip — only bottom arc is visible
    lv_obj_t* smile_arc = lv_obj_create(mouth_smile_);
    lv_obj_set_size(smile_arc, mouth_w_, mouth_w_);
    lv_obj_set_style_radius(smile_arc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(smile_arc, LV_OPA_TRANSP, 0);      // transparent fill
    lv_obj_set_style_border_color(smile_arc, lv_color_hex(0x2C2C2C), 0);
    lv_obj_set_style_border_width(smile_arc, 3, 0);              // outline only
    lv_obj_set_scrollbar_mode(smile_arc, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(smile_arc, 0, 0);
    lv_obj_align(smile_arc, LV_ALIGN_BOTTOM_MID, 0, 0);

    // --- Mouth: open oval (speaking, hidden initially) ---
    mouth_open_ = lv_obj_create(container_);
    lv_obj_set_size(mouth_open_, mouth_w_, mouth_min_h_);
    lv_obj_set_style_radius(mouth_open_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(mouth_open_, lv_color_hex(0x2C2C2C), 0);
    lv_obj_set_style_bg_opa(mouth_open_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(mouth_open_, 0, 0);
    lv_obj_set_scrollbar_mode(mouth_open_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(mouth_open_, 0, 0);
    lv_obj_align(mouth_open_, LV_ALIGN_CENTER, 0, mouth_y_);
    lv_obj_add_flag(mouth_open_, LV_OBJ_FLAG_HIDDEN);

    // Start animation timer (50ms = 20 FPS)
    esp_timer_create_args_t timer_args = {
        .callback = TimerCb,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "face_anim",
        .skip_unhandled_events = true,
    };
    esp_timer_create(&timer_args, &timer_);
    esp_timer_start_periodic(timer_, 50000); // 50ms

    visible_ = true;
    ESP_LOGI(TAG, "Face animation created (%dx%d) eye=%dx%d pupil=%d mouth=%d",
             width, height, eye_w_, eye_h_, pupil_size_, mouth_w_);
}

FaceAnimation::~FaceAnimation() {
    if (timer_) {
        esp_timer_stop(timer_);
        esp_timer_delete(timer_);
    }
}

void FaceAnimation::Show() {
    visible_ = true;
    need_visibility_update_ = true;
}

void FaceAnimation::Hide() {
    visible_ = false;
    need_visibility_update_ = true;
}

void FaceAnimation::SetSpeaking(bool speaking) {
    speaking_ = speaking;
    if (!speaking) {
        audio_level_.store(0);
    }
}

void FaceAnimation::SetListening(bool listening) {
    listening_ = listening;
}

void FaceAnimation::SetAudioLevel(int level) {
    audio_level_.store(level);
}

void FaceAnimation::TimerCb(void* arg) {
    auto* self = static_cast<FaceAnimation*>(arg);
    self->OnTick();
}

void FaceAnimation::OnTick() {
    if (!lvgl_port_lock(0)) return;

    // Handle visibility changes
    if (need_visibility_update_) {
        need_visibility_update_ = false;
        if (visible_) {
            lv_obj_remove_flag(container_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (!visible_) {
        lvgl_port_unlock();
        return;
    }

    // --- Mouth animation ---
    if (speaking_) {
        // Speaking: show open oval, hide smile
        lv_obj_add_flag(mouth_smile_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(mouth_open_, LV_OBJ_FLAG_HIDDEN);
        int level = audio_level_.load();
        smooth_level_ = (smooth_level_ * 3 + level) / 4;
        int mouth_h = mouth_min_h_ + smooth_level_ * (mouth_max_h_ - mouth_min_h_) / 100;
        if (mouth_h < mouth_min_h_ + 2) mouth_h = mouth_min_h_ + 2;
        int mouth_cur_w = mouth_w_ - smooth_level_ * (mouth_w_ / 5) / 100;
        lv_obj_set_size(mouth_open_, mouth_cur_w, mouth_h);
        lv_obj_set_style_radius(mouth_open_, LV_RADIUS_CIRCLE, 0);
        lv_obj_align(mouth_open_, LV_ALIGN_CENTER, 0, mouth_y_);
    } else {
        // Resting: show smile curve, hide open oval
        lv_obj_remove_flag(mouth_smile_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(mouth_open_, LV_OBJ_FLAG_HIDDEN);
        smooth_level_ = 0;
    }

    // --- Eye blink animation ---
    if (blink_ticks_ > 0) {
        blink_ticks_--;
        if (blink_ticks_ == 0) {
            // Blink done — restore eyes
            int h = listening_ ? eye_h_ + 4 : eye_h_;
            lv_obj_set_height(left_sclera_, h);
            lv_obj_set_height(right_sclera_, h);
            lv_obj_align(left_sclera_, LV_ALIGN_CENTER, -eye_spacing_, eye_y_);
            lv_obj_align(right_sclera_, LV_ALIGN_CENTER, eye_spacing_, eye_y_);
            // Show pupils again
            lv_obj_remove_flag(left_pupil_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(right_pupil_, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        blink_counter_++;
        if (blink_counter_ >= next_blink_) {
            // Start blink — squish sclera to thin line, hide pupils
            blink_ticks_ = kBlinkDurationTicks;
            blink_counter_ = 0;
            next_blink_ = 50 + (rand() % 40); // 2.5-4.5 seconds
            lv_obj_set_height(left_sclera_, blink_h_);
            lv_obj_set_height(right_sclera_, blink_h_);
            lv_obj_align(left_sclera_, LV_ALIGN_CENTER, -eye_spacing_, eye_y_);
            lv_obj_align(right_sclera_, LV_ALIGN_CENTER, eye_spacing_, eye_y_);
            lv_obj_add_flag(left_pupil_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(right_pupil_, LV_OBJ_FLAG_HIDDEN);
        } else if (listening_) {
            // Listening: slightly wider eyes
            lv_obj_set_height(left_sclera_, eye_h_ + 4);
            lv_obj_set_height(right_sclera_, eye_h_ + 4);
        }
    }

    // --- Pupil drift (subtle movement for liveliness) ---
    if (blink_ticks_ == 0) {
        drift_counter_++;
        if (drift_ticks_ > 0) {
            drift_ticks_--;
            if (drift_ticks_ == 0) {
                // Return to center
                drift_x_ = 0;
                drift_y_ = 0;
            }
        } else if (drift_counter_ >= next_drift_) {
            // New random drift
            drift_counter_ = 0;
            next_drift_ = 30 + (rand() % 60); // 1.5-4.5 seconds
            drift_ticks_ = 10 + (rand() % 10); // hold for 0.5-1 sec
            int max_drift = pupil_size_ / 3;
            drift_x_ = (rand() % (max_drift * 2 + 1)) - max_drift;
            drift_y_ = (rand() % (max_drift + 1)) - max_drift / 2;
        }
        lv_obj_align(left_pupil_, LV_ALIGN_CENTER, drift_x_, drift_y_);
        lv_obj_align(right_pupil_, LV_ALIGN_CENTER, drift_x_, drift_y_);
    }

    lvgl_port_unlock();
}
