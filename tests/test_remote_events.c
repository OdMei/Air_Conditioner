#include <assert.h>
#include <stdio.h>
#include "remote.h"

GPIO_TypeDef mock_gpioe, mock_gpiog;
TIM_HandleTypeDef htim1 = {TIM1};
static uint32_t s_tick, s_mask, s_capture;
static GPIO_PinState s_level;

uint32_t HAL_GetTick(void) { return s_tick; }
uint32_t __get_PRIMASK(void) { return s_mask; }
void __disable_irq(void) { s_mask = 1U; }
void __set_PRIMASK(uint32_t value) { s_mask = value; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    assert(port == GPIOE && pin == GPIO_PIN_9);
    return s_level;
}
HAL_StatusTypeDef HAL_TIM_IC_Start_IT(TIM_HandleTypeDef *handle, uint32_t channel)
{
    assert(handle == &htim1 && channel == TIM_CHANNEL_1);
    return HAL_OK;
}
uint32_t HAL_TIM_ReadCapturedValue(TIM_HandleTypeDef *handle, uint32_t channel)
{
    assert(handle == &htim1 && channel == TIM_CHANNEL_1);
    return s_capture;
}

static void space(uint32_t microseconds)
{
    s_level = GPIO_PIN_SET;
    HAL_TIM_IC_CaptureCallback(&htim1);
    s_capture = microseconds;
    s_level = GPIO_PIN_RESET;
    HAL_TIM_IC_CaptureCallback(&htim1);
}

static void frame(uint8_t key, uint8_t corrupt, unsigned int bits)
{
    uint32_t word = 0xFF00U | ((uint32_t)key << 16U) |
                    ((uint32_t)(uint8_t)~key << 24U);
    if (corrupt != 0U) { word ^= 1UL << 24U; }
    space(4500U);
    for (unsigned int bit = 0U; bit < bits; bit++)
    {
        space((word & (1UL << bit)) != 0U ? 1600U : 500U);
    }
}

static void reset_remote(uint32_t tick)
{
    s_tick = tick;
    s_mask = 0U;
    remote_init();
}

static void test_toggle_once_and_full_frame_repeat(void)
{
    reset_remote(0U);
    frame(69U, 0U, 32U);
    assert(remote_scan() == 69U);
    for (unsigned int poll = 0U; poll < 20U; poll++)
    {
        s_tick += 5U;
        HAL_TIM_PeriodElapsedCallback(&htim1);
        assert(remote_scan() == 0U);
    }
    s_tick = 108U;
    space(2250U);
    assert(remote_scan() == 0U);
    s_tick = 216U;
    frame(69U, 0U, 32U); /* NEC2 full-frame hold */
    assert(remote_scan() == 0U);
    s_tick = 467U; /* Release, then the same key is a new press. */
    frame(69U, 0U, 32U);
    assert(remote_scan() == 69U);
}

static void test_temperature_repeats_and_different_key(void)
{
    reset_remote(0U);
    frame(9U, 0U, 32U);
    assert(remote_scan() == 9U);
    s_tick = 108U; space(2250U); assert(remote_scan() == 0U);
    s_tick = 199U; space(2250U); assert(remote_scan() == 0U);
    s_tick = 200U; space(2250U); assert(remote_scan() == 9U);
    assert(remote_scan() == 0U);
    s_tick = 300U; frame(9U, 0U, 32U); assert(remote_scan() == 0U);
    s_tick = 400U; frame(9U, 0U, 32U); assert(remote_scan() == 9U);
    s_tick = 410U; frame(7U, 0U, 32U); assert(remote_scan() == 7U);
    s_tick = 610U; space(2250U); assert(remote_scan() == 7U);
}

static void test_bad_partial_frames_and_tick_wrap(void)
{
    reset_remote(0U);
    frame(69U, 0U, 31U);
    HAL_TIM_PeriodElapsedCallback(&htim1);
    assert(remote_scan() == 0U);
    frame(69U, 1U, 32U);
    assert(remote_scan() == 0U);
    space(2250U);
    assert(remote_scan() == 0U);

    reset_remote(UINT32_MAX - 90U);
    frame(7U, 0U, 32U);
    s_mask = 1U;
    assert(remote_scan() == 7U && s_mask == 1U);
    s_mask = 0U;
    s_tick = 108U; space(2250U); assert(remote_scan() == 0U);
    s_tick = 109U; space(2250U); assert(remote_scan() == 7U);
    s_tick = 360U; space(2250U); assert(remote_scan() == 0U);
}

int main(void)
{
    test_toggle_once_and_full_frame_repeat();
    test_temperature_repeats_and_different_key();
    test_bad_partial_frames_and_tick_wrap();
    puts("Remote event tests passed: one press, bounded repeats, invalid frames, wrap");
    return 0;
}
