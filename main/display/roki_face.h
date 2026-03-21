#ifndef ROKI_FACE_H
#define ROKI_FACE_H

#include <lvgl.h>
#include <misc/lv_timer_private.h>
#include <esp_timer.h>
#include <atomic>

class RokiFace {
public:
    enum Emotion {
        HAPPY, SAD, SURPRISED, ANGRY, EXCITED, LAUGH,
        WINK, WORRIED, SILLY, SMIRK, DISGUSTED, SCARED,
        EMO_COUNT
    };

    RokiFace();
    ~RokiFace();

    void Create(lv_obj_t* parent, int w, int h);
    void SetEmotion(Emotion e);
    void SetEmotion(const char* str);
    void SetSpeaking(bool on);
    void FeedAmplitude(const int16_t* pcm, size_t count);

private:
    lv_obj_t* canvas_ = nullptr;
    lv_draw_buf_t* dbuf_ = nullptr;
    int w_ = 0, h_ = 0;

    Emotion emo_ = HAPPY;
    std::atomic<bool> speaking_{false};
    std::atomic<int> amp_{0};
    bool mouth_open_ = false;  // current display state

    esp_timer_handle_t lip_tmr_ = nullptr;
    lv_timer_t* render_lv_tmr_ = nullptr;
    std::atomic<bool> dirty_{true};

    void Render();
    const uint8_t* GetFaceData(Emotion e);

    static void OnLvRender(lv_timer_t* t);
    static void OnLipSync(void* a);
};

#endif
