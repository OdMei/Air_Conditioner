#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "uart.h"

GPIO_TypeDef mock_gpioe, mock_gpiog;
UART_HandleTypeDef huart1 = {USART1};
static uint32_t s_mask, s_ipsr, s_calls, s_last_timeout;
static HAL_StatusTypeDef s_result = HAL_OK;
static char s_wire[32768];
static size_t s_wire_length;
static uint8_t s_reenter;
int __io_putchar(int ch);

uint32_t __get_PRIMASK(void) { return s_mask; }
void __disable_irq(void) { s_mask = 1U; }
void __set_PRIMASK(uint32_t value) { s_mask = value; }
uint32_t __get_IPSR(void) { return s_ipsr; }
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *handle,
                                  const uint8_t *data, uint16_t size,
                                  uint32_t timeout)
{
    assert(handle == &huart1 && s_mask == 0U && s_ipsr == 0U);
    assert(size > 0U && size <= 64U && timeout <= 10U);
    s_calls++;
    s_last_timeout = timeout;
    if (s_reenter != 0U)
    {
        s_reenter = 0U;
        uart_tx_process(); /* A second consumer must not race the ring tail. */
        assert(uart_tx_enqueue_line("during\n", 7U) == 1U);
    }
    if (s_result == HAL_OK)
    {
        assert(s_wire_length + size <= sizeof(s_wire));
        memcpy(s_wire + s_wire_length, data, size);
        s_wire_length += size;
    }
    return s_result;
}

static uart_tx_stats_t stats(void)
{
    uart_tx_stats_t result;
    uart_tx_get_stats(&result);
    return result;
}
static void drain(void)
{
    while (stats().queued_bytes != 0U) { uart_tx_process(); }
}

static void test_startup_and_no_producer_hal_calls(void)
{
    assert(__io_putchar('a') == 'a' && s_calls == 1U && s_last_timeout == 2U);
    s_result = HAL_TIMEOUT;
    assert(__io_putchar('b') == EOF && s_calls == 2U);
    s_result = HAL_OK;
    s_ipsr = 1U;
    assert(__io_putchar('c') == EOF && s_calls == 2U);
    s_ipsr = 0U;
    s_mask = 1U;
    assert(__io_putchar('d') == EOF && s_calls == 2U && s_mask == 1U);
    s_mask = 0U;
    uart_tx_start();
    s_calls = 0U; s_wire_length = 0U;
    assert(uart_tx_enqueue_line("one\n", 4U) == 1U && s_calls == 0U);
    s_mask = 1U;
    assert(uart_tx_enqueue_line("two\n", 4U) == 1U && s_mask == 1U);
    assert(stats().queued_bytes == 8U && s_mask == 1U);
    uart_tx_process();
    assert(s_calls == 0U && s_mask == 1U && stats().queued_bytes == 8U);
    s_mask = 0U;
    s_ipsr = 1U; uart_tx_process(); assert(s_calls == 0U);
    assert(__io_putchar('!') == '!' && s_calls == 0U);
    s_ipsr = 0U;
    uart_tx_start(); /* Idempotent, preserves nine bytes. */
    drain();
    assert(s_wire_length == 9U && memcmp(s_wire, "one\ntwo\n!", 9U) == 0);
}

static void test_full_record_drop_and_wrap(void)
{
    char record[512];
    size_t before = s_wire_length;
    uint32_t dropped_lines = stats().dropped_lines;
    for (unsigned int index = 0U; index < 8U; index++)
    {
        memset(record, (int)('A' + index), sizeof(record));
        assert(uart_tx_enqueue_line(record, sizeof(record)) == 1U);
    }
    assert(stats().queued_bytes == 4096U && stats().high_water_bytes == 4096U);
    assert(uart_tx_enqueue_line("not-partial\n", 12U) == 0U);
    assert(stats().queued_bytes == 4096U && stats().dropped_lines == dropped_lines + 1U);
    uart_tx_process();
    memset(record, 'Z', 64U);
    assert(uart_tx_enqueue_line(record, 64U) == 1U);
    drain();
    for (unsigned int index = 0U; index < 4096U; index++)
    {
        assert(s_wire[before + index] == (char)('A' + index / 512U));
    }
    for (unsigned int index = 0U; index < 64U; index++)
    {
        assert(s_wire[before + 4096U + index] == 'Z');
    }
}

static void test_timeout_no_replay_and_concurrent_enqueue(void)
{
    char record[128];
    memset(record, 'X', sizeof(record));
    uart_tx_stats_t before = stats();
    assert(uart_tx_enqueue_line(record, sizeof(record)) == 1U);
    s_result = HAL_TIMEOUT;
    uart_tx_process();
    assert(stats().queued_bytes == 64U && stats().tx_errors == before.tx_errors + 1U);
    assert(stats().dropped_bytes == before.dropped_bytes + 64U);
    assert(stats().last_hal_status == HAL_TIMEOUT);
    s_result = HAL_OK;
    size_t wire_before = s_wire_length;
    s_reenter = 1U;
    uart_tx_process();
    drain();
    assert(s_wire_length == wire_before + 71U);
    assert(memcmp(s_wire + wire_before + 64U, "during\n", 7U) == 0);
    assert(stats().sent_bytes == before.sent_bytes + 71U);
    uart_tx_get_stats(NULL);
    assert(uart_tx_enqueue_line(NULL, 2U) == 0U);
    assert(uart_tx_enqueue_line("", 0U) == 0U);
    assert(uart_tx_enqueue_line(record, UART_TX_LINE_MAX + 1U) == 0U);
}

int main(void)
{
    test_startup_and_no_producer_hal_calls();
    test_full_record_drop_and_wrap();
    test_timeout_no_replay_and_concurrent_enqueue();
    puts("UART queue tests passed: bounded producers, whole-record overflow, wrap, timeout, IRQ state");
    return 0;
}
