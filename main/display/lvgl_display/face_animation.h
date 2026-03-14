#ifndef FACE_ANIMATION_H
#define FACE_ANIMATION_H

#include <atomic>
#include <lvgl.h>
#include <esp_timer.h>

class FaceAnimation {
public:
    FaceAnimation(lv_obj_t* parent, int width, int height);
    ~FaceAnimation();

    void Show();
    void Hide();
    void SetSpeaking(bool speaking);
    void SetListening(bool listening);
    void SetAudioLevel(int level); // 0-100

private:
    lv_obj_t* container_ = nullptr;

    // Eyes: white sclera + black pupil
    lv_obj_t* left_sclera_ = nullptr;
    lv_obj_t* right_sclera_ = nullptr;
    lv_obj_t* left_pupil_ = nullptr;
    lv_obj_t* right_pupil_ = nullptr;

    // Mouth: smile arc (resting) + oval (speaking)
    lv_obj_t* mouth_smile_ = nullptr;   // arc for smile
    lv_obj_t* mouth_open_ = nullptr;    // oval for speaking

    esp_timer_handle_t timer_ = nullptr;

    std::atomic<int> audio_level_{0};
    bool speaking_ = false;
    bool listening_ = false;
    bool visible_ = false;
    bool need_visibility_update_ = false;

    // Display dimensions for proportional sizing
    int display_w_ = 240;
    int display_h_ = 240;

    // Blink state
    int blink_counter_ = 0;
    int next_blink_ = 60;
    int blink_ticks_ = 0;

    // Pupil drift (subtle eye movement for liveliness)
    int drift_counter_ = 0;
    int next_drift_ = 40;
    int drift_x_ = 0;
    int drift_y_ = 0;
    int drift_ticks_ = 0;

    // Computed sizes (proportional to display)
    int eye_w_;           // sclera width
    int eye_h_;           // sclera height
    int pupil_size_;      // pupil diameter
    int eye_spacing_;     // from center
    int eye_y_;           // vertical position
    int mouth_w_;         // mouth width
    int mouth_min_h_;     // mouth resting height
    int mouth_max_h_;     // mouth max open height
    int mouth_y_;         // mouth vertical position
    int blink_h_;         // eye height when blinking

    static constexpr int kBlinkDurationTicks = 3;  // 150ms

    void OnTick();
    static void TimerCb(void* arg);
};

#endif // FACE_ANIMATION_H
