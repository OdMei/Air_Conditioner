#include "app_event_journal.h"

#include <stddef.h>
#include <string.h>

#if defined(__arm__) || defined(__thumb__)
#define JOURNAL_PIXELS_ATTRIBUTES __attribute__((section(".EXTRAM"), aligned(32)))
#elif defined(__GNUC__)
#define JOURNAL_PIXELS_ATTRIBUTES __attribute__((aligned(32)))
#else
#define JOURNAL_PIXELS_ATTRIBUTES
#endif

static uint16_t s_pixels[APP_EVENT_JOURNAL_CAPACITY][APP_EVENT_THUMB_PIXELS]
    JOURNAL_PIXELS_ATTRIBUTES;
static app_event_journal_t *s_pixel_owner;

static bool owned(const app_event_journal_t *journal)
{ return journal && s_pixel_owner == journal; }

const app_event_record_t *app_event_journal_lookup(const app_event_journal_t *journal,
                                                  uint32_t id)
{
    if (!owned(journal) || id == 0U) { return NULL; }
    for (uint32_t i = 0U; i < APP_EVENT_JOURNAL_CAPACITY; ++i)
    {
        if (journal->records[i].id == id) { return &journal->records[i]; }
    }
    return NULL;
}

static app_event_record_t *lookup_mutable(app_event_journal_t *journal, uint32_t id)
{ return (app_event_record_t *)app_event_journal_lookup(journal, id); }

void app_event_journal_init(app_event_journal_t *journal, uint8_t source)
{
    if (!journal || source >= APP_EVENT_SOURCE_COUNT) { return; }
    memset(journal, 0, sizeof(*journal));
    journal->next_id = 1U;
    journal->revision = 1U;
    journal->source = source;
    s_pixel_owner = journal;
}

void app_event_journal_reset(app_event_journal_t *journal, uint8_t source)
{
    if (!owned(journal) || source >= APP_EVENT_SOURCE_COUNT) { return; }
    memset(journal->records, 0, sizeof(journal->records));
    journal->count = 0U;
    journal->head = 0U;
    journal->source = source;
    journal->capture_busy_id = 0U;
    journal->has_last_image = 0U;
    ++journal->revision;
}

static uint32_t allocate_id(app_event_journal_t *journal)
{
    /* At most four live IDs: five bounded probes find a free nonzero ID even
     * across UINT32 wrap. Source resets keep IDs moving for stable selection. */
    for (uint32_t probe = 0U; probe <= APP_EVENT_JOURNAL_CAPACITY; ++probe)
    {
        uint32_t candidate = journal->next_id;
        if (candidate == 0U) { candidate = 1U; }
        journal->next_id = candidate + 1U;
        if (!app_event_journal_lookup(journal, candidate)) { return candidate; }
    }
    return 0U;
}

uint32_t app_event_journal_append(app_event_journal_t *journal,
                                  const app_event_meta_t *meta)
{
    if (!owned(journal) || !meta || meta->source >= APP_EVENT_SOURCE_COUNT ||
        meta->type >= APP_EVENT_TYPE_COUNT || journal->capture_busy_id != 0U ||
        meta->temperature_quality > APP_EVENT_QUALITY_STALE ||
        meta->humidity_quality > APP_EVENT_QUALITY_STALE || meta->people_valid > 1U ||
        (meta->people_valid && meta->people_count < 0) ||
        (meta->humidity_quality == APP_EVENT_QUALITY_VALID && meta->humidity_x10 > 1000U))
    { return 0U; }
    if (meta->source != journal->source)
    { app_event_journal_reset(journal, meta->source); }
    uint32_t id = allocate_id(journal);
    if (!id) { return 0U; }
    app_event_record_t *record = &journal->records[journal->head];
    memset(record, 0, sizeof(*record));
    record->meta = *meta;
    if (!record->meta.people_valid) { record->meta.people_count = 0; }
    if (record->meta.temperature_quality == APP_EVENT_QUALITY_UNAVAILABLE)
    { record->meta.temperature_x10 = 0; }
    if (record->meta.humidity_quality == APP_EVENT_QUALITY_UNAVAILABLE)
    { record->meta.humidity_x10 = 0U; }
    record->id = id;
    record->slot = journal->head;
    record->image_status = APP_EVENT_IMAGE_PENDING;
    journal->head = (uint8_t)((journal->head + 1U) % APP_EVENT_JOURNAL_CAPACITY);
    if (journal->count < APP_EVENT_JOURNAL_CAPACITY) { ++journal->count; }
    ++journal->revision;
    return id;
}

uint16_t *app_event_journal_thumb_pixels(app_event_journal_t *journal, uint32_t id)
{
    app_event_record_t *record = lookup_mutable(journal, id);
    if (!record || record->image_status != APP_EVENT_IMAGE_PENDING ||
        journal->capture_busy_id != 0U) { return NULL; }
    journal->capture_busy_id = id;
    return s_pixels[record->slot];
}

bool app_event_journal_commit_thumb(app_event_journal_t *journal, uint32_t id,
                                    uint32_t display_frame_id, uint32_t captured_ms)
{
    app_event_record_t *record = lookup_mutable(journal, id);
    if (!record || record->image_status != APP_EVENT_IMAGE_PENDING ||
        journal->capture_busy_id != id) { return false; }
    journal->capture_busy_id = 0U;
    bool duplicate = journal->has_last_image &&
        journal->last_image_frame_id == display_frame_id &&
        journal->last_image_captured_ms == captured_ms;
    for (uint32_t slot = 0U; slot < APP_EVENT_JOURNAL_CAPACITY && !duplicate; ++slot)
    {
        const app_event_record_t *previous = &journal->records[slot];
        duplicate = previous->id != id && previous->id != 0U &&
            previous->image_status == APP_EVENT_IMAGE_READY &&
            previous->display_frame_id == display_frame_id &&
            previous->image_captured_ms == captured_ms;
    }
    if (duplicate)
    {
        record->image_status = APP_EVENT_IMAGE_SKIPPED;
        ++journal->revision;
        return false;
    }
    record->display_frame_id = display_frame_id;
    record->image_captured_ms = captured_ms;
    record->image_status = APP_EVENT_IMAGE_READY;
    journal->last_image_frame_id = display_frame_id;
    journal->last_image_captured_ms = captured_ms;
    journal->has_last_image = 1U;
    ++journal->revision;
    return true;
}

bool app_event_journal_mark_no_image(app_event_journal_t *journal, uint32_t id,
                                     app_event_image_status_t status)
{
    app_event_record_t *record = lookup_mutable(journal, id);
    if (!record || record->image_status != APP_EVENT_IMAGE_PENDING ||
        status <= APP_EVENT_IMAGE_READY || status > APP_EVENT_IMAGE_COPY_FAILED)
    { return false; }
    record->image_status = (uint8_t)status;
    if (journal->capture_busy_id == id) { journal->capture_busy_id = 0U; }
    ++journal->revision;
    return true;
}

const app_event_record_t *app_event_journal_latest(const app_event_journal_t *journal,
                                                  uint32_t index)
{
    if (!owned(journal) || index >= journal->count) { return NULL; }
    uint32_t slot = (journal->head + APP_EVENT_JOURNAL_CAPACITY - 1U - index) %
        APP_EVENT_JOURNAL_CAPACITY;
    return &journal->records[slot];
}

const uint16_t *app_event_journal_image(const app_event_journal_t *journal, uint32_t id)
{
    const app_event_record_t *record = app_event_journal_lookup(journal, id);
    return record && record->image_status == APP_EVENT_IMAGE_READY ?
        s_pixels[record->slot] : NULL;
}
