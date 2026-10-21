#ifndef APP_DEMO_ENGINE_H
#define APP_DEMO_ENGINE_H

#include <stdint.h>

/* Scripted input demonstration only: no learned policy or hardware samples. */
typedef enum {
    APP_DEMO_IDLE = 0,
    APP_DEMO_ENTER,
    APP_DEMO_WARM,
    APP_DEMO_NIGHT,
    APP_DEMO_ABSENCE_ECO,
    APP_DEMO_ABSENCE_OFF,
    APP_DEMO_AI_FAULT,
    APP_DEMO_RECOVER,
    APP_DEMO_COMPLETE
} app_demo_phase_t;

#define APP_DEMO_DURATION_MS (56000UL)

typedef struct {
    uint32_t elapsed_ms;       /* Script wall duration, frozen while paused. */
    uint32_t phase_elapsed_ms;
    uint8_t active;
    uint8_t paused;
    uint8_t phase;
} app_demo_engine_t;

/* Each call is a phase-bounded slice. enter is set before that phase's first
   slice, including a zero-length start notification. model_ms must be passed
   through the host's normal sample/control scheduler, never a direct jump. */
typedef void (*app_demo_step_cb_t)(app_demo_phase_t phase, uint8_t enter,
                                  uint32_t model_ms, void *context);

void app_demo_init(app_demo_engine_t *engine);
void app_demo_start(app_demo_engine_t *engine, app_demo_step_cb_t step,
                    void *context);
void app_demo_toggle_pause(app_demo_engine_t *engine);
void app_demo_stop(app_demo_engine_t *engine);
void app_demo_advance(app_demo_engine_t *engine, uint32_t wall_ms,
                      app_demo_step_cb_t step, void *context);
uint32_t app_demo_speed(const app_demo_engine_t *engine);
const char *app_demo_phase_name(app_demo_phase_t phase);

#endif
