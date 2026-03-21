#include "roki_face.h"
#include <esp_log.h>
#include <cstring>
#include <algorithm>

#define TAG "RokiFace"

extern const uint8_t _binary_happy_bin_start[]     asm("_binary_happy_bin_start");
extern const uint8_t _binary_sad_bin_start[]       asm("_binary_sad_bin_start");
extern const uint8_t _binary_angry_bin_start[]     asm("_binary_angry_bin_start");
extern const uint8_t _binary_surprised_bin_start[] asm("_binary_surprised_bin_start");
extern const uint8_t _binary_excited_bin_start[]   asm("_binary_excited_bin_start");
extern const uint8_t _binary_wink_bin_start[]      asm("_binary_wink_bin_start");
extern const uint8_t _binary_worried_bin_start[]   asm("_binary_worried_bin_start");

static const size_t FACE_SIZE = 240 * 240 * 2;

const uint8_t* RokiFace::GetFaceData(Emotion e) {
    switch (e) {
        case SAD:       return _binary_sad_bin_start;
        case ANGRY:     return _binary_angry_bin_start;
        case SURPRISED: return _binary_surprised_bin_start;
        case EXCITED:   return _binary_excited_bin_start;
        case LOVE:      return _binary_wink_bin_start;
        case SLEEPY:    return _binary_worried_bin_start;
        default:        return _binary_happy_bin_start;
    }
}

RokiFace::RokiFace() {}

RokiFace::~RokiFace() {
    if (lip_tmr_) { esp_timer_stop(lip_tmr_); esp_timer_delete(lip_tmr_); }
    if (render_lv_tmr_) lv_timer_delete(render_lv_tmr_);
    if (dbuf_) lv_draw_buf_destroy(dbuf_);
}

void RokiFace::Create(lv_obj_t* parent, int w, int h) {
    w_ = w; h_ = h;
    dbuf_ = lv_draw_buf_create(w, h, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    if (!dbuf_) { ESP_LOGE(TAG, "alloc fail"); return; }
    canvas_ = lv_canvas_create(parent);
    lv_canvas_set_draw_buf(canvas_, dbuf_);
    lv_obj_center(canvas_);

    memcpy(dbuf_->data, GetFaceData(HAPPY), FACE_SIZE);
    lv_obj_invalidate(canvas_);

    render_lv_tmr_ = lv_timer_create(OnLvRender, 33, this);  // 30 FPS

    esp_timer_create_args_t a = {
        .callback = OnLipSync, .arg = this,
        .dispatch_method = ESP_TIMER_TASK, .name = "lip"
    };
    esp_timer_create(&a, &lip_tmr_);

    ESP_LOGI(TAG, "Face ready");
}

void RokiFace::SetEmotion(Emotion e) {
    if (e != emo_) { emo_ = e; dirty_ = true; }
}

void RokiFace::SetEmotion(const char* s) {
    if (!s) return;
    Emotion e = HAPPY;
    if      (strcmp(s, "happy") == 0 || strcmp(s, "neutral") == 0) e = HAPPY;
    else if (strcmp(s, "sad") == 0)        e = SAD;
    else if (strcmp(s, "angry") == 0)      e = ANGRY;
    else if (strcmp(s, "surprised") == 0)  e = SURPRISED;
    else if (strcmp(s, "excited") == 0 || strcmp(s, "laughing") == 0) e = EXCITED;
    else if (strcmp(s, "love") == 0 || strcmp(s, "shy") == 0) e = LOVE;
    else if (strcmp(s, "sleepy") == 0 || strcmp(s, "confused") == 0) e = SLEEPY;
    else if (strcmp(s, "worried") == 0 || strcmp(s, "scared") == 0) e = SLEEPY;
    SetEmotion(e);
}

void RokiFace::SetSpeaking(bool on) {
    bool was = speaking_.exchange(on);
    if (on && !was) {
        amp_ = 0; mouth_open_ = false;
        esp_timer_start_periodic(lip_tmr_, 40000);  // 25 FPS
    } else if (!on && was) {
        esp_timer_stop(lip_tmr_);
        amp_ = 0; mouth_open_ = false;
        dirty_ = true;
    }
}

void RokiFace::FeedAmplitude(const int16_t* pcm, size_t n) {
    if (!speaking_ || n == 0) return;
    int32_t max_v = 0;
    for (size_t i = 0; i < n; i++) {
        int32_t v = pcm[i] < 0 ? -pcm[i] : pcm[i];
        if (v > max_v) max_v = v;
    }
    int prev = amp_.load();
    if (max_v > prev) {
        amp_ = (max_v * 70 + prev * 30) / 100;
    } else {
        amp_ = (max_v * 15 + prev * 85) / 100;
    }
}

// Just swap between original face images — no geometric overlays
void RokiFace::Render() {
    if (!canvas_ || !dbuf_) return;

    bool should_open = speaking_ && (amp_.load() > 500);

    if (should_open && !mouth_open_) {
        // Open mouth: show excited face
        memcpy(dbuf_->data, _binary_excited_bin_start, FACE_SIZE);
        mouth_open_ = true;
        lv_obj_invalidate(canvas_);
    } else if (!should_open && mouth_open_) {
        // Close mouth: show current emotion face
        memcpy(dbuf_->data, GetFaceData(emo_), FACE_SIZE);
        mouth_open_ = false;
        lv_obj_invalidate(canvas_);
    } else if (dirty_.exchange(false)) {
        // Emotion changed
        memcpy(dbuf_->data, GetFaceData(emo_), FACE_SIZE);
        mouth_open_ = false;
        lv_obj_invalidate(canvas_);
    }
}

void RokiFace::OnLvRender(lv_timer_t* t) {
    ((RokiFace*)t->user_data)->Render();
}

void RokiFace::OnLipSync(void* a) {
    auto* f = (RokiFace*)a;
    f->amp_ = (f->amp_.load() * 85) / 100;  // Decay
}
