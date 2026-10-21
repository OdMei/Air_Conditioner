#include "app_event_journal.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t s_checks;
#define REQUIRE(condition) do { ++s_checks; if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); return 0; } } while (0)

static app_event_meta_t metadata(uint32_t time)
{
    app_event_meta_t meta = {0};
    meta.run_ms = time;
    meta.people_count = 1;
    meta.people_valid = 1U;
    meta.temperature_x10 = 271;
    meta.humidity_x10 = 555U;
    meta.temperature_quality = APP_EVENT_QUALITY_VALID;
    meta.humidity_quality = APP_EVENT_QUALITY_VALID;
    meta.source = APP_EVENT_SOURCE_BOARD;
    meta.type = APP_EVENT_FIRST_PRESENT;
    meta.environment_channel = APP_EVENT_CHANNEL_NONE;
    return meta;
}

static int captures_and_lease(void)
{
    app_event_journal_t journal;
    app_event_journal_init(&journal, APP_EVENT_SOURCE_BOARD);
    REQUIRE(journal.count == 0U);
    REQUIRE(app_event_journal_latest(&journal, 0U) == NULL);
    app_event_meta_t meta = metadata(5000U);
    uint32_t first = app_event_journal_append(&journal, &meta);
    REQUIRE(first != 0U);
    REQUIRE(app_event_journal_latest(&journal, 0U)->meta.temperature_x10 == 271);
    REQUIRE(app_event_journal_latest(&journal, 0U)->meta.humidity_x10 == 555U);
    REQUIRE(app_event_journal_image(&journal, first) == NULL);
    REQUIRE(!app_event_journal_commit_thumb(&journal, first, 100U, 4990U));
    uint16_t *pixels = app_event_journal_thumb_pixels(&journal, first);
    REQUIRE(pixels != NULL);
    REQUIRE(((uintptr_t)pixels & 31U) == 0U);
    REQUIRE(app_event_journal_thumb_pixels(&journal, first) == NULL);
    uint32_t revision = journal.revision;
    REQUIRE(app_event_journal_append(&journal, &meta) == 0U);
    REQUIRE(journal.revision == revision && journal.count == 1U);
    pixels[0] = 0xF800U;
    pixels[APP_EVENT_THUMB_PIXELS - 1U] = 0x07E0U;
    REQUIRE(app_event_journal_commit_thumb(&journal, first, 100U, 4990U));
    REQUIRE(journal.capture_busy_id == 0U);
    REQUIRE(app_event_journal_image(&journal, first)[0] == 0xF800U);
    REQUIRE(app_event_journal_image(&journal, first)[APP_EVENT_THUMB_PIXELS - 1U] == 0x07E0U);
    REQUIRE(app_event_journal_lookup(&journal, first)->display_frame_id == 100U);
    REQUIRE(app_event_journal_lookup(&journal, first)->image_captured_ms == 4990U);
    REQUIRE(app_event_journal_thumb_pixels(&journal, first) == NULL);
    REQUIRE(!app_event_journal_mark_no_image(&journal, first, APP_EVENT_IMAGE_OLD));
    uint32_t second = app_event_journal_append(&journal, &meta);
    REQUIRE(second != 0U && second != first);
    REQUIRE(app_event_journal_thumb_pixels(&journal, second) != NULL);
    REQUIRE(!app_event_journal_commit_thumb(&journal, second, 100U, 4990U));
    REQUIRE(app_event_journal_lookup(&journal, second)->image_status == APP_EVENT_IMAGE_SKIPPED);
    REQUIRE(app_event_journal_image(&journal, second) == NULL);
    REQUIRE(journal.capture_busy_id == 0U);
    uint32_t third = app_event_journal_append(&journal, &meta);
    REQUIRE(app_event_journal_thumb_pixels(&journal, third) != NULL);
    REQUIRE(app_event_journal_commit_thumb(&journal, third, 100U, 5030U));
    uint32_t fourth = app_event_journal_append(&journal, &meta);
    REQUIRE(app_event_journal_thumb_pixels(&journal, fourth) != NULL);
    REQUIRE(!app_event_journal_commit_thumb(&journal, fourth, 100U, 4990U));
    REQUIRE(app_event_journal_lookup(&journal, fourth)->image_status == APP_EVENT_IMAGE_SKIPPED);
    return 1;
}

