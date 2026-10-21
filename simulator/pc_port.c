#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>

#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <windef.h>
#include <winbase.h>
#endif

#include "app_ui_port.h"
#include "pc_port.h"
#include "lvgl.h"

#define PIXEL_COUNT (LCD_FG_WIDTH * LCD_FG_HEIGHT)

static SDL_Window *s_window;
static SDL_Renderer *s_renderer;
static SDL_Texture *s_texture;
static uint16_t s_foreground[2][PIXEL_COUNT];
static uint16_t s_background[PIXEL_COUNT];
static uint32_t s_composite[PIXEL_COUNT];
static unsigned int s_draw_index;
static bool s_test_clock;
static uint32_t s_fixed_tick;
static uint32_t s_blocked_fg_access;
static uint32_t s_camera_frame_id;
static uint32_t s_camera_captured_ms;
static bool s_camera_available;
static pc_camera_person_t s_camera_people[2];
static uint8_t s_camera_people_count;

#ifdef _WIN32
static LONG WINAPI preview_exception(EXCEPTION_POINTERS *error)
{
    uintptr_t base = (uintptr_t)GetModuleHandleW(NULL);
    fprintf(stderr, "PREVIEW exception 0x%08lX at module+0x%llX\n",
        error->ExceptionRecord->ExceptionCode,
        (unsigned long long)((uintptr_t)error->ExceptionRecord->ExceptionAddress - base));
    lv_mem_monitor_t heap;
    lv_mem_monitor(&heap);
    fprintf(stderr, "PREVIEW heap total=%zu peak=%zu free=%zu largest=%zu\n",
        heap.total_size, heap.max_used, heap.free_size, heap.free_biggest_size);
    fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

static void draw_camera_background(void)
{
    memset(s_background, 0, sizeof(s_background));
    for (uint32_t y = 0U; y < LCD_CAMERA_VIEW_HEIGHT; y++) {
        for (uint32_t x = 0U; x < LCD_CAMERA_VIEW_WIDTH; x++) {
            uint16_t red = (uint16_t)(x * 31U / (LCD_CAMERA_VIEW_WIDTH - 1U));
            uint16_t green = (uint16_t)(y * 63U / (LCD_CAMERA_VIEW_HEIGHT - 1U));
            uint16_t blue = (uint16_t)((x / 40U + y / 40U) % 2U ? 20U : 8U);
            s_background[(y + LCD_CAMERA_VIEW_Y) * LCD_FG_WIDTH + x + LCD_CAMERA_VIEW_X] =
                (uint16_t)((red << 11U) | (green << 5U) | blue);
        }
    }
    for (uint8_t i = 0U; i < s_camera_people_count; i++) {
        const pc_camera_person_t *person = &s_camera_people[i];
        if (!person->width || !person->height) { continue; }
        int32_t cx = person->x + person->width / 2;
        int32_t radius = person->width / 6;
        int32_t head_y = person->y + radius;
        for (int32_t y = person->y; y < person->y + person->height && y < LCD_FG_HEIGHT; y++) {
            for (int32_t x = person->x; x < person->x + person->width && x < LCD_FG_WIDTH; x++) {
                if (x < LCD_CAMERA_VIEW_X || x >= LCD_CAMERA_VIEW_X + LCD_CAMERA_VIEW_WIDTH) { continue; }
                int32_t dx = x - cx, dy = y - head_y;
                bool head = dx * dx + dy * dy <= radius * radius;
                bool torso = y >= head_y + radius && y < person->y + person->height * 2 / 3 &&
                    abs(dx) < person->width / 3;
                bool legs = y >= person->y + person->height * 2 / 3 &&
                    abs(dx) < person->width / 4 && abs(dx) > person->width / 16;
                if (head || torso || legs) {
                    s_background[y * LCD_FG_WIDTH + x] = head ? 0xFDD0U :
                        torso ? (i == 0U ? 0x39DFU : 0xEC63U) : 0x18C3U;
                }
            }
        }
    }
}

void pc_port_camera_sample(uint32_t frame_id, uint32_t captured_ms,
                            bool available, const pc_camera_person_t *people,
                            uint8_t count)
{
    if (count > 2U || (count && !people)) { available = false; count = 0U; }
    bool changed = count != s_camera_people_count ||
        (count && memcmp(s_camera_people, people, count * sizeof(*people)) != 0);
    if (count) { memcpy(s_camera_people, people, count * sizeof(*people)); }
    s_camera_people_count = count;
    s_camera_frame_id = frame_id;
    s_camera_captured_ms = captured_ms;
    s_camera_available = available;
    if (changed) { draw_camera_background(); }
}

uint8_t app_lcd_copy_camera_thumbnail(uint16_t *dst, uint16_t width,
                                     uint16_t height,
                                     app_camera_thumbnail_info_t *info)
{
    if (info) { memset(info, 0, sizeof(*info)); }
    if (!dst || !info || !width || width != height || width > APP_CAMERA_THUMBNAIL_MAX_SIDE ||
        !s_camera_available ||
        (uint32_t)(app_ui_tick_ms() - s_camera_captured_ms) > APP_CAMERA_THUMBNAIL_MAX_AGE_MS) { return 0U; }
    for (uint32_t y = 0U; y < height; y++) {
        uint32_t sy = LCD_CAMERA_VIEW_Y + ((y * 2U + 1U) * LCD_CAMERA_VIEW_HEIGHT) / (height * 2U);
        for (uint32_t x = 0U; x < width; x++) {
            uint32_t sx = LCD_CAMERA_VIEW_X + ((x * 2U + 1U) * LCD_CAMERA_VIEW_WIDTH) / (width * 2U);
            dst[y * width + x] = s_background[sy * LCD_FG_WIDTH + sx];
        }
    }
    info->frame_id = s_camera_frame_id;
    info->captured_ms = s_camera_captured_ms;
    info->source_x = LCD_CAMERA_VIEW_X;
    info->source_y = LCD_CAMERA_VIEW_Y;
    info->source_width = LCD_CAMERA_VIEW_WIDTH;
    info->source_height = LCD_CAMERA_VIEW_HEIGHT;
    info->simulated = 1U;
    return 1U;
}

static uint8_t blend(uint8_t foreground, uint8_t background, uint8_t alpha)
{
    return (uint8_t)(((uint32_t)foreground * alpha +
                      (uint32_t)background * (255U - alpha) + 127U) / 255U);
}

bool pc_port_init(bool hidden)
{
#ifdef _WIN32
    SetUnhandledExceptionFilter(preview_exception);
#endif
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        return false;
    }
    s_window = SDL_CreateWindow("N647 LVGL PC Preview - 模拟数据 / RGB565 test background",
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                LCD_FG_WIDTH, LCD_FG_HEIGHT,
                                hidden ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN);
    if (!s_window) { return false; }
    s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_SOFTWARE);
    if (!s_renderer) { return false; }
    s_texture = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  LCD_FG_WIDTH, LCD_FG_HEIGHT);
    if (!s_texture) { return false; }
    if (SDL_SetTextureBlendMode(s_texture, SDL_BLENDMODE_NONE) != 0) {
        return false;
    }
    s_draw_index = 0U;
    s_fixed_tick = 0U;
    s_test_clock = false;
    s_blocked_fg_access = 0U;
    memset(s_foreground, 0, sizeof(s_foreground));
    /* A deterministic scene makes the debug page's transparency visible.
       It is a test image, not a camera or NPU simulation. */
    s_camera_available = false;
    s_camera_frame_id = s_camera_captured_ms = 0U;
    s_camera_people_count = 0U;
    memset(s_camera_people, 0, sizeof(s_camera_people));
    draw_camera_background();
    app_lcd_present_fg_buffer(app_lcd_get_fg_draw_buffer());
    return true;
}

