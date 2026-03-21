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
extern const uint8_t _binary_laugh_bin_start[]     asm("_binary_laugh_bin_start");
extern const uint8_t _binary_wink_bin_start[]      asm("_binary_wink_bin_start");
extern const uint8_t _binary_worried_bin_start[]   asm("_binary_worried_bin_start");
extern const uint8_t _binary_silly_bin_start[]     asm("_binary_silly_bin_start");
extern const uint8_t _binary_smirk_bin_start[]     asm("_binary_smirk_bin_start");
extern const uint8_t _binary_disgusted_bin_start[] asm("_binary_disgusted_bin_start");
extern const uint8_t _binary_scared_bin_start[]    asm("_binary_scared_bin_start");

static const size_t FACE_SIZE = 240 * 240 * 2;

const uint8_t* RokiFace::GetFaceData(Emotion e) {
    switch (e) {
        case SAD:       return _binary_sad_bin_start;
        case ANGRY:     return _binary_angry_bin_start;
        case SURPRISED: return _binary_surprised_bin_start;
        case EXCITED:   return _binary_excited_bin_start;
        case LAUGH:     return _binary_laugh_bin_start;
        case WINK:      return _binary_wink_bin_start;
        case WORRIED:   return _binary_worried_bin_start;
        case SILLY:     return _binary_silly_bin_start;
        case SMIRK:     return _binary_smirk_bin_start;
        case DISGUSTED: return _binary_disgusted_bin_start;
        case SCARED:    return _binary_scared_bin_start;
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
    else if (strcmp(s, "excited") == 0)    e = EXCITED;
    else if (strcmp(s, "laughing") == 0)   e = LAUGH;
    else if (strcmp(s, "winking") == 0)    e = WINK;
    else if (strcmp(s, "thinking") == 0 || strcmp(s, "confused") == 0) e = WORRIED;
    else if (strcmp(s, "silly") == 0)      e = SILLY;
    else if (strcmp(s, "cool") == 0)       e = SMIRK;
    else if (strcmp(s, "scared") == 0)     e = SCARED;
    else if (strcmp(s, "disgusted") == 0)  e = DISGUSTED;
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

// Draw a filled ellipse on the RGB565 buffer
void RokiFace::DrawEllipse(uint16_t* buf, int cx, int cy, int rx, int ry, uint16_t color) {
    int x0 = std::max(0, cx - rx);
    int x1 = std::min(w_ - 1, cx + rx);
    int y0 = std::max(0, cy - ry);
    int y1 = std::min(h_ - 1, cy + ry);
    for (int y = y0; y <= y1; y++) {
        int dy = y - cy;
        // Ellipse equation: (dx/rx)^2 + (dy/ry)^2 <= 1
        // dx^2 <= rx^2 * (1 - dy^2/ry^2) = rx^2 * (ry^2 - dy^2) / ry^2
        int span_sq = (int64_t)rx * rx * (ry * ry - dy * dy) / (ry * ry);
        if (span_sq < 0) continue;
        int span = (int)sqrtf((float)span_sq);
        int lx = std::max(x0, cx - span);
        int hx = std::min(x1, cx + span);
        for (int x = lx; x <= hx; x++) {
            buf[y * w_ + x] = color;
        }
    }
}

void RokiFace::DrawMouthOverlay(int open_amount) {
    uint16_t* buf = (uint16_t*)dbuf_->data;
    // Mouth center at (120, 185) on 240x240 face
    int cx = 120, cy = 185;
    int rx = 16 + open_amount / 3;  // wider when more open
    int ry = 4 + open_amount;       // height driven by amplitude

    // Dark mouth interior (RGB565: R=3, G=1, B=2 → very dark reddish)
    uint16_t mouth_color = (3 << 11) | (1 << 5) | 2;
    DrawEllipse(buf, cx, cy, rx, ry, mouth_color);

    // Lighter tongue hint at bottom (RGB565: R=22, G=8, B=8 → pinkish)
    if (ry > 8) {
        uint16_t tongue_color = (22 << 11) | (8 << 5) | 8;
        DrawEllipse(buf, cx, cy + ry / 2, rx / 2, ry / 4, tongue_color);
    }
}

void RokiFace::Render() {
    if (!canvas_ || !dbuf_) return;

    int amp = amp_.load();
    bool should_open = speaking_ && (amp > 500);
    // Map amplitude (500..10000) to mouth open amount (1..18)
    int open_amount = should_open ? std::min(18, std::max(1, (amp - 500) / 530)) : 0;

    if (should_open && !mouth_open_) {
        // Start talking: redraw face with mouth overlay
        memcpy(dbuf_->data, GetFaceData(emo_), FACE_SIZE);
        DrawMouthOverlay(open_amount);
        mouth_open_ = true;
        prev_open_amount_ = open_amount;
        lv_obj_invalidate(canvas_);
    } else if (should_open && mouth_open_ && open_amount != prev_open_amount_) {
        // Mouth size changed: redraw
        memcpy(dbuf_->data, GetFaceData(emo_), FACE_SIZE);
        DrawMouthOverlay(open_amount);
        prev_open_amount_ = open_amount;
        lv_obj_invalidate(canvas_);
    } else if (!should_open && mouth_open_) {
        // Close mouth: restore original face
        memcpy(dbuf_->data, GetFaceData(emo_), FACE_SIZE);
        mouth_open_ = false;
        prev_open_amount_ = 0;
        lv_obj_invalidate(canvas_);
    } else if (dirty_.exchange(false)) {
        // Emotion changed
        memcpy(dbuf_->data, GetFaceData(emo_), FACE_SIZE);
        mouth_open_ = false;
        prev_open_amount_ = 0;
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
