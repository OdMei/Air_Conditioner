#include "app_demo_engine.h"
#include <string.h>

typedef struct { uint32_t wall_ms; uint32_t speed; } demo_segment_t;
static const demo_segment_t s_segments[] = {
    {4000U, 1U}, {8000U, 4U}, {5000U, 1U}, {11000U, 60U},
    {20000U, 60U}, {4000U, 1U}, {4000U, 1U}
};

void app_demo_init(app_demo_engine_t *engine)
{
    if (engine) { memset(engine, 0, sizeof(*engine)); }
}

void app_demo_start(app_demo_engine_t *engine, app_demo_step_cb_t step,
                    void *context)
{
    if (!engine) { return; }
    app_demo_init(engine);
    engine->active = 1U;
    engine->phase = APP_DEMO_ENTER;
    if (step) { step(APP_DEMO_ENTER, 1U, 0U, context); }
}

void app_demo_toggle_pause(app_demo_engine_t *engine)
{
    if (engine && engine->active) { engine->paused = (uint8_t)!engine->paused; }
}

void app_demo_stop(app_demo_engine_t *engine)
{
    app_demo_init(engine);
}

uint32_t app_demo_speed(const app_demo_engine_t *engine)
{
    if (!engine || !engine->active || engine->phase < APP_DEMO_ENTER ||
        engine->phase > APP_DEMO_RECOVER) { return 1U; }
    return s_segments[engine->phase - APP_DEMO_ENTER].speed;
}

void app_demo_advance(app_demo_engine_t *engine, uint32_t wall_ms,
                      app_demo_step_cb_t step, void *context)
{
    if (!engine || !engine->active || engine->paused) { return; }
    /* The maximum script duration bounds both iteration count and product.
       Even UINT32_MAX input cannot wrap elapsed_ms or overrun a segment. */
    while (wall_ms && engine->active) {
        const demo_segment_t *segment = &s_segments[engine->phase - APP_DEMO_ENTER];
        uint32_t remaining = segment->wall_ms - engine->phase_elapsed_ms;
        uint32_t slice = wall_ms < remaining ? wall_ms : remaining;
        if (step) {
            step((app_demo_phase_t)engine->phase, 0U, slice * segment->speed, context);
        }
        engine->phase_elapsed_ms += slice;
        engine->elapsed_ms += slice;
        wall_ms -= slice;
        if (engine->phase_elapsed_ms == segment->wall_ms) {
            engine->phase_elapsed_ms = 0U;
            engine->phase++;
            if (engine->phase == APP_DEMO_COMPLETE) { engine->active = 0U; }
            if (step) { step((app_demo_phase_t)engine->phase, 1U, 0U, context); }
        }
    }
}

const char *app_demo_phase_name(app_demo_phase_t phase)
{
    switch (phase) {
    case APP_DEMO_ENTER: return "Occupied entry";
    case APP_DEMO_WARM: return "Warm room / cooling request";
    case APP_DEMO_NIGHT: return "Dark occupied / sleep";
    case APP_DEMO_ABSENCE_ECO: return "Absence / ten-minute ECO";
    case APP_DEMO_ABSENCE_OFF: return "Absence / thirty-minute shutdown";
    case APP_DEMO_AI_FAULT: return "AI fault / unknown occupancy";
    case APP_DEMO_RECOVER: return "AI recovery";
    case APP_DEMO_COMPLETE: return "Completed / manual state restored";
    default: return "Idle";
    }
}