void pc_port_shutdown(void)
{
    SDL_DestroyTexture(s_texture);
    SDL_DestroyRenderer(s_renderer);
    SDL_DestroyWindow(s_window);
    s_texture = NULL;
    s_renderer = NULL;
    s_window = NULL;
    SDL_Quit();
}

uint32_t app_ui_tick_ms(void)
{
    /* Model time is advanced only by the scheduler, never by SDL implicitly. */
    return s_fixed_tick;
}

uint32_t pc_port_wall_tick_ms(void)
{
    return SDL_GetTicks();
}

uint32_t pc_port_render_tick_ms(void)
{
    /* Interactive animation/render pacing stays real-time at 60x model time.
       Self-tests use deterministic ticks without waiting for wall time. */
    return s_test_clock ? s_fixed_tick : SDL_GetTicks();
}

void pc_port_advance_time(uint32_t milliseconds)
{
    s_fixed_tick += milliseconds;
}

void pc_port_use_test_clock(void)
{
    s_test_clock = true;
}

void pc_port_sleep_ms(uint32_t milliseconds)
{
    SDL_Delay(milliseconds);
}

uint8_t *app_lcd_get_fg_draw_buffer(void)
{
    if (s_blocked_fg_access != 0U) {
        s_blocked_fg_access--;
        return NULL;
    }
    return (uint8_t *)s_foreground[s_draw_index];
}

void pc_port_block_fg_access(uint32_t attempts)
{
    /* Explicit self-test injection for an unacknowledged display reload. */
    s_blocked_fg_access = attempts;
}

void app_lcd_mark_fg_dirty(int32_t x, int32_t y, uint32_t width, uint32_t height)
{
    /* Desktop composition already preserves both complete buffers. Keep the
       shared contract bounded even though it needs no cache maintenance. */
    if (x < 0 || y < 0 || !width || !height ||
        (uint32_t)x >= LCD_FG_WIDTH || (uint32_t)y >= LCD_FG_HEIGHT ||
        width > LCD_FG_WIDTH - (uint32_t)x ||
        height > LCD_FG_HEIGHT - (uint32_t)y) { return; }
}

