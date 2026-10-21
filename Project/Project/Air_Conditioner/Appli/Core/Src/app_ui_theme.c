#include "app_ui_theme.h"
#include "app_ui_port.h"

#include <string.h>

LV_FONT_DECLARE(app_font_ui_16)

static bool s_initialized;
static lv_style_t s_screen;
static lv_style_t s_transparent;
static lv_style_t s_card;
static lv_style_t s_label;
static lv_style_t s_caption;
static lv_style_t s_button;
static lv_style_t s_button_checked;
static lv_style_t s_button_pressed;
static lv_style_t s_button_checked_pressed;
static lv_style_t s_button_disabled;
static lv_style_t s_row;
static lv_style_t s_muted;
static lv_style_t s_accent;
static lv_style_t s_caution;

static void passive(lv_obj_t *object)
{
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE |
        LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
}

void app_ui_theme_init(void)
{
    if (s_initialized) { return; }

    lv_style_init(&s_screen);
    lv_style_set_pad_all(&s_screen, 0);
    lv_style_set_border_width(&s_screen, 0);
    lv_style_set_bg_color(&s_screen, lv_color_hex(APP_UI_COLOR_BACKGROUND));
    lv_style_set_bg_opa(&s_screen, LV_OPA_COVER);
    lv_style_set_text_font(&s_screen, &app_font_ui_16);
    lv_style_set_text_color(&s_screen, lv_color_hex(APP_UI_COLOR_TEXT));

    lv_style_init(&s_transparent);
    lv_style_set_bg_opa(&s_transparent, LV_OPA_TRANSP);

    lv_style_init(&s_card);
    lv_style_set_pad_all(&s_card, 0);
    lv_style_set_radius(&s_card, 14);
    lv_style_set_bg_color(&s_card, lv_color_hex(APP_UI_COLOR_CARD));
    lv_style_set_bg_opa(&s_card, LV_OPA_COVER);
    lv_style_set_border_color(&s_card, lv_color_hex(APP_UI_COLOR_BORDER));
    lv_style_set_border_width(&s_card, 1);
    lv_style_set_shadow_width(&s_card, 0);

    lv_style_init(&s_label);
    lv_style_set_text_font(&s_label, &app_font_ui_16);
    lv_style_set_text_color(&s_label, lv_color_hex(APP_UI_COLOR_TEXT));

    /* Captions inherit button state colors, especially disabled text. */
    lv_style_init(&s_caption);
    lv_style_set_text_font(&s_caption, &app_font_ui_16);
    lv_style_set_text_align(&s_caption, LV_TEXT_ALIGN_CENTER);

    lv_style_init(&s_button);
    lv_style_set_pad_all(&s_button, 0);
    lv_style_set_radius(&s_button, 10);
    lv_style_set_bg_color(&s_button, lv_color_hex(APP_UI_COLOR_BUTTON));
    lv_style_set_bg_opa(&s_button, LV_OPA_COVER);
    lv_style_set_border_width(&s_button, 0);
    lv_style_set_shadow_width(&s_button, 0);
    lv_style_set_text_color(&s_button, lv_color_hex(APP_UI_COLOR_TEXT));

    lv_style_init(&s_button_checked);
    lv_style_set_bg_color(&s_button_checked, lv_color_hex(APP_UI_COLOR_BUTTON_ON));
    lv_style_set_text_color(&s_button_checked, lv_color_hex(APP_UI_COLOR_TEXT));

    lv_style_init(&s_button_pressed);
    lv_style_set_bg_color(&s_button_pressed, lv_color_hex(APP_UI_COLOR_PRESSED));
    lv_style_set_text_color(&s_button_pressed, lv_color_hex(APP_UI_COLOR_TEXT));

    lv_style_init(&s_button_checked_pressed);
    lv_style_set_bg_color(&s_button_checked_pressed, lv_color_hex(APP_UI_COLOR_PRESSED));
    lv_style_set_text_color(&s_button_checked_pressed, lv_color_hex(APP_UI_COLOR_TEXT));

    lv_style_init(&s_button_disabled);
    lv_style_set_bg_color(&s_button_disabled, lv_color_hex(APP_UI_COLOR_BORDER));
    lv_style_set_text_color(&s_button_disabled, lv_color_hex(APP_UI_COLOR_MUTED));

    lv_style_init(&s_row);
    lv_style_set_pad_all(&s_row, 0);
    lv_style_set_pad_row(&s_row, 0);
    lv_style_set_border_width(&s_row, 0);
    lv_style_set_bg_opa(&s_row, LV_OPA_TRANSP);
    lv_style_set_layout(&s_row, LV_LAYOUT_FLEX);
    lv_style_set_flex_flow(&s_row, LV_FLEX_FLOW_ROW);
    lv_style_set_flex_main_place(&s_row, LV_FLEX_ALIGN_START);
    lv_style_set_flex_cross_place(&s_row, LV_FLEX_ALIGN_CENTER);
    lv_style_set_flex_track_place(&s_row, LV_FLEX_ALIGN_START);

    lv_style_init(&s_muted);
    lv_style_set_text_color(&s_muted, lv_color_hex(APP_UI_COLOR_MUTED));
    lv_style_init(&s_accent);
    lv_style_set_text_color(&s_accent, lv_color_hex(APP_UI_COLOR_ACCENT));
    lv_style_init(&s_caution);
    lv_style_set_text_color(&s_caution, lv_color_hex(APP_UI_COLOR_CAUTION));

    s_initialized = true;
}

