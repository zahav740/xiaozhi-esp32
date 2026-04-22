#ifndef ROKI_FACE_H
#define ROKI_FACE_H

#include <lvgl.h>
#include <misc/lv_timer_private.h>
#include <esp_timer.h>
#include <atomic>
#include <cmath>

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
    // Server-driven viseme hint (0..9). amplitude is 0..255.
    // Latest value is latched atomically and used by DrawMouthOverlay.
    void SetViseme(int viseme_id, int amplitude);

    enum Viseme {
        VIS_SIL = 0, VIS_PP, VIS_FF, VIS_SS, VIS_NN,
        VIS_AA, VIS_EE, VIS_II, VIS_OO, VIS_UU,
        VIS_COUNT
    };

private:
    lv_obj_t* canvas_ = nullptr;
    lv_draw_buf_t* dbuf_ = nullptr;
    int w_ = 0, h_ = 0;
    // "Mouthless" copy of the happy sprite (flash copy with the painted
    // smile erased out of the mouth region). Built once in Create().
    uint16_t* happy_clean_ = nullptr;

    Emotion emo_ = HAPPY;
    std::atomic<bool> speaking_{false};
    std::atomic<int> amp_{0};
    bool mouth_open_ = false;
    int prev_open_amount_ = 0;

    // Blink animation
    std::atomic<int> blink_frames_{0};  // Countdown frames for blink
    esp_timer_handle_t blink_tmr_ = nullptr;
    bool was_blinking_ = false;

    // Счётчик подряд идущих тихих аудио-чанков (для биаса моргания в паузах речи)
    std::atomic<int> quiet_frames_{0};

    // Server-driven viseme (0..9) + amplitude. last_viseme_ts_us_ is used to
    // fall back to amplitude-based drawing if the server stops emitting hints.
    std::atomic<int> viseme_{0};
    std::atomic<int> viseme_amp_{0};
    std::atomic<uint64_t> last_viseme_ts_us_{0};

    esp_timer_handle_t lip_tmr_ = nullptr;
    lv_timer_t* render_lv_tmr_ = nullptr;
    std::atomic<bool> dirty_{true};

    void Render();
    const uint8_t* GetFaceData(Emotion e);
    void DrawEllipse(uint16_t* buf, int cx, int cy, int rx, int ry, uint16_t color);
    void DrawMouthOverlay(int open_amount);
    void DrawBlinkOverlay();

    static void OnLvRender(lv_timer_t* t);
    static void OnLipSync(void* a);
    static void OnBlinkTimer(void* a);
};

#endif
