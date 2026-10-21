#ifndef APP_ZONE_LOGIC_H
#define APP_ZONE_LOGIC_H

#include <stdint.h>

#define APP_ZONE_COUNT (2U)
#define APP_ZONE_MAX_BOXES (10U)
#define APP_ZONE_WIDTH (800U)
#define APP_ZONE_HEIGHT (480U)
#define APP_ZONE_SPLIT_X (400U)
#define APP_ZONE_LEFT_MASK (1U)
#define APP_ZONE_RIGHT_MASK (2U)
#define APP_ZONE_ALL_MASK (3U)
#define APP_ZONE_ENTER_MS (500UL)
#define APP_ZONE_LEAVE_MS (1000UL)
#define APP_ZONE_STALE_MS (3000UL)

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} app_zone_box_t;

typedef struct
{
    uint32_t frame_id;
    uint32_t captured_ms;
    uint8_t valid;
    uint8_t count;
    /* Display coordinates, after the camera crop has been applied. */
    app_zone_box_t boxes[APP_ZONE_MAX_BOXES];
} app_zone_frame_t;

typedef enum
{
    APP_ZONE_MISSING = 0,
    APP_ZONE_FRESH,
    APP_ZONE_STALE,
    APP_ZONE_INVALID
} app_zone_quality_t;

typedef enum
{
    APP_ZONE_ADVICE_UNKNOWN = 0,
    APP_ZONE_ADVICE_NONE,
    APP_ZONE_ADVICE_LEFT,
    APP_ZONE_ADVICE_RIGHT,
    APP_ZONE_ADVICE_BOTH
} app_zone_advice_t;

typedef struct
{
    uint8_t raw_count[APP_ZONE_COUNT];
    uint8_t known_mask;
    uint8_t occupied_mask;
    uint8_t confirming_mask;
    uint8_t frame_valid;
    app_zone_quality_t quality;
    app_zone_advice_t advice;
    uint32_t frame_id;
    uint32_t captured_ms;
    uint32_t age_ms;
} app_zone_snapshot_t;

typedef struct
{
    uint32_t frame_id;
    uint32_t captured_ms;
    uint32_t candidate_since_ms[APP_ZONE_COUNT];
    uint8_t have_frame;
    uint8_t raw_count[APP_ZONE_COUNT];
    uint8_t known_mask;
    uint8_t occupied_mask;
    uint8_t candidate_mask;
    uint8_t confirming_mask;
    app_zone_quality_t quality;
} app_zone_logic_t;

/* Call init again when the producer starts a new frame-id sequence. */
void app_zone_logic_init(app_zone_logic_t *state);
/* NULL only expires old data. Duplicate/older valid frames never advance a
 * confirmation or refresh captured_ms. An invalid report clears all estimates.
 * The caller owns synchronization; this module is a single-threaded reducer. */
void app_zone_logic_update(app_zone_logic_t *state,
                           const app_zone_frame_t *frame, uint32_t now_ms);
void app_zone_logic_get(const app_zone_logic_t *state, uint32_t now_ms,
                        app_zone_snapshot_t *snapshot);

#endif /* APP_ZONE_LOGIC_H */
