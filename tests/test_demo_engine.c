#include "app_demo_engine.h"
#include "app_climate_logic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    app_climate_state_t climate;
    app_climate_inputs_t inputs;
    uint32_t now;
    uint32_t model_elapsed;
    uint32_t carry_ms;
    unsigned enters[APP_DEMO_COMPLETE + 1U];
    uint8_t saw_cooling, saw_sleep, saw_eco, saw_off, saw_fault, saw_recovery;
} fixture_t;

static void step_model(fixture_t *f)
{
    f->inputs.people_updated_ms = f->inputs.als_updated_ms = f->now;
    f->inputs.sensor_updated_ms = f->inputs.humidity_updated_ms = f->now;
    app_climate_step(&f->climate, &f->inputs, f->now);
    f->saw_cooling |= f->climate.cooling_request;
    f->saw_sleep |= f->climate.auto_sleep;
    f->saw_eco |= f->climate.auto_energy_saving && f->climate.power_on;
    f->saw_off |= !f->climate.power_on && f->climate.automatic_off;
    f->saw_fault |= f->climate.presence == APP_PRESENCE_UNKNOWN &&
                    !f->inputs.people_valid && f->climate.absence_ms == 0U;
    f->saw_recovery |= f->enters[APP_DEMO_RECOVER] &&
                       f->climate.presence == APP_PRESENCE_PRESENT;
}

static void scene(app_demo_phase_t phase, uint8_t enter, uint32_t model_ms, void *context)
{
    fixture_t *f = context;
    if (enter) {
        assert(phase >= APP_DEMO_ENTER && phase <= APP_DEMO_COMPLETE);
        f->enters[phase]++;
        switch (phase) {
        case APP_DEMO_ENTER:
            app_climate_init(&f->climate, 26U);
            f->inputs = (app_climate_inputs_t){0};
            f->inputs.people_valid = f->inputs.als_valid = 1U;
            f->inputs.sensor_valid = f->inputs.humidity_valid = 1U;
            f->inputs.people_count = 1;
            f->inputs.als = 250U;
            f->inputs.temperature = 25.0f;
            f->inputs.humidity = 55.0f;
            app_climate_apply_action(&f->climate, APP_ACTION_POWER);
            break;
        case APP_DEMO_WARM: f->inputs.temperature = 29.0f; break;
        case APP_DEMO_NIGHT: f->inputs.als = 50U; break;
        case APP_DEMO_ABSENCE_ECO: f->inputs.people_count = 0; break;
        case APP_DEMO_AI_FAULT:
            f->inputs.people_valid = 0U;
            f->inputs.temperature = 25.0f;
            assert(!f->climate.power_on && f->climate.automatic_off);
            app_climate_apply_action(&f->climate, APP_ACTION_POWER);
            break;
        case APP_DEMO_RECOVER:
            f->inputs.people_valid = 1U;
            f->inputs.people_count = 1;
            f->inputs.als = 250U;
            f->inputs.temperature = 27.0f;
            break;
        default: break;
        }
        if (phase != APP_DEMO_COMPLETE) { step_model(f); }
    }
    while (model_ms) {
        uint32_t until_step = 500U - f->carry_ms;
        uint32_t slice = model_ms < until_step ? model_ms : until_step;
        f->now += slice;
        f->model_elapsed += slice;
        f->carry_ms += slice;
        model_ms -= slice;
        if (f->carry_ms == 500U) { f->carry_ms = 0U; step_model(f); }
    }
}

static fixture_t run(uint32_t start, uint32_t stride)
{
    app_demo_engine_t engine;
    fixture_t f = {0};
    f.now = start;
    app_demo_start(&engine, scene, &f);
    while (engine.active) { app_demo_advance(&engine, stride, scene, &f); }
    assert(engine.elapsed_ms == APP_DEMO_DURATION_MS);
    assert(engine.phase == APP_DEMO_COMPLETE && !engine.paused);
    for (unsigned i = APP_DEMO_ENTER; i <= APP_DEMO_COMPLETE; i++) {
        assert(f.enters[i] == 1U);
    }
    assert(f.model_elapsed == 1909000U);
    assert(f.saw_cooling && f.saw_sleep && f.saw_eco && f.saw_off);
    assert(f.saw_fault && f.saw_recovery);
    return f;
}

int main(void)
{
    fixture_t reference = run(0U, UINT32_MAX);
    const uint32_t strides[] = {1U, 5U, 17U, 1500U, 54321U};
    for (unsigned i = 0U; i < sizeof(strides) / sizeof(strides[0]); i++) {
        fixture_t f = run(0U, strides[i]);
        assert(f.model_elapsed == reference.model_elapsed);
        assert(memcmp(&f.climate, &reference.climate, sizeof(f.climate)) == 0);
    }
    fixture_t wrapped = run(UINT32_MAX - 1000U, 17U);
    assert(wrapped.model_elapsed == reference.model_elapsed);
    assert(wrapped.saw_cooling && wrapped.saw_sleep && wrapped.saw_eco && wrapped.saw_off);
    app_demo_engine_t engine;
    fixture_t f = {0};
    app_demo_start(&engine, scene, &f);
    app_demo_advance(&engine, 6000U, scene, &f);
    app_demo_toggle_pause(&engine);
    app_demo_engine_t frozen = engine;
    fixture_t frozen_model = f;
    app_demo_advance(&engine, UINT32_MAX, scene, &f);
    assert(memcmp(&engine, &frozen, sizeof(engine)) == 0);
    assert(memcmp(&f, &frozen_model, sizeof(f)) == 0);
    app_demo_toggle_pause(&engine);
    app_demo_advance(&engine, 1U, scene, &f);
    assert(engine.elapsed_ms == frozen.elapsed_ms + 1U);
    app_demo_stop(&engine);
    assert(!engine.active && engine.phase == APP_DEMO_IDLE && !engine.elapsed_ms);
    app_demo_advance(&engine, UINT32_MAX, scene, &f);
    assert(engine.phase == APP_DEMO_IDLE);
    app_demo_init(NULL);
    app_demo_start(NULL, NULL, NULL);
    app_demo_advance(NULL, 1U, NULL, NULL);
    puts("PASS: scene schedule, real climate policies, stride invariance, wrap, pause and stop");
    return 0;
}
