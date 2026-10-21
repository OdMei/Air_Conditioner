#ifndef APP_EVENT_JOURNAL_H
#define APP_EVENT_JOURNAL_H

#include <stdbool.h>
#include <stdint.h>

#define APP_EVENT_JOURNAL_CAPACITY (4U)
#define APP_EVENT_THUMB_WIDTH (128U)
#define APP_EVENT_THUMB_HEIGHT (128U)
#define APP_EVENT_THUMB_PIXELS (APP_EVENT_THUMB_WIDTH * APP_EVENT_THUMB_HEIGHT)
#define APP_EVENT_THUMB_BYTES (APP_EVENT_THUMB_PIXELS * sizeof(uint16_t))
#define APP_EVENT_CHANNEL_TEMPERATURE (0U)
#define APP_EVENT_CHANNEL_HUMIDITY (1U)
#define APP_EVENT_CHANNEL_NONE (0xFFU)

typedef enum {
    APP_EVENT_FIRST_PRESENT = 0,
    APP_EVENT_ABSENT,
    APP_EVENT_ENV_TEMP_RAISED,
    APP_EVENT_ENV_HUM_RAISED,
    APP_EVENT_ENV_RECOVERY,
    APP_EVENT_MANUAL_SNAPSHOT,
    APP_EVENT_TYPE_COUNT
} app_event_type_t;

typedef enum {
    APP_EVENT_SOURCE_BOARD = 0,
    APP_EVENT_SOURCE_SIMULATED,
    APP_EVENT_SOURCE_DEMO,
    APP_EVENT_SOURCE_COUNT
} app_event_source_t;

typedef enum {
    APP_EVENT_QUALITY_UNAVAILABLE = 0,
    APP_EVENT_QUALITY_VALID,
    APP_EVENT_QUALITY_STALE
} app_event_quality_t;

typedef enum {
    APP_EVENT_IMAGE_PENDING = 0,
    APP_EVENT_IMAGE_READY,
    APP_EVENT_IMAGE_CAMERA_UNAVAILABLE,
    APP_EVENT_IMAGE_OLD,
    APP_EVENT_IMAGE_SKIPPED,
    APP_EVENT_IMAGE_COPY_FAILED
} app_event_image_status_t;

typedef struct {
    uint32_t run_ms; /* Uptime, never a fabricated calendar timestamp. */
    int16_t people_count;
    int16_t temperature_x10;
    uint16_t humidity_x10;
    uint8_t people_valid;
    uint8_t temperature_quality;
    uint8_t humidity_quality;
    uint8_t type;
    uint8_t source;
    uint8_t environment_channel;
} app_event_meta_t;

typedef struct {
    app_event_meta_t meta;
    uint32_t id;
    uint32_t display_frame_id;
    uint32_t image_captured_ms;
    uint8_t image_status;
    uint8_t slot;
} app_event_record_t;

typedef struct {
    app_event_record_t records[APP_EVENT_JOURNAL_CAPACITY];
    uint32_t next_id;
    uint32_t revision;
    uint32_t capture_busy_id;
    uint32_t last_image_frame_id;
    uint32_t last_image_captured_ms;
    uint8_t count;
    uint8_t head;
    uint8_t source;
    uint8_t has_last_image;
} app_event_journal_t;

/* The application has one journal, owned by its existing LVGL thread. Its
 * fixed pixel pool is external RAM on ARM and ordinary static storage on PC.
 * Metadata operations never copy or clear that 128 KiB pool. A reserve/copy/
 * commit transaction must finish before changing source or resetting. */
void app_event_journal_init(app_event_journal_t *journal, uint8_t source);
void app_event_journal_reset(app_event_journal_t *journal, uint8_t source);
uint32_t app_event_journal_append(app_event_journal_t *journal,
                                  const app_event_meta_t *meta);
/* Reserves one pending record for synchronous capture. Append is rejected
 * while a copy is outstanding; commit or mark_no_image releases the lease. */
uint16_t *app_event_journal_thumb_pixels(app_event_journal_t *journal, uint32_t id);
bool app_event_journal_commit_thumb(app_event_journal_t *journal, uint32_t id,
                                    uint32_t display_frame_id, uint32_t captured_ms);
bool app_event_journal_mark_no_image(app_event_journal_t *journal, uint32_t id,
                                     app_event_image_status_t status);
/* Returned record pointers are transient: retain the ID, and look it up after
 * every journal mutation. Index 0 is newest; an evicted ID returns NULL. */
const app_event_record_t *app_event_journal_latest(const app_event_journal_t *journal,
                                                  uint32_t index);
const app_event_record_t *app_event_journal_lookup(const app_event_journal_t *journal,
                                                  uint32_t id);
const uint16_t *app_event_journal_image(const app_event_journal_t *journal, uint32_t id);

#endif
