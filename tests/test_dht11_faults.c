#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "dht11.h"

GPIO_TypeDef mock_gpioe, mock_gpiog;
typedef struct { uint32_t until; GPIO_PinState level; } phase_t;
static phase_t s_phases[90];
static unsigned int s_phase_count;
static uint32_t s_us, s_release_us;
static uint8_t s_started;
static GPIO_PinState s_tail;
static unsigned int s_reads;
static uint32_t s_irq_mask, s_ipsr, s_critical_start, s_capture_max_us;
static uint32_t s_basepri, s_basepri_start, s_high_irq_us, s_high_irqs;
static uint32_t s_read_cost_us, s_preempt_us, s_preemptions;
static unsigned int s_mask_entries, s_mask_exits, s_delays;
static unsigned int s_capture_entries, s_capture_exits, s_barriers;

uint32_t HAL_GetTick(void) { return s_us / 1000U; }
uint32_t __get_PRIMASK(void) { return s_irq_mask; }
uint32_t __get_IPSR(void) { return s_ipsr; }
uint32_t __get_BASEPRI(void) { return s_basepri; }
void __set_BASEPRI(uint32_t value)
{
    assert(s_irq_mask == 0U);
    if (value != 0U)
    {
        assert(value == (8U << (8U - __NVIC_PRIO_BITS)) && s_basepri == 0U);
        s_basepri_start = s_us;
        s_capture_entries++;
    }
    else
    {
        assert(s_basepri != 0U);
        uint32_t duration = s_us - s_basepri_start;
        if (duration > s_capture_max_us) { s_capture_max_us = duration; }
        s_capture_exits++;
    }
    s_basepri = value;
}
void __DSB(void) { s_barriers++; }
void __ISB(void) { s_barriers++; }
void __disable_irq(void)
{
    if (s_irq_mask == 0U) { s_critical_start = s_us; }
    s_irq_mask = 1U;
    s_mask_entries++;
}
void __set_PRIMASK(uint32_t value)
{
    if (s_irq_mask != 0U && value == 0U)
    {
        /* Diagnostic snapshots alone use very short PRIMASK protection. */
        assert(s_us - s_critical_start == 0U);
    }
    s_irq_mask = value;
    s_mask_exits++;
}
void HAL_Delay(uint32_t milliseconds)
{
    assert(s_irq_mask == 0U && s_basepri == 0U && s_ipsr == 0U);
    assert(milliseconds == 18U);
    s_delays++;
    s_us += milliseconds * 1000U;
    if (s_preempt_us != 0U) { s_us += s_preempt_us; s_preemptions++; }
}
void sys_delay_us(uint32_t us)
{
    s_us += us;
    if (s_irq_mask == 0U && s_basepri == 0U && s_preempt_us != 0U)
    { s_us += s_preempt_us; s_preemptions++; }
    if (s_high_irq_us != 0U && us == 40U)
    {
        assert(s_irq_mask == 0U && s_basepri == (8U << 4U));
        /* Priority 7 camera/CSI (and priority 0 IR) remain eligible. */
        s_us += s_high_irq_us;
        s_high_irqs++;
    }
}
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState level)
{
    assert(port == GPIOG && pin == GPIO_PIN_3);
    if (level == GPIO_PIN_SET)
    {
        assert(s_irq_mask == 0U && s_basepri == (8U << 4U));
        s_release_us = s_us;
        s_started = 1U;
    }
    else { assert(s_irq_mask == 0U && s_basepri == 0U); }
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    assert(port == GPIOG && pin == GPIO_PIN_3 && s_started != 0U);
    assert(s_irq_mask == 0U && s_basepri == (8U << 4U));
    uint32_t elapsed = s_us - s_release_us;
    GPIO_PinState level = s_tail;
    s_reads++;
    for (unsigned int index = 0U; index < s_phase_count; index++)
    {
        if (elapsed < s_phases[index].until)
        { level = s_phases[index].level; break; }
    }
    s_us += s_read_cost_us; /* The real HAL call and loop are not free. */
    return level;
}

static void phase(uint32_t length, GPIO_PinState level)
{
    uint32_t previous = s_phase_count == 0U ? 0U : s_phases[s_phase_count - 1U].until;
    s_phases[s_phase_count++] = (phase_t){previous + length, level};
}

static void prepare_widths(const uint8_t bytes[5], uint32_t zero_us,
                           uint32_t one_us, uint8_t jitter)
{
    s_us = s_release_us = 0U;
    s_started = 0U;
    s_reads = s_phase_count = 0U;
    s_irq_mask = s_ipsr = s_capture_max_us = 0U;
    s_basepri = s_high_irq_us = s_high_irqs = 0U;
    s_mask_entries = s_mask_exits = s_delays = 0U;
    s_capture_entries = s_capture_exits = s_barriers = 0U;
    s_read_cost_us = 1U;
    s_preempt_us = s_preemptions = 0U;
    s_tail = GPIO_PIN_SET;
    phase(20U, GPIO_PIN_SET);
    phase(80U, GPIO_PIN_RESET);
    phase(80U, GPIO_PIN_SET);
    for (unsigned int byte = 0U; byte < 5U; byte++)
    {
        for (int bit = 7; bit >= 0; bit--)
        {
            uint32_t variation = jitter != 0U ? (byte + (unsigned int)bit) % 3U : 0U;
            phase(49U + variation, GPIO_PIN_RESET);
            phase((bytes[byte] & (1U << bit)) != 0U ? one_us + variation : zero_us + variation,
                  GPIO_PIN_SET);
        }
    }
    phase(100U, GPIO_PIN_RESET);
}

