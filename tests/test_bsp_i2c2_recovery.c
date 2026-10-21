#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bsp_bus.h"
#include "tx_api.h"

I2C_TypeDef mock_i2c2;
I2C_HandleTypeDef hi2c2;
uint32_t mock_irq_depth, mock_context;
static TX_THREAD mock_thread;
static uint32_t tick, tick_reads, mutex_owned, mutex_fail;
static uint32_t mx_calls, reset_calls, disable_calls, stop_calls, mem_calls;
static uint32_t stop_releases, external_low, self_stretches, freeze_tick;
static uint32_t fail_mode;
enum { TRANSFER_OK, TRANSFER_TIMEOUT, TRANSFER_NACK };

uint32_t HAL_GetTick(void) { tick_reads++; return tick; }
uint32_t HAL_I2C_GetState(I2C_HandleTypeDef *h) { return h->State; }
TX_THREAD *tx_thread_identify(void) { return &mock_thread; }
UINT tx_mutex_create(TX_MUTEX *m,char *name,UINT inherit)
{ (void)name; assert(inherit==TX_INHERIT); m->created=1U; return TX_SUCCESS; }
UINT tx_mutex_get(TX_MUTEX *m,ULONG wait)
{
    assert(m->created && wait==20U && !mutex_owned);
    if (mutex_fail) { tick+=20U; return TX_NOT_AVAILABLE; }
    mutex_owned=1U; return TX_SUCCESS;
}
UINT tx_mutex_put(TX_MUTEX *m)
{ (void)m; assert(mutex_owned); mutex_owned=0U; return TX_SUCCESS; }

void MX_I2C2_Init(void)
{
    assert(mutex_owned); mx_calls++;
    hi2c2.Instance=I2C2; hi2c2.Init.Timing=0x30C0EDFFU;
    hi2c2.State=HAL_I2C_STATE_READY; hi2c2.Lock=HAL_UNLOCKED;
    mock_i2c2.TIMINGR=hi2c2.Init.Timing;
    mock_i2c2.CR1=I2C_CR1_PE|I2C_CR1_ANFOFF|(3U<<8);
}

