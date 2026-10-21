#include "app_history.h"

#include <stddef.h>
#include <string.h>

#if APP_HISTORY_SAMPLE_CAPACITY == 0 || APP_HISTORY_SAMPLE_CAPACITY > 65535U
#error APP_HISTORY_SAMPLE_CAPACITY must fit a nonzero uint16_t count
#endif
#if APP_HISTORY_EVENT_CAPACITY == 0 || APP_HISTORY_EVENT_CAPACITY > 65535U
#error APP_HISTORY_EVENT_CAPACITY must fit a nonzero uint16_t count
#endif
#if APP_HISTORY_SAMPLE_INTERVAL_MS == 0 || APP_HISTORY_SAMPLE_INTERVAL_MS > 0x7FFFFFFFUL
#error APP_HISTORY_SAMPLE_INTERVAL_MS must be in 1..INT32_MAX
#endif
#if APP_HISTORY_MAX_KNOWN_GAP_MS == 0 || APP_HISTORY_MAX_KNOWN_GAP_MS > 0x7FFFFFFFUL
#error APP_HISTORY_MAX_KNOWN_GAP_MS must be in 1..INT32_MAX
#endif

static app_history_input_t normalize_input(const app_history_input_t *input)
{
    app_history_input_t data = *input;
    if (!(data.valid_flags & APP_HISTORY_VALID_TEMPERATURE))
        data.temperature_x10 = 0;
    if (!(data.valid_flags & APP_HISTORY_VALID_HUMIDITY))
        data.humidity_x10 = 0;
    if (!(data.valid_flags & APP_HISTORY_VALID_ALS)) data.als = 0;
    if (!(data.valid_flags & APP_HISTORY_VALID_PEOPLE)) data.people = 0;
    if (!(data.valid_flags & APP_HISTORY_VALID_TARGET)) data.target_x10 = 0;
    if (!(data.valid_flags & APP_HISTORY_VALID_REQUEST)) data.request_mask = 0;
    if (!(data.valid_flags & APP_HISTORY_VALID_REASON)) data.reason = 0;
    if (!(data.valid_flags & APP_HISTORY_VALID_PRESENCE)) data.presence = 0;
    return data;
}

static void append_sample(app_history_t *history,
                           const app_history_input_t *data, uint32_t now_ms)
{
    app_history_sample_t *sample = &history->samples[history->sample_head];
    sample->elapsed_ms = history->stats.elapsed_ms;
    sample->data = *data;
    history->sample_head = (uint16_t)((history->sample_head + 1U) %
                                      APP_HISTORY_SAMPLE_CAPACITY);
    if (history->stats.sample_count < APP_HISTORY_SAMPLE_CAPACITY)
        ++history->stats.sample_count;
    history->last_sample_tick_ms = now_ms;
    ++history->stats.revision;
}

static void append_event(app_history_t *history,
                          const app_history_input_t *data, uint8_t changed)
{
    app_history_event_t *event = &history->events[history->event_head];
    event->elapsed_ms = history->stats.elapsed_ms;
    event->quality = data->quality;
    event->request_mask = data->request_mask;
    event->reason = data->reason;
    event->presence = data->presence;
    event->source = data->source;
    event->valid_flags = data->valid_flags;
    event->changed_flags = changed;
    history->event_head = (uint16_t)((history->event_head + 1U) %
                                    APP_HISTORY_EVENT_CAPACITY);
    if (history->stats.event_count < APP_HISTORY_EVENT_CAPACITY)
        ++history->stats.event_count;
    ++history->stats.revision;
}

static uint8_t event_changes(const app_history_input_t *previous,
                              const app_history_input_t *current)
{
    uint8_t changed = 0;
    uint8_t validity_changed = previous->valid_flags ^ current->valid_flags;
    if ((validity_changed & APP_HISTORY_VALID_REASON) ||
        previous->reason != current->reason)
        changed |= APP_HISTORY_EVENT_REASON;
    if ((validity_changed & APP_HISTORY_VALID_REQUEST) ||
        previous->request_mask != current->request_mask)
        changed |= APP_HISTORY_EVENT_REQUEST;
    if ((validity_changed & APP_HISTORY_VALID_PRESENCE) ||
        previous->presence != current->presence)
        changed |= APP_HISTORY_EVENT_PRESENCE;
    if (validity_changed || previous->quality != current->quality)
        changed |= APP_HISTORY_EVENT_QUALITY;
    return changed;
}

