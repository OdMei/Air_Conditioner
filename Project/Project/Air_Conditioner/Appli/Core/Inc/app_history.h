#ifndef APP_HISTORY_H
#define APP_HISTORY_H

#include <stdint.h>

#ifndef APP_HISTORY_SAMPLE_CAPACITY
#define APP_HISTORY_SAMPLE_CAPACITY (60U)
#endif
#ifndef APP_HISTORY_EVENT_CAPACITY
#define APP_HISTORY_EVENT_CAPACITY (12U)
#endif
#ifndef APP_HISTORY_SAMPLE_INTERVAL_MS
#define APP_HISTORY_SAMPLE_INTERVAL_MS (5000UL)
#endif
#ifndef APP_HISTORY_MAX_KNOWN_GAP_MS
#define APP_HISTORY_MAX_KNOWN_GAP_MS (5000UL)
#endif

#define APP_HISTORY_SOURCE_BOARD (0U)
#define APP_HISTORY_SOURCE_SIMULATED (1U)
#define APP_HISTORY_VALID_TEMPERATURE (0x01U)
#define APP_HISTORY_VALID_HUMIDITY (0x02U)
#define APP_HISTORY_VALID_ALS (0x04U)
#define APP_HISTORY_VALID_PEOPLE (0x08U)
#define APP_HISTORY_VALID_TARGET (0x10U)
#define APP_HISTORY_VALID_REQUEST (0x20U)
#define APP_HISTORY_VALID_REASON (0x40U)
#define APP_HISTORY_VALID_PRESENCE (0x80U)
#define APP_HISTORY_COOLING_REQUEST (0x01U)

#define APP_HISTORY_EVENT_REASON (0x01U)
#define APP_HISTORY_EVENT_REQUEST (0x02U)
#define APP_HISTORY_EVENT_PRESENCE (0x04U)
#define APP_HISTORY_EVENT_QUALITY (0x08U)
#define APP_HISTORY_EVENT_SESSION (0x10U)

/* Values originate in an existing control snapshot. No control policy runs here.
 * quality is an opaque caller-defined quality/status bit field. Missing values
 * are explicit in valid_flags; stored payloads of missing values are zeroed. */
typedef struct
{
    int16_t temperature_x10;
    int16_t target_x10;
    uint16_t humidity_x10;
    uint16_t als;
    uint16_t people;
    uint16_t quality;
    uint8_t request_mask;
    uint8_t reason;
    uint8_t presence;
    uint8_t source;
    uint8_t valid_flags;
} app_history_input_t;

typedef struct
{
    uint64_t elapsed_ms;
    app_history_input_t data;
} app_history_sample_t;

typedef struct
{
    uint64_t elapsed_ms;
    uint16_t quality;
    uint8_t request_mask;
    uint8_t reason;
    uint8_t presence;
    uint8_t source;
    uint8_t valid_flags;
    uint8_t changed_flags;
} app_history_event_t;

typedef struct
{
    uint64_t elapsed_ms;
    uint64_t request_known_ms;
    uint64_t request_unknown_ms;
    uint64_t cooling_request_ms;
    uint32_t cooling_request_starts;
    uint32_t revision;
    uint32_t session_id;
    uint16_t sample_count;
    uint16_t event_count;
    uint8_t source;
    uint8_t initialized;
} app_history_stats_t;

typedef struct
{
    app_history_sample_t samples[APP_HISTORY_SAMPLE_CAPACITY];
    app_history_event_t events[APP_HISTORY_EVENT_CAPACITY];
    app_history_input_t previous;
    app_history_stats_t stats;
    uint32_t last_tick_ms;
    uint32_t last_sample_tick_ms;
    uint16_t sample_head;
    uint16_t event_head;
} app_history_t;

void app_history_init(app_history_t *history);
/* Serialized caller only. Tick wrap is supported; a backward jump whose
 * unsigned delta exceeds INT32_MAX, or source change, opens a clean session.
 * A missed sampling interval creates one current point, never invented points.
 * A cooling request interval is known only when both endpoints are valid,
 * agree on cooling state, and are at most MAX_KNOWN_GAP_MS apart. */
void app_history_observe(app_history_t *history,
                         const app_history_input_t *input, uint32_t now_ms);
/* Oldest-first access; pointers remain valid only until the next observe/init. */
const app_history_sample_t *app_history_sample_at(const app_history_t *history,
                                                 uint16_t oldest_index);
const app_history_event_t *app_history_event_at(const app_history_t *history,
                                               uint16_t oldest_index);
void app_history_get_stats(const app_history_t *history,
                            app_history_stats_t *stats);

#endif
