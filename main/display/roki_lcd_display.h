#ifndef ROKI_LCD_DISPLAY_H
#define ROKI_LCD_DISPLAY_H

#include "lcd_display.h"
#include "roki_face.h"
#include <memory>

class RokiLcdDisplay : public SpiLcdDisplay {
public:
    RokiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy);

    virtual void SetupUI() override;
    virtual void SetEmotion(const char* emotion) override;
    void SetSpeaking(bool speaking);
    void FeedAudioAmplitude(const int16_t* data, size_t count);

private:
    std::unique_ptr<RokiFace> face_;
};

#endif