static void begin_session(app_history_t *history,
                           const app_history_input_t *data, uint32_t now_ms)
{
    uint32_t next_revision = history->stats.revision + 1U;
    uint32_t next_session = history->stats.session_id + 1U;
    memset(history, 0, sizeof(*history));
    history->stats.revision = next_revision;
    history->stats.session_id = next_session;
    history->stats.source = data->source;
    history->stats.initialized = 1;
    history->last_tick_ms = now_ms;
    history->previous = *data;
    append_sample(history, data, now_ms);
    append_event(history, data, APP_HISTORY_EVENT_SESSION);
}

static void account_request_interval(app_history_t *history,
                                      const app_history_input_t *current,
                                      uint32_t elapsed_ms)
{
    uint8_t endpoints_valid =
        (history->previous.valid_flags & APP_HISTORY_VALID_REQUEST) &&
        (current->valid_flags & APP_HISTORY_VALID_REQUEST);
    uint8_t was_cooling =
        (history->previous.request_mask & APP_HISTORY_COOLING_REQUEST) != 0;
    uint8_t is_cooling =
        (current->request_mask & APP_HISTORY_COOLING_REQUEST) != 0;
    if (endpoints_valid && elapsed_ms <= APP_HISTORY_MAX_KNOWN_GAP_MS)
    {
        if (was_cooling == is_cooling)
        {
            history->stats.request_known_ms += elapsed_ms;
            if (is_cooling) history->stats.cooling_request_ms += elapsed_ms;
            return;
        }
        if (!was_cooling && is_cooling &&
            history->stats.cooling_request_starts != UINT32_MAX)
            ++history->stats.cooling_request_starts;
    }
    /* A transition can have happened anywhere between endpoints. A missing or
     * late observation cannot prove what the request was during the gap. */
    history->stats.request_unknown_ms += elapsed_ms;
}

void app_history_init(app_history_t *history)
{
    if (history != NULL) memset(history, 0, sizeof(*history));
}

void app_history_observe(app_history_t *history,
                         const app_history_input_t *input, uint32_t now_ms)
{
    app_history_input_t data;
    uint32_t elapsed_ms;
    uint8_t changed;
    if (history == NULL || input == NULL) return;
    data = normalize_input(input);
    elapsed_ms = now_ms - history->last_tick_ms;
    if (!history->stats.initialized || data.source != history->stats.source ||
        elapsed_ms > (uint32_t)INT32_MAX)
    {
        begin_session(history, &data, now_ms);
        return;
    }
    history->stats.elapsed_ms += elapsed_ms;
    account_request_interval(history, &data, elapsed_ms);
    changed = event_changes(&history->previous, &data);
    if (changed != 0) append_event(history, &data, changed);
    if ((uint32_t)(now_ms - history->last_sample_tick_ms) >=
        APP_HISTORY_SAMPLE_INTERVAL_MS)
        append_sample(history, &data, now_ms);
    history->last_tick_ms = now_ms;
    history->previous = data;
}

const app_history_sample_t *app_history_sample_at(const app_history_t *history,
                                                 uint16_t oldest_index)
{
    uint32_t index;
    if (history == NULL || oldest_index >= history->stats.sample_count)
        return NULL;
    index = (history->sample_head + APP_HISTORY_SAMPLE_CAPACITY -
             history->stats.sample_count + oldest_index) %
            APP_HISTORY_SAMPLE_CAPACITY;
    return &history->samples[index];
}

const app_history_event_t *app_history_event_at(const app_history_t *history,
                                               uint16_t oldest_index)
{
    uint32_t index;
    if (history == NULL || oldest_index >= history->stats.event_count)
        return NULL;
    index = (history->event_head + APP_HISTORY_EVENT_CAPACITY -
             history->stats.event_count + oldest_index) %
            APP_HISTORY_EVENT_CAPACITY;
    return &history->events[index];
}

void app_history_get_stats(const app_history_t *history,
                            app_history_stats_t *stats)
{
    if (stats == NULL) return;
    if (history == NULL) memset(stats, 0, sizeof(*stats));
    else *stats = history->stats;
}