static int eviction_and_stable_selection(void)
{
    app_event_journal_t journal;
    app_event_journal_init(&journal, APP_EVENT_SOURCE_BOARD);
    uint32_t ids[12];
    for (uint32_t i = 0U; i < 12U; ++i)
    {
        app_event_meta_t meta = metadata(i * 2000U);
        ids[i] = app_event_journal_append(&journal, &meta);
        REQUIRE(ids[i] != 0U);
        REQUIRE(app_event_journal_mark_no_image(&journal, ids[i], APP_EVENT_IMAGE_CAMERA_UNAVAILABLE));
        REQUIRE(journal.count == (i < 4U ? i + 1U : 4U));
        REQUIRE(app_event_journal_latest(&journal, 0U)->id == ids[i]);
        for (uint32_t index = 0U; index < journal.count; ++index)
        {
            const app_event_record_t *record = app_event_journal_latest(&journal, index);
            REQUIRE(record && record->id == ids[i - index]);
            REQUIRE(record->meta.run_ms == (i - index) * 2000U);
            REQUIRE(app_event_journal_lookup(&journal, record->id) == record);
        }
        if (i >= 4U) { REQUIRE(app_event_journal_lookup(&journal, ids[i - 4U]) == NULL); }
    }
    uint32_t selected = ids[10];
    REQUIRE(app_event_journal_lookup(&journal, selected)->meta.run_ms == 20000U);
    for (uint32_t i = 0U; i < 4U; ++i)
    {
        app_event_meta_t meta = metadata(30000U + i);
        uint32_t id = app_event_journal_append(&journal, &meta);
        REQUIRE(id != selected);
        REQUIRE(app_event_journal_mark_no_image(&journal, id, APP_EVENT_IMAGE_OLD));
        if (i < 2U) { REQUIRE(app_event_journal_lookup(&journal, selected) != NULL); }
        else { REQUIRE(app_event_journal_lookup(&journal, selected) == NULL); }
    }
    REQUIRE(app_event_journal_latest(&journal, 4U) == NULL);
    REQUIRE(app_event_journal_latest(&journal, UINT32_MAX) == NULL);
    return 1;
}

static int source_reset_and_invalid_requests(void)
{
    app_event_journal_t journal;
    app_event_journal_init(&journal, APP_EVENT_SOURCE_BOARD);
    app_event_meta_t meta = metadata(120U);
    uint32_t first = app_event_journal_append(&journal, &meta);
    REQUIRE(first != 0U);
    REQUIRE(app_event_journal_thumb_pixels(&journal, first) != NULL);
    REQUIRE(!app_event_journal_mark_no_image(&journal, first, APP_EVENT_IMAGE_READY));
    REQUIRE(!app_event_journal_mark_no_image(&journal, first, APP_EVENT_IMAGE_PENDING));
    REQUIRE(app_event_journal_mark_no_image(&journal, first, APP_EVENT_IMAGE_COPY_FAILED));
    REQUIRE(journal.capture_busy_id == 0U);
    meta.source = APP_EVENT_SOURCE_SIMULATED;
    meta.people_valid = 0U;
    meta.temperature_quality = APP_EVENT_QUALITY_UNAVAILABLE;
    meta.humidity_quality = APP_EVENT_QUALITY_STALE;
    uint32_t simulated = app_event_journal_append(&journal, &meta);
    REQUIRE(simulated != first);
    REQUIRE(journal.count == 1U && journal.source == APP_EVENT_SOURCE_SIMULATED);
    REQUIRE(app_event_journal_lookup(&journal, first) == NULL);
    REQUIRE(app_event_journal_lookup(&journal, simulated)->meta.people_count == 0);
    REQUIRE(app_event_journal_lookup(&journal, simulated)->meta.temperature_x10 == 0);
    REQUIRE(app_event_journal_lookup(&journal, simulated)->meta.humidity_x10 == 555U);
    uint32_t revision = journal.revision;
    app_event_journal_reset(&journal, APP_EVENT_SOURCE_COUNT);
    REQUIRE(journal.count == 1U && journal.revision == revision);
    meta.source = APP_EVENT_SOURCE_COUNT;
    REQUIRE(app_event_journal_append(&journal, &meta) == 0U);
    meta.source = APP_EVENT_SOURCE_SIMULATED;
    meta.type = APP_EVENT_TYPE_COUNT;
    REQUIRE(app_event_journal_append(&journal, &meta) == 0U);
    meta.type = APP_EVENT_MANUAL_SNAPSHOT;
    meta.temperature_quality = 255U;
    REQUIRE(app_event_journal_append(&journal, &meta) == 0U);
    meta.temperature_quality = APP_EVENT_QUALITY_VALID;
    meta.people_valid = 2U;
    REQUIRE(app_event_journal_append(&journal, &meta) == 0U);
    meta.people_valid = 1U;
    meta.people_count = -1;
    REQUIRE(app_event_journal_append(&journal, &meta) == 0U);
    meta.people_count = 1;
    meta.humidity_quality = APP_EVENT_QUALITY_VALID;
    meta.humidity_x10 = 1001U;
    REQUIRE(app_event_journal_append(&journal, &meta) == 0U);
    REQUIRE(journal.count == 1U && journal.revision == revision);
    REQUIRE(app_event_journal_latest(NULL, 0U) == NULL);
    REQUIRE(app_event_journal_lookup(&journal, 0U) == NULL);
    REQUIRE(app_event_journal_thumb_pixels(&journal, first) == NULL);
    REQUIRE(!app_event_journal_commit_thumb(&journal, first, 1U, 1U));
    REQUIRE(!app_event_journal_mark_no_image(&journal, first, APP_EVENT_IMAGE_OLD));
    app_event_journal_reset(&journal, APP_EVENT_SOURCE_DEMO);
    REQUIRE(journal.count == 0U && journal.source == APP_EVENT_SOURCE_DEMO);
    REQUIRE(app_event_journal_lookup(&journal, simulated) == NULL);
    meta = metadata(5000U);
    meta.source = APP_EVENT_SOURCE_DEMO;
    uint32_t demo = app_event_journal_append(&journal, &meta);
    REQUIRE(demo > simulated);
    REQUIRE(app_event_journal_thumb_pixels(&journal, demo) != NULL);
    app_event_journal_reset(&journal, APP_EVENT_SOURCE_BOARD);
    REQUIRE(!app_event_journal_commit_thumb(&journal, demo, 55U, 5000U));
    REQUIRE(app_event_journal_image(&journal, demo) == NULL);
    return 1;
}