static void prepare(const uint8_t bytes[5])
{
    prepare_widths(bytes, 26U, 70U, 0U);
}

static dht11_diag_t get_diag(void)
{
    dht11_diag_t value;
    memset(&value, 0, sizeof(value));
    dht11_get_diag(&value);
    return value;
}

static void assert_restored(void)
{
    assert(s_irq_mask == 0U);
    assert(s_basepri == 0U);
    assert(s_mask_entries == s_mask_exits);
    assert(s_capture_entries == s_capture_exits);
    assert(s_barriers == s_capture_entries * 4U);
    assert(s_capture_max_us < 5200U);
}

static void test_good_and_zero_frames(void)
{
    const uint8_t good[5] = {55U, 0U, 27U, 0U, 82U};
    const uint8_t zero[5] = {0U, 0U, 0U, 0U, 0U};
    const uint8_t negative[5] = {55U, 0U, 5U, 0x83U, 191U};
    float temperature = -100.0f, humidity = -100.0f;
    prepare(good);
    assert(dht11_get_data(&temperature, &humidity) == 0U);
    assert(temperature == 27.0f && humidity == 55.0f);
    assert(s_capture_max_us >= 2500U && s_capture_max_us < 5200U);
    assert_restored();
    for (uint32_t irq_us = 5U; irq_us <= 15U; irq_us += 5U)
    {
        prepare_widths(good, 26U, 68U, 1U);
        s_high_irq_us = irq_us;
        temperature = -100.0f; humidity = -100.0f;
        assert(dht11_get_data(&temperature, &humidity) == 0U);
        assert(temperature == 27.0f && humidity == 55.0f);
        assert(s_high_irqs == 40U);
        assert_restored();
    }
    prepare(good);
    s_high_irq_us = 90U; /* Too long for the available sampling margin. */
    temperature = 28.5f; humidity = 61.5f;
    assert(dht11_get_data(&temperature, &humidity) == 1U);
    assert(temperature == 28.5f && humidity == 61.5f);
    assert(get_diag().last_status != DHT11_STATUS_OK && s_high_irqs != 0U);
    assert_restored();
    prepare(zero);
    assert(dht11_get_data(&temperature, &humidity) == 0U);
    assert(temperature == 0.0f && humidity == 0.0f);
    assert_restored();
    prepare(negative);
    assert(dht11_get_data(&temperature, &humidity) == 0U);
    assert(fabsf(temperature + 5.3f) < 0.0001f && humidity == 55.0f);
    assert_restored();
}

static void test_pulse_width_margin_and_preemption(void)
{
    const uint8_t frame[5] = {55U, 0U, 27U, 0U, 82U};
    for (uint32_t zero_us = 26U; zero_us <= 28U; zero_us++)
    {
        for (uint32_t one_us = 68U; one_us <= 72U; one_us++)
        {
            float temperature = -100.0f, humidity = -100.0f;
            prepare_widths(frame, zero_us, one_us, 0U);
            assert(dht11_get_data(&temperature, &humidity) == 0U);
            assert(temperature == 27.0f && humidity == 55.0f);
            assert_restored();
        }
    }
    for (uint32_t cost = 0U; cost <= 2U; cost++)
    {
        float temperature = -100.0f, humidity = -100.0f;
        prepare_widths(frame, 26U, 68U, 1U);
        s_read_cost_us = cost;
        s_preempt_us = 200U;
        assert(dht11_get_data(&temperature, &humidity) == 0U);
        assert(temperature == 27.0f && humidity == 55.0f);
        assert(s_preemptions == 1U); /* Start pulse permits ISR scheduling. */
        assert_restored(); /* Capture defers thread switches, not camera IRQs. */
    }
    /* A deliberately dense bit frame bounds capture time independently of
       the normal room values. The driver only checks framing/checksum. */
    const uint8_t dense[5] = {255U, 255U, 255U, 255U, 252U};
    float temperature = -100.0f, humidity = -100.0f;
    prepare_widths(dense, 28U, 72U, 1U);
    s_read_cost_us = 2U;
    assert(dht11_get_data(&temperature, &humidity) == 0U);
    assert(s_capture_max_us > 4700U && s_capture_max_us < 5200U);
    assert_restored();
}

