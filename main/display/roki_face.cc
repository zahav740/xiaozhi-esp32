#include "roki_face.h"
#include <esp_log.h>
#include <esp_random.h>
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

// Face feature positions (adjusted for 88% scale + offset in enhanced sprites)
// Original face: 240x240 with white bg
// Enhanced: scaled to 88%, placed at offset (14, 16) on yellow circle
static const int MOUTH_CX = 120;
static const int MOUTH_CY = 178;  // Was 185 in original, adjusted for scaling

// Painted mouth region of the cartoon "happy" sprite. Used for
// natural lip-sync: we copy rows from the base sprite's mouth and squash
// them vertically based on voice amplitude, preserving the original art
// style instead of drawing a synthetic ellipse on top.
static const int HAPPY_MOUTH_CX    = 118;
static const int HAPPY_MOUTH_CY    = 178;  // vertical center of painted mouth
static const int HAPPY_MOUTH_Y0    = 138;  // top of painted mouth (inclusive)
static const int HAPPY_MOUTH_Y1    = 218;  // bottom of painted mouth (inclusive)
static const int HAPPY_MOUTH_X0    = 38;   // left edge of painted mouth
static const int HAPPY_MOUTH_X1    = 198;  // right edge of painted mouth

// Eye positions for legacy emoji sprites (yellow circle style)
static const int LEFT_EYE_CX = 89;
static const int LEFT_EYE_CY = 117;
static const int RIGHT_EYE_CX = 150;
static const int RIGHT_EYE_CY = 117;
static const int EYE_RX = 20;  // Horizontal radius of eyelid
static const int EYE_RY = 18;  // Vertical radius of eyelid

// Eye positions for the new cartoon "happy" sprite (coffee-cup character).
// The sprite has much larger eyes placed near the top of the face.
static const int HAPPY_LEFT_EYE_CX  = 80;
static const int HAPPY_LEFT_EYE_CY  = 72;
static const int HAPPY_RIGHT_EYE_CX = 170;
static const int HAPPY_RIGHT_EYE_CY = 62;
static const int HAPPY_EYE_RX = 26;
static const int HAPPY_EYE_RY = 30;

// Yellow color matching the emoji circle background (RGB565 native endian)
// RGB(255, 215, 40) → R=31, G=53, B=5 → (31<<11)|(53<<5)|5 = 0xFAA5
static const uint16_t YELLOW_COLOR = (31 << 11) | (53 << 5) | 5;

// Виземо-пороги для открытия рта (соответствуют интенсивности фонемы)
static const int MOUTH_ON_THRESHOLD  = 280;   // Порог открытия (было 500 — пропускал согласные)
static const int MOUTH_MAX_AMP       = 6000;  // Выше — полное открытие
static const int MOUTH_MAX_OPEN      = 18;    // Макс. «высота» рта в пикселях
static const int SILENCE_THRESHOLD   = 180;   // Ниже — считается паузой в речи

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
    if (blink_tmr_) { esp_timer_stop(blink_tmr_); esp_timer_delete(blink_tmr_); }
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

    // Lip sync timer
    esp_timer_create_args_t lip_args = {
        .callback = OnLipSync, .arg = this,
        .dispatch_method = ESP_TIMER_TASK, .name = "lip"
    };
    esp_timer_create(&lip_args, &lip_tmr_);

    // Blink timer — first blink after 2-4 seconds
    esp_timer_create_args_t blink_args = {
        .callback = OnBlinkTimer, .arg = this,
        .dispatch_method = ESP_TIMER_TASK, .name = "blink"
    };
    esp_timer_create(&blink_args, &blink_tmr_);
    uint32_t first_blink_ms = 2000 + (esp_random() % 2000);
    esp_timer_start_once(blink_tmr_, first_blink_ms * 1000);

    ESP_LOGI(TAG, "Face ready (with blink + lip sync)");
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
        amp_ = 0; mouth_open_ = false; quiet_frames_ = 0;
        esp_timer_start_periodic(lip_tmr_, 40000);  // 25 FPS
    } else if (!on && was) {
        esp_timer_stop(lip_tmr_);
        amp_ = 0; mouth_open_ = false; quiet_frames_ = 0;
        dirty_ = true;
    }
}

