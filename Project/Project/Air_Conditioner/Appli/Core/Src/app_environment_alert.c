#include "app_environment_alert.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define APP_ENV_TEMPERATURE_HYSTERESIS_X10 (10)
#define APP_ENV_HUMIDITY_HYSTERESIS_X10 (30)

static uint32_t bounded_duration(uint32_t value, uint32_t minimum,
                                 uint32_t maximum)
{
    if (value < minimum) return minimum;
    return value > maximum ? maximum : value;
}

static void sanitize_config(app_environment_config_t *result,
                             const app_environment_config_t *config)
{
    memset(result, 0, sizeof(*result));
    result->temperature_high_x10 = config->temperature_high_x10;
    if (result->temperature_high_x10 < 160) result->temperature_high_x10 = 160;
    if (result->temperature_high_x10 > 400) result->temperature_high_x10 = 400;
    result->humidity_high = config->humidity_high;
    if (result->humidity_high < 30U) result->humidity_high = 30U;
    if (result->humidity_high > 95U) result->humidity_high = 95U;
    result->hold_ms = bounded_duration(config->hold_ms, APP_ENV_HOLD_MIN_MS,
                                       APP_ENV_HOLD_MAX_MS);
    result->snooze_ms = bounded_duration(config->snooze_ms,
                                         APP_ENV_SNOOZE_MIN_MS,
                                         APP_ENV_SNOOZE_MAX_MS);
}

static app_environment_events_t channel_event(uint8_t channel, uint8_t event)
{
    return (app_environment_events_t)((uint16_t)event << (channel * 8U));
}

static app_environment_events_t reset_channels(app_environment_alert_t *state,
                                                uint8_t event)
{
    uint8_t channel;
    app_environment_events_t events = 0U;
    memset(state->channels, 0, sizeof(state->channels));
    for (channel = 0U; channel < APP_ENV_CHANNEL_COUNT; ++channel)
        events |= channel_event(channel, event);
    return events;
}

/* An unsigned delta above INT32_MAX means the caller's clock moved backwards,
 * rather than the ordinary ~49-day uint32 wrap. No old timer survives it. */
static app_environment_events_t update_clock(app_environment_alert_t *state,
                                              uint32_t now_ms)
{
    app_environment_events_t events = 0U;
    if (state->clock_initialized &&
        (uint32_t)(now_ms - state->last_updated_ms) > INT32_MAX)
        events = reset_channels(state, APP_ENV_EVENT_CONFIG_RESET);
    state->last_updated_ms = now_ms;
    state->clock_initialized = 1U;
    return events;
}

void app_environment_config_defaults(app_environment_config_t *config)
{
    if (config == NULL) return;
    memset(config, 0, sizeof(*config));
    config->temperature_high_x10 = 300;
    config->humidity_high = 70U;
    config->hold_ms = APP_ENV_DEFAULT_HOLD_MS;
    config->snooze_ms = APP_ENV_DEFAULT_SNOOZE_MS;
}

void app_environment_init(app_environment_alert_t *state,
                          const app_environment_config_t *config,
                          uint8_t source)
{
    app_environment_config_t effective_config;
    if (state == NULL) return;
    /* Copy first: callers may reinitialise using &state->config. */
    if (config == NULL) app_environment_config_defaults(&effective_config);
    else sanitize_config(&effective_config, config);
    memset(state, 0, sizeof(*state));
    state->config = effective_config;
    state->source = source;
}

app_environment_events_t app_environment_set_config(
    app_environment_alert_t *state, const app_environment_config_t *config,
    uint32_t now_ms)
{
    app_environment_config_t sanitized;
    app_environment_events_t events;
    if (state == NULL || config == NULL) return 0U;
    events = update_clock(state, now_ms);
    sanitize_config(&sanitized, config);
    if (sanitized.temperature_high_x10 == state->config.temperature_high_x10 &&
        sanitized.humidity_high == state->config.humidity_high &&
        sanitized.hold_ms == state->config.hold_ms &&
        sanitized.snooze_ms == state->config.snooze_ms)
        return events;
    state->config = sanitized;
    return events | reset_channels(state, APP_ENV_EVENT_CONFIG_RESET);
}

static uint8_t update_channel(app_environment_channel_state_t *channel,
                              uint8_t fresh, int16_t value_x10,
                              int16_t threshold_x10, int16_t hysteresis_x10,
                              const app_environment_config_t *config,
                              uint32_t now_ms)
{
    uint8_t events = 0U;
    if (!fresh)
    {
        if (channel->fresh) events |= APP_ENV_EVENT_UNKNOWN;
        channel->fresh = 0U;
        channel->pending = 0U;
        /* Keep an existing alert, acknowledgement and snooze: missing data
         * cannot establish that the environment has recovered. */
        return events;
    }
    channel->fresh = 1U;
    channel->value_x10 = value_x10;
    if (channel->snoozed &&
        (uint32_t)(now_ms - channel->snooze_since_ms) >= config->snooze_ms)
        channel->snoozed = 0U;

    if (channel->raised)
    {
        if (value_x10 <= threshold_x10 - hysteresis_x10)
        {
            channel->raised = 0U;
            channel->acknowledged = 0U;
            channel->snoozed = 0U;
            channel->pending = 0U;
            events |= APP_ENV_EVENT_RECOVERED;
        }
        return events;
    }

    if (value_x10 < threshold_x10)
    {
        channel->pending = 0U;
        return events;
    }
    if (!channel->pending)
    {
        channel->pending = 1U;
        channel->pending_since_ms = now_ms;
    }
    if ((uint32_t)(now_ms - channel->pending_since_ms) >= config->hold_ms)
    {
        channel->pending = 0U;
        channel->raised = 1U;
        channel->raised_since_ms = now_ms;
        channel->acknowledged = 0U;
        channel->snoozed = 0U;
        events |= APP_ENV_EVENT_RAISED;
    }
    return events;
}