static void test_faults_do_not_change_outputs(void)
{
    const uint8_t good[5] = {55U, 0U, 27U, 0U, 82U};
    const uint8_t bad_sum[5] = {55U, 0U, 27U, 0U, 81U};
    const dht11_status_t expected[5] = {
        DHT11_STATUS_NO_RESPONSE, DHT11_STATUS_BIT_HIGH_TIMEOUT,
        DHT11_STATUS_BIT_LOW_TIMEOUT, DHT11_STATUS_CHECKSUM,
        DHT11_STATUS_RESPONSE_TIMEOUT
    };
    for (unsigned int failure = 0U; failure < 5U; failure++)
    {
        float temperature = 28.5f, humidity = 61.5f;
        prepare(good);
        if (failure == 0U) { s_phase_count = 0U; } /* No handshake. */
        if (failure == 1U) { s_phase_count = 3U; } /* Handshake, stuck high. */
        if (failure == 2U) { s_phase_count = 28U; s_tail = GPIO_PIN_RESET; }
        if (failure == 3U) { prepare(bad_sum); }
        if (failure == 4U) { s_phase_count = 1U; s_tail = GPIO_PIN_RESET; }
        dht11_diag_t before = get_diag();
        assert(dht11_get_data(&temperature, &humidity) == 1U);
        assert(temperature == 28.5f && humidity == 61.5f);
        assert(s_reads < 15000U);
        dht11_diag_t after = get_diag();
        assert(after.last_status == expected[failure]);
        assert(after.attempts == before.attempts + 1U);
        assert(after.failures == before.failures + 1U);
        assert(after.successes == before.successes);
        assert(after.last_success_ms == before.last_success_ms && after.has_sample == before.has_sample);
        assert(after.consecutive_failures == before.consecutive_failures + 1U);
        if (failure == 0U || failure == 4U) { assert(s_capture_max_us < 250U); }
        if (failure == 1U) { assert(s_capture_max_us < 450U); }
        assert_restored();
    }
    prepare(good);
    float value = 10.0f;
    assert(dht11_get_data(NULL, &value) == 1U && s_reads == 0U);
    assert(dht11_get_data(&value, NULL) == 1U && value == 10.0f);
    assert(get_diag().last_status == DHT11_STATUS_BAD_ARGUMENT);
    assert(s_delays == 0U);
    assert_restored();
}

static void test_context_and_initialization(void)
{
    const uint8_t frame[5] = {55U, 0U, 27U, 0U, 82U};
    float temperature = 28.5f, humidity = 61.5f;
    prepare(frame);
    s_ipsr = 16U;
    assert(dht11_get_data(&temperature, &humidity) == 1U);
    assert(dht11_init() == 1U);
    assert(get_diag().last_status == DHT11_STATUS_BAD_CONTEXT);
    assert(s_reads == 0U && s_delays == 0U && s_irq_mask == 0U);
    assert(temperature == 28.5f && humidity == 61.5f);
    prepare(frame);
    s_irq_mask = 1U;
    assert(dht11_get_data(&temperature, &humidity) == 1U);
    assert(dht11_init() == 1U);
    assert(get_diag().last_status == DHT11_STATUS_BAD_CONTEXT);
    assert(s_reads == 0U && s_delays == 0U && s_irq_mask == 1U);
    assert(temperature == 28.5f && humidity == 61.5f);
    s_irq_mask = 0U;
    prepare(frame);
    s_basepri = 128U;
    assert(dht11_get_data(&temperature, &humidity) == 1U);
    assert(dht11_init() == 1U);
    assert(get_diag().last_status == DHT11_STATUS_BAD_CONTEXT);
    assert(s_reads == 0U && s_delays == 0U && s_basepri == 128U);
    assert(s_irq_mask == 0U && temperature == 28.5f && humidity == 61.5f);
    s_basepri = 0U;
    prepare(frame);
    dht11_diag_t before = get_diag();
    assert(dht11_init() == 0U);
    dht11_diag_t after = get_diag();
    assert(before.attempts == after.attempts && before.successes == after.successes);
    assert(before.has_sample == after.has_sample && before.last_success_ms == after.last_success_ms);
    assert_restored();
    prepare(frame);
    assert(dht11_get_data(&temperature, &humidity) == 0U);
    after = get_diag();
    assert(after.last_status == DHT11_STATUS_OK && after.has_sample == 1U);
    assert(after.consecutive_failures == 0U && after.last_attempt_ms == 0U);
    assert(after.last_success_ms == HAL_GetTick());
    assert_restored();
    dht11_get_diag(NULL);
}

int main(void)
{
    const uint8_t first_probe[5] = {55U, 0U, 27U, 0U, 82U};
    dht11_diag_t initial = get_diag();
    assert(initial.attempts == 0U && initial.has_sample == 0U);
    assert(initial.last_status == DHT11_STATUS_NOT_READ);
    prepare(first_probe);
    assert(dht11_init() == 0U);
    initial = get_diag();
    assert(initial.attempts == 0U && initial.successes == 0U && initial.has_sample == 0U);
    assert(initial.last_status == DHT11_STATUS_NOT_READ);
    assert_restored();
    test_good_and_zero_frames();
    test_pulse_width_margin_and_preemption();
    test_faults_do_not_change_outputs();
    test_context_and_initialization();
    puts("DHT11 waveform tests passed: pulse margin, GPIO cost, bounded BASEPRI capture, preserved camera/IR IRQs, context guards, diagnostics, checksum and unchanged outputs");
    return 0;
}