void app_lcd_present_fg_buffer(uint8_t *buffer)
{
    if (!buffer || !s_texture) { return; }
    const uint16_t *foreground = (const uint16_t *)buffer;
    for (uint32_t i = 0U; i < PIXEL_COUNT; i++) {
        uint16_t fg = foreground[i];
        uint16_t bg = s_background[i];
        uint8_t alpha = (uint8_t)((fg >> 12U) * 17U);
        uint8_t red = blend((uint8_t)(((fg >> 8U) & 15U) * 17U),
                            (uint8_t)(((bg >> 11U) & 31U) * 255U / 31U), alpha);
        uint8_t green = blend((uint8_t)(((fg >> 4U) & 15U) * 17U),
                              (uint8_t)(((bg >> 5U) & 63U) * 255U / 63U), alpha);
        uint8_t blue = blend((uint8_t)((fg & 15U) * 17U),
                             (uint8_t)((bg & 31U) * 255U / 31U), alpha);
        s_composite[i] = 0xFF000000U | ((uint32_t)red << 16U) |
                          ((uint32_t)green << 8U) | blue;
    }
    if (SDL_UpdateTexture(s_texture, NULL, s_composite, LCD_FG_WIDTH * 4) == 0 &&
        SDL_RenderClear(s_renderer) == 0 &&
        SDL_RenderCopy(s_renderer, s_texture, NULL, NULL) == 0) {
        SDL_RenderPresent(s_renderer);
    }
    /* Preserve untouched pixels across LVGL partial flushes, like the board. */
    unsigned int next = s_draw_index ^ 1U;
    memcpy(s_foreground[next], buffer, sizeof(s_foreground[next]));
    s_draw_index = next;
}

bool pc_port_poll_event(pc_event_t *event)
{
    SDL_Event input;
    while (SDL_PollEvent(&input)) {
        memset(event, 0, sizeof(*event));
        if (input.type == SDL_QUIT ||
            (input.type == SDL_KEYDOWN && input.key.keysym.sym == SDLK_ESCAPE)) {
            event->type = PC_EVENT_QUIT;
            return true;
        }
        if (input.type == SDL_MOUSEBUTTONDOWN || input.type == SDL_MOUSEBUTTONUP) {
            if (input.button.button != SDL_BUTTON_LEFT) { continue; }
            event->type = PC_EVENT_POINTER;
            event->x = input.button.x;
            event->y = input.button.y;
            event->pressed = input.type == SDL_MOUSEBUTTONDOWN;
            return true;
        }
        if (input.type == SDL_MOUSEMOTION) {
            event->type = PC_EVENT_POINTER;
            event->x = input.motion.x;
            event->y = input.motion.y;
            event->pressed = (input.motion.state & SDL_BUTTON_LMASK) != 0U;
            return true;
        }
        if (input.type == SDL_KEYDOWN && input.key.repeat == 0) {
            event->type = PC_EVENT_KEY;
            event->shift = (input.key.keysym.mod & KMOD_SHIFT) != 0;
            event->key = input.key.keysym.sym == SDLK_UP ? PC_KEY_UP :
                         input.key.keysym.sym == SDLK_DOWN ? PC_KEY_DOWN :
                         (int32_t)input.key.keysym.sym;
            return true;
        }
    }
    return false;
}

bool pc_port_push_pointer(int32_t x, int32_t y, bool pressed)
{
    SDL_Event input;
    memset(&input, 0, sizeof(input));
    input.type = pressed ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    input.button.windowID = SDL_GetWindowID(s_window);
    input.button.button = SDL_BUTTON_LEFT;
    input.button.state = pressed ? SDL_PRESSED : SDL_RELEASED;
    input.button.x = x;
    input.button.y = y;
    return SDL_PushEvent(&input) == 1;
}

bool pc_port_push_key(int32_t key, bool shift)
{
    SDL_Event input;
    memset(&input, 0, sizeof(input));
    input.type = SDL_KEYDOWN;
    input.key.windowID = SDL_GetWindowID(s_window);
    input.key.state = SDL_PRESSED;
    input.key.keysym.sym = key == PC_KEY_UP ? SDLK_UP :
                           key == PC_KEY_DOWN ? SDLK_DOWN : (SDL_Keycode)key;
    input.key.keysym.mod = shift ? KMOD_LSHIFT : KMOD_NONE;
    if (SDL_PushEvent(&input) != 1) { return false; }
    input.type = SDL_KEYUP;
    input.key.state = SDL_RELEASED;
    return SDL_PushEvent(&input) == 1;
}

bool pc_port_push_quit(void)
{
    SDL_Event input;
    memset(&input, 0, sizeof(input));
    input.type = SDL_QUIT;
    return SDL_PushEvent(&input) == 1;
}

bool pc_port_save_bmp(const char *path)
{
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
        s_composite, LCD_FG_WIDTH, LCD_FG_HEIGHT, 32, LCD_FG_WIDTH * 4,
        SDL_PIXELFORMAT_ARGB8888);
    if (!surface) { return false; }
    int result = SDL_SaveBMP(surface, path);
    SDL_FreeSurface(surface);
    return result == 0;
}

const char *pc_port_last_error(void)
{
    return SDL_GetError();
}