app_environment_events_t app_environment_update(
    app_environment_alert_t *state, const app_environment_input_t *input,
    uint32_t now_ms)
{
    app_environment_events_t events;
    uint8_t temperature_fresh;
    uint8_t humidity_fresh;
    if (state == NULL || input == NULL) return 0U;
    events = update_clock(state, now_ms);
    if (input->source != state->source)
    {
        events |= reset_channels(state, APP_ENV_EVENT_SOURCE_RESET);
        state->source = input->source;
    }
    temperature_fresh = input->temperature_valid &&
        input->temperature_x10 >= -400 && input->temperature_x10 <= 1250;
    humidity_fresh = input->humidity_valid && input->humidity_x10 <= 1000U;
    events |= channel_event(APP_ENV_CHANNEL_TEMPERATURE,
        update_channel(&state->channels[APP_ENV_CHANNEL_TEMPERATURE],
                       temperature_fresh, input->temperature_x10,
                       state->config.temperature_high_x10,
                       APP_ENV_TEMPERATURE_HYSTERESIS_X10,
                       &state->config, now_ms));
    events |= channel_event(APP_ENV_CHANNEL_HUMIDITY,
        update_channel(&state->channels[APP_ENV_CHANNEL_HUMIDITY],
                       humidity_fresh, (int16_t)input->humidity_x10,
                       (int16_t)(state->config.humidity_high * 10U),
                       APP_ENV_HUMIDITY_HYSTERESIS_X10,
                       &state->config, now_ms));
    return events;
}

app_environment_events_t app_environment_ack(app_environment_alert_t *state,
                                             uint8_t channel_mask,
                                             uint32_t now_ms)
{
    uint8_t index;
    app_environment_events_t events;
    if (state == NULL) return 0U;
    events = update_clock(state, now_ms);
    for (index = 0U; index < APP_ENV_CHANNEL_COUNT; ++index)
    {
        app_environment_channel_state_t *channel = &state->channels[index];
        if ((channel_mask & (1U << index)) && channel->raised &&
            !channel->acknowledged)
        {
            channel->acknowledged = 1U;
            channel->snoozed = 0U;
            events |= channel_event(index, APP_ENV_EVENT_ACK);
        }
    }
    return events;
}

app_environment_events_t app_environment_snooze(app_environment_alert_t *state,
                                                uint8_t channel_mask,
                                                uint32_t now_ms)
{
    uint8_t index;
    app_environment_events_t events;
    if (state == NULL) return 0U;
    events = update_clock(state, now_ms);
    for (index = 0U; index < APP_ENV_CHANNEL_COUNT; ++index)
    {
        app_environment_channel_state_t *channel = &state->channels[index];
        if ((channel_mask & (1U << index)) && channel->raised)
        {
            channel->acknowledged = 0U;
            channel->snoozed = 1U;
            channel->snooze_since_ms = now_ms;
            events |= channel_event(index, APP_ENV_EVENT_SNOOZE);
        }
    }
    return events;
}

static uint32_t elapsed(uint32_t now_ms, uint32_t since_ms)
{
    uint32_t duration = now_ms - since_ms;
    return duration > INT32_MAX ? 0U : duration;
}

void app_environment_get_view(const app_environment_alert_t *state,
                              uint32_t now_ms, app_environment_view_t *view)
{
    uint8_t index;
    if (state == NULL || view == NULL) return;
    memset(view, 0, sizeof(*view));
    view->config = state->config;
    view->source = state->source;
    for (index = 0U; index < APP_ENV_CHANNEL_COUNT; ++index)
    {
        const app_environment_channel_state_t *channel = &state->channels[index];
        app_environment_channel_view_t *output = &view->channels[index];
        output->fresh = channel->fresh;
        output->raised = channel->raised;
        output->acknowledged = channel->acknowledged;
        output->value_x10 = channel->value_x10;
        output->threshold_x10 = index == APP_ENV_CHANNEL_TEMPERATURE ?
            state->config.temperature_high_x10 :
            (int16_t)(state->config.humidity_high * 10U);
        if (channel->pending)
            output->pending_ms = elapsed(now_ms, channel->pending_since_ms);
        if (channel->raised)
            output->active_ms = elapsed(now_ms, channel->raised_since_ms);
        if (channel->snoozed)
        {
            uint32_t snooze_elapsed = elapsed(now_ms, channel->snooze_since_ms);
            if (snooze_elapsed < state->config.snooze_ms)
                output->snooze_remaining_ms = state->config.snooze_ms -
                    snooze_elapsed;
        }
        if (!channel->fresh) output->status = APP_ENV_UNKNOWN;
        else if (channel->raised)
        {
            if (output->snooze_remaining_ms) output->status = APP_ENV_SNOOZED;
            else if (channel->acknowledged) output->status = APP_ENV_ACKNOWLEDGED;
            else output->status = APP_ENV_ACTIVE;
        }
        else output->status = channel->pending ? APP_ENV_PENDING : APP_ENV_NORMAL;
    }
}