void mock_set_bit(uint32_t *reg,uint32_t bits)
{
    assert(mutex_owned); *reg|=bits;
    if (reg==&mock_i2c2.CR2 && bits==I2C_CR2_STOP)
    {
        stop_calls++;
        if (stop_releases) { mock_i2c2.ISR&=~I2C_FLAG_BUSY; mock_i2c2.ISR|=I2C_FLAG_STOPF; }
    }
}
void mock_i2c_disable(I2C_HandleTypeDef *h)
{
    assert(mutex_owned); disable_calls++;
    h->Instance->CR1&=~I2C_CR1_PE;
    /* PE=0 resets controller state/flags, independently of an external low line. */
    h->Instance->ISR=0U; h->Instance->CR2&=~I2C_CR2_STOP;
}
void mock_i2c_clear(I2C_HandleTypeDef *h,uint32_t flags)
{ assert(mutex_owned); h->Instance->ICR=flags; h->Instance->ISR&=~flags; }
void mock_rcc_reset(void)
{ assert(mutex_owned && !(mock_i2c2.CR1&I2C_CR1_PE)); reset_calls++; memset(&mock_i2c2,0,sizeof(mock_i2c2)); }
GPIO_PinState HAL_GPIO_ReadPin(void *port,uint16_t pin)
{
    (void)port;
    if (external_low && pin==GPIO_PIN_4) { return GPIO_PIN_RESET; }
    if (self_stretches && pin==GPIO_PIN_14 && (mock_i2c2.CR1&I2C_CR1_PE)) { return GPIO_PIN_RESET; }
    return GPIO_PIN_SET;
}
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *h)
{
    assert(mutex_owned && h->State!=HAL_I2C_STATE_RESET);
    h->Instance->TIMINGR=h->Init.Timing; h->Instance->CR1=I2C_CR1_PE;
    h->State=HAL_I2C_STATE_READY; h->Mode=HAL_I2C_MODE_NONE;
    h->Lock=HAL_UNLOCKED; h->ErrorCode=HAL_I2C_ERROR_NONE;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_I2CEx_ConfigAnalogFilter(I2C_HandleTypeDef *h,uint32_t filter)
{ assert(mutex_owned); h->Instance->CR1=(h->Instance->CR1&~I2C_CR1_ANFOFF)|filter; return HAL_OK; }
HAL_StatusTypeDef HAL_I2CEx_ConfigDigitalFilter(I2C_HandleTypeDef *h,uint32_t filter)
{ assert(mutex_owned); h->Instance->CR1=(h->Instance->CR1&~I2C_CR1_DNF)|(filter<<8); return HAL_OK; }
static HAL_StatusTypeDef transfer(I2C_HandleTypeDef *h,uint32_t timeout)
{
    assert(mutex_owned && timeout==20U); mem_calls++;
    h->State=HAL_I2C_STATE_READY; h->Lock=HAL_UNLOCKED;
    if (fail_mode==TRANSFER_TIMEOUT)
    {
        if (!freeze_tick) { tick+=21U; }
        h->ErrorCode=HAL_I2C_ERROR_TIMEOUT; h->Instance->ISR=I2C_FLAG_BUSY;
        h->Instance->CR2=0x1234U; return HAL_ERROR;
    }
    if (!freeze_tick) { tick++; }
    h->Instance->ISR=0U; h->Instance->CR2=0U;
    h->ErrorCode=fail_mode==TRANSFER_NACK ? HAL_I2C_ERROR_AF : 0U;
    return fail_mode==TRANSFER_NACK ? HAL_ERROR : HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *h,uint16_t a,uint16_t r,uint16_t s,uint8_t *d,uint16_t l,uint32_t t)
{ (void)a;(void)r;(void)s;memset(d,0,l);return transfer(h,t); }
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *h,uint16_t a,uint16_t r,uint16_t s,uint8_t *d,uint16_t l,uint32_t t)
{ (void)a;(void)r;(void)s;(void)d;(void)l;return transfer(h,t); }
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *h,uint16_t a,uint32_t n,uint32_t t)
{ (void)a;assert(n==1U);return transfer(h,t); }
static bsp_i2c2_diag_t diag(uint16_t address)
{ bsp_i2c2_diag_t d;assert(bsp_i2c2_get_diag(address,&d)==0);return d; }
static int32_t read_touch(void)
{ uint8_t value;int32_t result=bsp_i2c2_read_reg8(0x70U,2U,&value,1U);assert(!mutex_owned);return result; }
static void idle_at(uint32_t now)
{
    tick=now; mock_i2c2.ISR=0U; hi2c2.State=HAL_I2C_STATE_READY;hi2c2.Lock=HAL_UNLOCKED;
    external_low=self_stretches=freeze_tick=0U;stop_releases=0U;
}
int main(void)
{
    uint8_t value=1U;
    uint32_t calls, resets, disables, loops, recoveries;
    bsp_i2c2_diag_t touch;
    tick=10U; assert(bsp_i2c2_init()==0);assert(mx_calls==1U);
    assert(read_touch()==0);assert(reset_calls==0U && stop_calls==0U);

    idle_at(1000U);fail_mode=TRANSFER_TIMEOUT;stop_releases=1U;calls=mem_calls;
    assert(read_touch()==-1 && mem_calls==calls+1U); /* Never replay a failed transaction. */
    touch=diag(0x70U);
    assert(touch.error_kind==BSP_I2C2_ERR_TIMEOUT && touch.last_hal_status==HAL_ERROR);
    assert(touch.last_hal_error==HAL_I2C_ERROR_TIMEOUT && touch.last_hal_state==HAL_I2C_STATE_READY);
    assert(touch.last_isr==I2C_FLAG_BUSY && touch.last_cr2==0x1234U && touch.last_elapsed_ms==21U);
    assert(stop_calls==1U && reset_calls==0U && !(mock_i2c2.ISR&I2C_FLAG_BUSY));
    assert(touch.bus_recovery_count==1U);
    fail_mode=TRANSFER_OK;assert(read_touch()==0);
    assert(diag(0x70U).last_cr2==0x1234U); /* A success does not erase first-failure evidence. */

    idle_at(2000U);fail_mode=TRANSFER_TIMEOUT;self_stretches=1U;
    assert(read_touch()==-1);assert(reset_calls==1U && disable_calls==1U);
    assert((mock_i2c2.CR1&(I2C_CR1_PE|I2C_CR1_ANFOFF|I2C_CR1_DNF))==(I2C_CR1_PE|I2C_CR1_ANFOFF|(3U<<8)));
    assert(mock_i2c2.TIMINGR==0x30C0EDFFU && mx_calls==1U);
    touch=diag(0x70U);assert(touch.last_isr==I2C_FLAG_BUSY && touch.last_hal_error==HAL_I2C_ERROR_TIMEOUT);
    fail_mode=TRANSFER_NACK;idle_at(2200U);
    assert(bsp_i2c2_read_reg16(0x34U,0x3000U,&value,1U)==-1);
    assert(diag(0x34U).error_kind==BSP_I2C2_ERR_NACK);
    assert(diag(0x70U).last_failure_ms==touch.last_failure_ms && diag(0x70U).error_kind==touch.error_kind);
    assert(reset_calls==1U);

    idle_at(4000U);fail_mode=TRANSFER_TIMEOUT;external_low=1U;resets=reset_calls;
    recoveries=diag(0x70U).bus_recovery_count;
    assert(read_touch()==-1 && reset_calls==resets);
    assert(diag(0x70U).error_kind==BSP_I2C2_ERR_LINE_LOW);
    assert(diag(0x70U).bus_recovery_count==recoveries && (mock_i2c2.CR1&I2C_CR1_PE));
    idle_at(4500U);fail_mode=TRANSFER_TIMEOUT;disables=disable_calls;
    assert(read_touch()==-1 && disable_calls==disables && reset_calls==resets);
    assert(diag(0x70U).error_kind==BSP_I2C2_ERR_RECOVERY);

    idle_at(6000U);mock_i2c2.ISR=I2C_FLAG_BUSY;calls=mem_calls;
    assert(bsp_i2c2_init()==0 && reset_calls==resets+1U && mem_calls==calls && mx_calls==1U);
    assert(diag(0xFFFFU).error_kind==BSP_I2C2_ERR_HOST_BUSY);
    fail_mode=TRANSFER_OK;assert(read_touch()==0);

    idle_at(8000U);mutex_fail=1U;calls=mem_calls;
    assert(read_touch()==-1 && mem_calls==calls);
    assert(diag(0x70U).error_kind==BSP_I2C2_ERR_LOCK_WAIT && diag(0x70U).last_hal_status==UINT32_MAX);
    assert(diag(0x70U).last_isr==0U && diag(0x70U).last_elapsed_ms==20U);
    mutex_fail=0U;assert(read_touch()==0);
    mock_context=1U;assert(read_touch()==-1);mock_context=0U;
    assert(diag(0x70U).error_kind==BSP_I2C2_ERR_CONTEXT);

    idle_at(10000U);fail_mode=TRANSFER_TIMEOUT;freeze_tick=1U;loops=tick_reads;
    assert(read_touch()==-1); /* Fixed poll limit still terminates with a stopped HAL tick. */
    assert(tick_reads-loops<5000U && !mutex_owned);
    idle_at(UINT32_MAX-100U);mock_i2c2.ISR=I2C_FLAG_BUSY;
    assert(bsp_i2c2_init()==0);resets=reset_calls;
    idle_at(499U);mock_i2c2.ISR=I2C_FLAG_BUSY;
    assert(bsp_i2c2_init()==-1 && reset_calls==resets);
    idle_at(1100U);mock_i2c2.ISR=I2C_FLAG_BUSY;
    assert(bsp_i2c2_init()==0 && reset_calls==resets+1U);
    assert(!mutex_owned && !mock_irq_depth && mx_calls==1U);
    puts("PASS: I2C2 original error capture, STOP cleanup, host stretching, external held-low, reset rate/tick wrap, mutex ownership and no replay.");
    return 0;
}
