#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "lvgl_display.h"
#include "gif/lvgl_gif.h"
#include "face_animation.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <font_emoji.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#define PREVIEW_IMAGE_DURATION_MS 5000


class LcdDisplay : public LvglDisplay {
protected:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    
    lv_draw_buf_t draw_buf_;
    lv_obj_t* top_bar_ = nullptr;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;
    lv_obj_t* bottom_bar_ = nullptr;
    lv_obj_t* preview_image_ = nullptr;
    lv_obj_t* emoji_label_ = nullptr;
    lv_obj_t* emoji_image_ = nullptr;
    std::unique_ptr<LvglGif> gif_controller_ = nullptr;
    std::unique_ptr<FaceAnimation> face_ = nullptr;
    lv_obj_t* emoji_box_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;
    esp_timer_handle_t preview_timer_ = nullptr;
    std::unique_ptr<LvglImage> preview_image_cached_ = nullptr;
    bool hide_subtitle_ = false;  // Control whether to hide chat messages/subtitles

public:
    // Profile selector panel
    static constexpr int kProfileCount = 3;

private:
    lv_obj_t* profile_panel_ = nullptr;
    lv_obj_t* profile_rows_[kProfileCount] = {};
    lv_obj_t* profile_labels_[kProfileCount] = {};
    lv_obj_t* profile_indicators_[kProfileCount] = {};
    int current_profile_index_ = 0;
    bool profile_panel_visible_ = false;
    std::function<void(int index, const char* id)> on_profile_selected_;
    void CreateProfilePanel();
    void UpdateProfileHighlight();

    void InitializeLcdThemes();
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

protected:
    // Add protected constructor
    LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height);
    
public:
    ~LcdDisplay();
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void ClearChatMessages() override;
    virtual void SetPreviewImage(std::unique_ptr<LvglImage> image) override;
    virtual void SetupUI() override;
    // Add theme switching function
    virtual void SetTheme(Theme* theme) override;
    
    // Set whether to hide chat messages/subtitles
    void SetHideSubtitle(bool hide);

    // Face animation controls
    void SetFaceSpeaking(bool speaking);
    void UpdateFaceAudioLevel(int level);

    // Profile selector
    void ShowProfileSelector();
    void HideProfileSelector();
    void SelectProfile(int index);
    bool HandleProfileTap(int touch_y);  // Returns true if tap consumed by profile panel
    int GetSelectedProfile() const { return current_profile_index_; }
    bool IsProfilePanelVisible() const { return profile_panel_visible_; }
    void SetOnProfileSelected(std::function<void(int index, const char* id)> callback) { on_profile_selected_ = callback; }
};

// SPI LCD display
class SpiLcdDisplay : public LcdDisplay {
public:
    SpiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy);
};

// RGB LCD display
class RgbLcdDisplay : public LcdDisplay {
public:
    RgbLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy);
};

// MIPI LCD display
class MipiLcdDisplay : public LcdDisplay {
public:
    MipiLcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy);
};

#endif // LCD_DISPLAY_H