void RokiFace::FeedAmplitude(const int16_t* pcm, size_t n) {
    if (!speaking_ || n == 0) return;
    // RMS — более устойчив к спайкам чем peak, ближе к восприятию громкости
    int64_t sum_sq = 0;
    for (size_t i = 0; i < n; i++) {
        int32_t s = pcm[i];
        sum_sq += (int64_t)s * s;
    }
    int rms = (int)sqrtf((float)(sum_sq / (int64_t)n));

    int prev = amp_.load();
    // Быстрая атака (естественно при открытии рта на слоге) + мягкий релиз
    if (rms > prev) {
        amp_ = (rms * 65 + prev * 35) / 100;  // attack ~ 20ms
    } else {
        amp_ = (rms * 20 + prev * 80) / 100;  // release ~ 80ms
    }

    // Считаем тихие фреймы для синхронизации моргания с паузами в речи
    if (rms < SILENCE_THRESHOLD) {
        quiet_frames_ = quiet_frames_.load() + 1;
    } else {
        quiet_frames_ = 0;
    }
}

void RokiFace::DrawEllipse(uint16_t* buf, int cx, int cy, int rx, int ry, uint16_t color) {
    int x0 = std::max(0, cx - rx);
    int x1 = std::min(w_ - 1, cx + rx);
    int y0 = std::max(0, cy - ry);
    int y1 = std::min(h_ - 1, cy + ry);
    for (int y = y0; y <= y1; y++) {
        int dy = y - cy;
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

    if (emo_ == HAPPY) {
        // Natural lip-sync for the cartoon sprite. We split the painted
        // mouth into three bands:
        //   * outer top lip curve    (~5 rows, dark brown)
        //   * inner mouth interior   (teeth / dark / tongue)
        //   * outer bottom lip curve (~5 rows, dark brown)
        // The inner interior is vertically squashed by amplitude, and the
        // outer lip curves are redrawn as fixed-thickness dark arcs that
        // track the new top/bottom edges — so the contour is always visible
        // and moves together with the mouth opening.
        const uint16_t* base = (const uint16_t*)GetFaceData(HAPPY);
        const uint16_t face_color = base[120 * w_ + w_ / 2];

        const int mcy = HAPPY_MOUTH_CY;
        const int OUTER_TOP = HAPPY_MOUTH_Y0;   // 138
        const int OUTER_BOT = HAPPY_MOUTH_Y1;   // 218
        const int INNER_TOP = 146;              // first row of teeth/interior
        const int INNER_BOT = 208;              // last row of tongue/interior
        const int inner_half_top = mcy - INNER_TOP;   // 32
        const int inner_half_bot = INNER_BOT - mcy;   // 30
        const int x0 = HAPPY_MOUTH_X0;
        const int x1 = HAPPY_MOUTH_X1;

        // Amplitude to vertical scale. Use wide dynamic range so the lip
        // contour visibly moves: near-silence => mouth nearly closed,
        // loudest => fully open painted mouth.
        float amp_norm = (float)open_amount / (float)MOUTH_MAX_OPEN;
        if (amp_norm < 0.0f) amp_norm = 0.0f;
        if (amp_norm > 1.0f) amp_norm = 1.0f;
        float scale = 0.06f + 0.94f * amp_norm;

        int new_ht = (int)(inner_half_top * scale + 0.5f);
        int new_hb = (int)(inner_half_bot * scale + 0.5f);
        if (new_ht < 2) new_ht = 2;
        if (new_hb < 2) new_hb = 2;

        const int out_top = mcy - new_ht;
        const int out_bot = mcy + new_hb;

        // Step 1: erase the full painted mouth region with face color.
        for (int y = OUTER_TOP; y <= OUTER_BOT; y++) {
            if (y < 0 || y >= h_) continue;
            uint16_t* row = buf + y * w_;
            for (int x = x0; x <= x1; x++) {
                if (x < 0 || x >= w_) continue;
                row[x] = face_color;
            }
        }

        // Step 2: copy inner mouth rows with vertical scaling.
        for (int y = out_top; y <= out_bot; y++) {
            if (y < 0 || y >= h_) continue;
            int src_y;
            if (y <= mcy) {
                int offs = mcy - y;
                int denom = new_ht ? new_ht : 1;
                src_y = mcy - (offs * inner_half_top) / denom;
            } else {
                int offs = y - mcy;
                int denom = new_hb ? new_hb : 1;
                src_y = mcy + (offs * inner_half_bot) / denom;
            }
            if (src_y < INNER_TOP) src_y = INNER_TOP;
            if (src_y > INNER_BOT) src_y = INNER_BOT;
            const uint16_t* src_row = base + src_y * w_;
            uint16_t* dst_row       = buf  + y     * w_;
            for (int x = x0; x <= x1; x++) {
                if (x < 0 || x >= w_) continue;
                dst_row[x] = src_row[x];
            }
        }

        // Step 3: draw lip contour as dark curves tracking the new mouth
        // edges. A thin ellipse segment gives the painted "smile" curve.
        // Contour has fixed pixel thickness so it stays visible at any
        // scale — this is what the user sees as "moving lips".
        const uint16_t lip_color =
            ((60 >> 3) << 11) | ((30 >> 2) << 5) | (30 >> 3);
        const int lip_cx = (x0 + x1) / 2;
        const int lip_rx = (x1 - x0) / 2 - 6;          // slight inset
        const int lip_thick = 3;                        // px thickness
        // Upper lip curve: arch slightly above out_top (eyelids-like curve)
        DrawEllipse(buf, lip_cx, out_top - 1, lip_rx, lip_thick, lip_color);
        // Lower lip curve
        DrawEllipse(buf, lip_cx, out_bot + 1, lip_rx, lip_thick, lip_color);

        // Corner highlights for a smile shape: two small dots at mouth corners
        const int corner_off = lip_rx - 2;
        DrawEllipse(buf, lip_cx - corner_off, mcy, 3, 2, lip_color);
        DrawEllipse(buf, lip_cx + corner_off, mcy, 3, 2, lip_color);
        return;
    }

    // Legacy yellow-emoji path (sad, angry, surprised, ...).
    const int mcx = MOUTH_CX;
    const int mcy = MOUTH_CY;
    DrawEllipse(buf, mcx, mcy, 38, 16, YELLOW_COLOR);

    uint16_t lip_color  = (11 << 11) | (7 << 5) | 3;
    uint16_t mouth_dark = (5  << 11) | (1 << 5) | 1;
    int rx = 14 + open_amount / 2;
    int ry = 2  + open_amount;

    if (open_amount <= 2) {
        DrawEllipse(buf, mcx, mcy, rx, 2, lip_color);
    } else {
        DrawEllipse(buf, mcx, mcy, rx + 2, ry + 2, lip_color);
        DrawEllipse(buf, mcx, mcy, rx,     ry,     mouth_dark);
        if (ry > 10) {
            uint16_t tongue = (22 << 11) | (12 << 5) | 6;
            DrawEllipse(buf, mcx, mcy + ry / 2, rx * 2 / 3, ry / 3, tongue);
        }
    }
}

void RokiFace::DrawBlinkOverlay() {
    uint16_t* buf = (uint16_t*)dbuf_->data;

    int lcx, lcy, rcx, rcy, rx, ry;
    uint16_t bg_color;

    if (emo_ == HAPPY) {
        // New cartoon sprite has cream/white face body and repositioned eyes.
        // Sample the face color from a "safe cheek" pixel in the base sprite so
        // future sprite swaps keep working without code changes.
        lcx = HAPPY_LEFT_EYE_CX;  lcy = HAPPY_LEFT_EYE_CY;
        rcx = HAPPY_RIGHT_EYE_CX; rcy = HAPPY_RIGHT_EYE_CY;
        rx  = HAPPY_EYE_RX;       ry  = HAPPY_EYE_RY;
        // Cheek sample point: just below and inward from the left eye.
        int sx = (lcx + rcx) / 2;
        int sy = (lcy + rcy) / 2 + ry + 8;
        if (sy >= h_) sy = h_ - 1;
        bg_color = ((const uint16_t*)dbuf_->data)[sy * w_ + sx];
    } else {
        lcx = LEFT_EYE_CX;  lcy = LEFT_EYE_CY;
        rcx = RIGHT_EYE_CX; rcy = RIGHT_EYE_CY;
        rx  = EYE_RX;       ry  = EYE_RY;
        bg_color = YELLOW_COLOR;
    }

    // Erase eyes with face-matching background
    DrawEllipse(buf, lcx, lcy, rx, ry, bg_color);
    DrawEllipse(buf, rcx, rcy, rx, ry, bg_color);

    // Thin dark curved line for closed eyelids
    uint16_t lid_color = (8 << 11) | (12 << 5) | 6;  // Dark brownish
    DrawEllipse(buf, lcx, lcy, rx - 2, 2, lid_color);
    DrawEllipse(buf, rcx, rcy, rx - 2, 2, lid_color);
}

void RokiFace::Render() {
    if (!canvas_ || !dbuf_) return;

    bool needs_redraw = false;
    int blink = blink_frames_.load();
    bool is_blinking = blink > 0;

    // Check if blink state changed
    if (is_blinking != was_blinking_) {
        needs_redraw = true;
        was_blinking_ = is_blinking;
    }

    int amp = amp_.load();
    // For HAPPY we always animate while speaking (even at silence between
    // syllables) so the painted mouth smoothly squashes/opens. For legacy
    // emojis we keep the original amp-threshold gating.
    bool animate_always = (emo_ == HAPPY) && speaking_.load();
    bool should_open = animate_always || (speaking_ && (amp > MOUTH_ON_THRESHOLD));
    int open_amount = 0;
    if (should_open) {
        // Линейное отображение [THRESHOLD..MAX_AMP] → [1..MOUTH_MAX_OPEN]
        int span = MOUTH_MAX_AMP - MOUTH_ON_THRESHOLD;
        int raw  = (amp - MOUTH_ON_THRESHOLD) * MOUTH_MAX_OPEN / span;
        open_amount = std::min(MOUTH_MAX_OPEN, std::max(0, raw));
        // Квантизация до 2-пиксельного шага — убираем мелкое дрожание
        open_amount = (open_amount / 2) * 2;
    }

    if (should_open && !mouth_open_) {
        needs_redraw = true;
        mouth_open_ = true;
    } else if (should_open && mouth_open_ && open_amount != prev_open_amount_) {
        needs_redraw = true;
    } else if (!should_open && mouth_open_) {
        needs_redraw = true;
        mouth_open_ = false;
    } else if (dirty_.exchange(false)) {
        needs_redraw = true;
        mouth_open_ = false;
    }

    if (needs_redraw) {
        // Always start from the base sprite
        memcpy(dbuf_->data, GetFaceData(emo_), FACE_SIZE);

        // Apply overlays
        if (is_blinking && emo_ != WINK) {
            DrawBlinkOverlay();
        }
        if (should_open) {
            DrawMouthOverlay(open_amount);
        }
        prev_open_amount_ = open_amount;
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

void RokiFace::OnBlinkTimer(void* a) {
    auto* f = (RokiFace*)a;

    int frames = f->blink_frames_.load();
    if (frames > 0) {
        // Моргание в процессе — декремент
        f->blink_frames_ = frames - 1;
        if (frames - 1 > 0) {
            esp_timer_start_once(f->blink_tmr_, 33000);
        } else {
            f->dirty_ = true;
            // Следующее моргание:
            //   - в речи: короче (1.5-3.5с), но откладываем если сейчас активная фаза
            //   - в молчании: обычное (2-5с)
            uint32_t next_ms;
            if (f->speaking_.load()) {
                int q = f->quiet_frames_.load();
                if (q < 5) {
                    // Идёт активная речь — проверим ещё через 300мс
                    next_ms = 300;
                } else {
                    next_ms = 1500 + (esp_random() % 2000);
                }
            } else {
                next_ms = 2000 + (esp_random() % 3000);
            }
            esp_timer_start_once(f->blink_tmr_, next_ms * 1000);
        }
    } else {
        // Если в речи и сейчас громкий слог — не моргаем, переоткладываем
        if (f->speaking_.load() && f->quiet_frames_.load() < 3) {
            esp_timer_start_once(f->blink_tmr_, 300000);  // 300ms retry
            return;
        }
        // Запускаем моргание: 4 кадра ≈ 130мс
        f->blink_frames_ = 4;
        esp_timer_start_once(f->blink_tmr_, 33000);
    }
}
