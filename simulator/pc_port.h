#ifndef PC_PORT_H
#define PC_PORT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { PC_EVENT_QUIT, PC_EVENT_POINTER, PC_EVENT_KEY } pc_event_type_t;
typedef enum { PC_KEY_ESCAPE = 27, PC_KEY_UP = 0x100, PC_KEY_DOWN } pc_key_t;
typedef struct {
    pc_event_type_t type;
    int32_t x;
    int32_t y;
    int32_t key;
    bool pressed;
    bool shift;
} pc_event_t;

typedef struct { uint16_t x, y, width, height; } pc_camera_person_t;
/* Deterministic synthetic background samples. They are never board images. */
void pc_port_camera_sample(uint32_t frame_id, uint32_t captured_ms,
                            bool available, const pc_camera_person_t *people,
                            uint8_t count);

bool pc_port_init(bool hidden);
void pc_port_shutdown(void);
bool pc_port_poll_event(pc_event_t *event);
bool pc_port_push_pointer(int32_t x, int32_t y, bool pressed);
bool pc_port_push_key(int32_t key, bool shift);
bool pc_port_push_quit(void);
void pc_port_sleep_ms(uint32_t milliseconds);
uint32_t pc_port_wall_tick_ms(void);
uint32_t pc_port_render_tick_ms(void);
void pc_port_advance_time(uint32_t milliseconds);
void pc_port_use_test_clock(void);
void pc_port_block_fg_access(uint32_t attempts);
bool pc_port_save_bmp(const char *path);
const char *pc_port_last_error(void);

#endif