static int id_wrap_and_pixels_survive_metadata_reset(void)
{
    app_event_journal_t journal;
    app_event_journal_init(&journal, APP_EVENT_SOURCE_BOARD);
    app_event_meta_t meta = metadata(1U);
    uint32_t one = app_event_journal_append(&journal, &meta);
    REQUIRE(one == 1U);
    uint16_t *pixels = app_event_journal_thumb_pixels(&journal, one);
    REQUIRE(pixels != NULL);
    pixels[10] = 0x1234U;
    REQUIRE(app_event_journal_commit_thumb(&journal, one, UINT32_MAX, UINT32_MAX - 10U));
    journal.next_id = UINT32_MAX;
    uint32_t maximum = app_event_journal_append(&journal, &meta);
    REQUIRE(maximum == UINT32_MAX);
    uint32_t after = app_event_journal_append(&journal, &meta);
    REQUIRE(after == 2U);
    REQUIRE(app_event_journal_lookup(&journal, one) != NULL);
    REQUIRE(app_event_journal_lookup(&journal, maximum) != NULL);
    REQUIRE(app_event_journal_image(&journal, one)[10] == 0x1234U);
    app_event_journal_reset(&journal, APP_EVENT_SOURCE_BOARD);
    REQUIRE(app_event_journal_lookup(&journal, one) == NULL);
    meta.type = APP_EVENT_ENV_RECOVERY;
    meta.environment_channel = APP_EVENT_CHANNEL_HUMIDITY;
    uint32_t new_id = app_event_journal_append(&journal, &meta);
    REQUIRE(new_id == 3U);
    REQUIRE(app_event_journal_lookup(&journal, new_id)->meta.environment_channel ==
        APP_EVENT_CHANNEL_HUMIDITY);
    REQUIRE(app_event_journal_image(&journal, new_id) == NULL);
    REQUIRE(app_event_journal_thumb_pixels(&journal, new_id)[10] == 0x1234U);
    REQUIRE(app_event_journal_mark_no_image(&journal, new_id, APP_EVENT_IMAGE_SKIPPED));
    app_event_journal_t other;
    app_event_journal_init(&other, APP_EVENT_SOURCE_SIMULATED);
    REQUIRE(app_event_journal_lookup(&journal, new_id) == NULL);
    REQUIRE(app_event_journal_append(&journal, &meta) == 0U);
    return 1;
}

static int stress_reference_ring(void)
{
    app_event_journal_t journal;
    app_event_journal_init(&journal, APP_EVENT_SOURCE_BOARD);
    uint32_t reference[APP_EVENT_JOURNAL_CAPACITY] = {0};
    for (uint32_t i = 0U; i < 100000U; ++i)
    {
        app_event_meta_t meta = metadata(i);
        uint32_t evicted = reference[APP_EVENT_JOURNAL_CAPACITY - 1U];
        for (uint32_t index = APP_EVENT_JOURNAL_CAPACITY - 1U; index > 0U; --index)
        { reference[index] = reference[index - 1U]; }
        reference[0] = app_event_journal_append(&journal, &meta);
        REQUIRE(reference[0] != 0U);
        REQUIRE(app_event_journal_mark_no_image(&journal, reference[0], APP_EVENT_IMAGE_OLD));
        for (uint32_t index = 0U; index < journal.count; ++index)
        { REQUIRE(app_event_journal_latest(&journal, index)->id == reference[index]); }
        if (evicted) { REQUIRE(app_event_journal_lookup(&journal, evicted) == NULL); }
    }
    REQUIRE(sizeof(journal) < 512U);
    return 1;
}

int main(void)
{
    if (!captures_and_lease() || !eviction_and_stable_selection() ||
        !source_reset_and_invalid_requests() || !id_wrap_and_pixels_survive_metadata_reset() ||
        !stress_reference_ring()) { return EXIT_FAILURE; }
    printf("PASS event journal: %" PRIu32 " checks; 4 fixed 128x128 RGB565 slots; metadata %zu bytes\n",
        s_checks, sizeof(app_event_journal_t));
    return EXIT_SUCCESS;
}