lv_obj_t *app_ui_theme_screen(bool transparent)
{
    app_ui_theme_init();
    lv_obj_t *object = lv_obj_create(NULL);
    lv_obj_remove_style_all(object);
    lv_obj_add_style(object, &s_screen, LV_PART_MAIN);
    if (transparent) { lv_obj_add_style(object, &s_transparent, LV_PART_MAIN); }
    passive(object);
    return object;
}

lv_obj_t *app_ui_theme_card(lv_obj_t *parent, int32_t x, int32_t y,
                            int32_t width, int32_t height)
{
    app_ui_theme_init();
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_add_style(object, &s_card, LV_PART_MAIN);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    passive(object);
    return object;
}

lv_obj_t *app_ui_theme_label(lv_obj_t *parent, const char *lifetime_static_text,
                             int32_t x, int32_t y, int32_t width)
{
    app_ui_theme_init();
    lv_obj_t *object = lv_label_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_add_style(object, &s_label, LV_PART_MAIN);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_width(object, width);
    lv_label_set_long_mode(object, LV_LABEL_LONG_WRAP);
    lv_label_set_text_static(object, lifetime_static_text ? lifetime_static_text : "");
    passive(object);
    return object;
}

lv_obj_t *app_ui_theme_button(lv_obj_t *parent, const char *lifetime_static_text,
                              int32_t x, int32_t y, int32_t width, int32_t height)
{
    app_ui_theme_init();
    lv_obj_t *object = lv_button_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_add_style(object, &s_button, LV_PART_MAIN);
    lv_obj_add_style(object, &s_button_checked, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_add_style(object, &s_button_pressed, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_style(object, &s_button_checked_pressed,
        LV_PART_MAIN | LV_STATE_CHECKED | LV_STATE_PRESSED);
    lv_obj_add_style(object, &s_button_disabled, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CHECKABLE |
        LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *caption = lv_label_create(object);
    lv_obj_remove_style_all(caption);
    lv_obj_add_style(caption, &s_caption, LV_PART_MAIN);
    lv_obj_set_width(caption, width > 12 ? width - 12 : width);
    lv_label_set_long_mode(caption, LV_LABEL_LONG_WRAP);
    lv_label_set_text_static(caption, lifetime_static_text ? lifetime_static_text : "");
    passive(caption);
    lv_obj_center(caption);
    return object;
}

void app_ui_theme_checked(lv_obj_t *button, bool checked)
{
    if (button && lv_obj_has_state(button, LV_STATE_CHECKED) != checked)
    { lv_obj_set_state(button, LV_STATE_CHECKED, checked); }
}

void app_ui_theme_text(lv_obj_t *label, const char *text)
{
    if (!label) { return; }
    if (!text) { text = ""; }
    if (strcmp(lv_label_get_text(label), text) != 0)
    { lv_label_set_text(label, text); }
}

lv_obj_t *app_ui_theme_row(lv_obj_t *parent, int32_t x, int32_t y,
                           int32_t width, int32_t height, int32_t gap)
{
    app_ui_theme_init();
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_add_style(object, &s_row, LV_PART_MAIN);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_pad_column(object, gap, LV_PART_MAIN);
    passive(object);
    return object;
}

void app_ui_theme_text_muted(lv_obj_t *object)
{
    app_ui_theme_init();
    if (object) { lv_obj_add_style(object, &s_muted, LV_PART_MAIN); }
}

void app_ui_theme_text_accent(lv_obj_t *object)
{
    app_ui_theme_init();
    if (object) { lv_obj_add_style(object, &s_accent, LV_PART_MAIN); }
}

void app_ui_theme_text_caution(lv_obj_t *object)
{
    app_ui_theme_init();
    if (object) { lv_obj_add_style(object, &s_caution, LV_PART_MAIN); }
}
