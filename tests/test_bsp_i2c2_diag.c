#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bsp_bus.h"
#include "tx_api.h"

/* Compile the actual production unit unchanged. Its private counters are
 * visible to the IRQ-boundary observer below, which detects counter writes
 * outside a critical section without adding instrumentation to firmware. */
#ifndef BSP_DIAG_SOURCE_FILE
#define BSP_DIAG_SOURCE_FILE "../Project/Project/Air_Conditioner/Drivers/BSP/BUS/bsp_bus.c"
#endif
#include BSP_DIAG_SOURCE_FILE

I2C_TypeDef mock_i2c2;
I2C_HandleTypeDef hi2c2;
uint32_t mock_context;
static TX_THREAD thread;
static uint32_t irq_depth, mutex_owned, mutex_fail, tick=10U;
static uint32_t expected_counts[4][4], expected_recoveries;
static uint32_t critical_sections, hal_calls, reset_calls;
static uint8_t preempt_on_restore, preemption_ran;
static HAL_StatusTypeDef next_status=HAL_OK;
static uint32_t next_hal_error;

static void observe_counts(uint8_t update)
{
    uint32_t i;
    for (i=0U;i<4U;i++)
    {
        uint32_t now[4]={bsp_i2c2_diag[i].attempt_count,
            bsp_i2c2_diag[i].success_count,bsp_i2c2_diag[i].error_count,
            bsp_i2c2_diag[i].lock_timeout_count};
        if (update) { memcpy(expected_counts[i],now,sizeof(now)); }
        else { assert(memcmp(expected_counts[i],now,sizeof(now))==0); }
    }
    if (update) { expected_recoveries=bsp_i2c2_recovery_count; }
    else { assert(expected_recoveries==bsp_i2c2_recovery_count); }
}

uint32_t mock_diag_irq_save(void)
{
    uint32_t prior=irq_depth;
    if (prior==0U) { observe_counts(0U); }
    irq_depth=prior+1U;
    critical_sections++;
    return prior;
}
void mock_diag_irq_restore(uint32_t prior)
{
    assert(irq_depth==prior+1U);
    if (prior==0U) { observe_counts(1U); }
    irq_depth=prior;
    if (prior==0U && preempt_on_restore)
    {
        /* Another thread runs at the first attempt-counter restore, before
         * the first caller owns the bus. Both unknown addresses share slot 3. */
        preempt_on_restore=0U;
        assert(!mutex_owned);
        assert(bsp_i2c2_probe(0x46U)==0);
        preemption_ran=1U;
    }
}

uint32_t HAL_GetTick(void) { return tick; }
uint32_t HAL_I2C_GetState(I2C_HandleTypeDef *h) { return h->State; }
TX_THREAD *tx_thread_identify(void) { return &thread; }
UINT tx_mutex_create(TX_MUTEX *m,char *name,UINT inherit)
{ (void)name;assert(inherit==TX_INHERIT);m->created=1U;return TX_SUCCESS; }
UINT tx_mutex_get(TX_MUTEX *m,ULONG wait)
{
    assert(!irq_depth && !mutex_owned && m->created && wait==20U);
    if (mutex_fail) { tick+=20U;return TX_NOT_AVAILABLE; }
    mutex_owned=1U;return TX_SUCCESS;
}
UINT tx_mutex_put(TX_MUTEX *m)
{ (void)m;assert(!irq_depth && mutex_owned);mutex_owned=0U;return TX_SUCCESS; }
void MX_I2C2_Init(void)
{
    assert(!irq_depth && mutex_owned);
    hi2c2.Instance=I2C2;hi2c2.State=HAL_I2C_STATE_READY;
    hi2c2.Mode=HAL_I2C_MODE_NONE;hi2c2.Lock=HAL_UNLOCKED;
    hi2c2.Init.Timing=0x30C0EDFFU;mock_i2c2.CR1=I2C_CR1_PE;
}
void mock_set_bit(uint32_t *reg,uint32_t bits)
{
    assert(!irq_depth && mutex_owned);*reg|=bits;
    if (reg==&mock_i2c2.CR2 && bits==I2C_CR2_STOP)
    { mock_i2c2.ISR&=~I2C_FLAG_BUSY; }
}
void mock_i2c_disable(I2C_HandleTypeDef *h)
{ assert(!irq_depth && mutex_owned);h->Instance->CR1&=~I2C_CR1_PE; }
void mock_i2c_clear(I2C_HandleTypeDef *h,uint32_t flags)
{ assert(!irq_depth && mutex_owned);h->Instance->ISR&=~flags; }
void mock_rcc_reset(void)
{ assert(!irq_depth && mutex_owned);reset_calls++;memset(&mock_i2c2,0,sizeof(mock_i2c2)); }
GPIO_PinState HAL_GPIO_ReadPin(void *port,uint16_t pin)
{ (void)port;(void)pin;assert(!irq_depth);return GPIO_PIN_SET; }
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *h)
{
    assert(!irq_depth && mutex_owned);h->State=HAL_I2C_STATE_READY;
    h->Lock=HAL_UNLOCKED;h->ErrorCode=0U;h->Instance->CR1=I2C_CR1_PE;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_I2CEx_ConfigAnalogFilter(I2C_HandleTypeDef *h,uint32_t v)
{ assert(!irq_depth && mutex_owned);h->Instance->CR1|=v;return HAL_OK; }
HAL_StatusTypeDef HAL_I2CEx_ConfigDigitalFilter(I2C_HandleTypeDef *h,uint32_t v)
{ assert(!irq_depth && mutex_owned);h->Instance->CR1|=(v<<8);return HAL_OK; }
static HAL_StatusTypeDef transfer(I2C_HandleTypeDef *h,uint16_t address,uint32_t budget)
{
    assert(!irq_depth && mutex_owned && budget==20U && !(address&1U));
    hal_calls++;tick++;
    h->State=HAL_I2C_STATE_READY;h->Lock=HAL_UNLOCKED;
    h->ErrorCode=next_hal_error;
    return next_status;
}
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *h,uint16_t a,uint16_t r,uint16_t s,uint8_t *d,uint16_t n,uint32_t t)
{ (void)r;(void)s;memset(d,0,n);return transfer(h,a,t); }
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *h,uint16_t a,uint16_t r,uint16_t s,uint8_t *d,uint16_t n,uint32_t t)
{ (void)r;(void)s;(void)d;(void)n;return transfer(h,a,t); }
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *h,uint16_t a,uint32_t n,uint32_t t)
{ assert(n==1U);return transfer(h,a,t); }
static bsp_i2c2_diag_t snapshot(uint16_t a)
{ bsp_i2c2_diag_t d;assert(bsp_i2c2_get_diag(a,&d)==0);return d; }

int main(void)
{
    uint8_t data=0U;
    uint32_t calls;
    bsp_i2c2_diag_t d,kept;
    assert(bsp_i2c2_init()==0);
    d=snapshot(0xFFFFU);assert(d.attempt_count==1U && d.success_count==1U);

    preempt_on_restore=1U;
    assert(bsp_i2c2_read_reg8(0x44U,0x22U,&data,1U)==0 && preemption_ran);
    d=snapshot(0x46U);
    assert(d.attempt_count==3U && d.success_count==3U && d.error_count==0U);
    assert(snapshot(0x70U).attempt_count==0U);

    calls=hal_calls;
    assert(bsp_i2c2_write_reg16(0x45U,0x1234U,&data,1U)==-1);
    assert(hal_calls==calls);
    d=snapshot(0xFFFFU);
    assert(d.attempt_count==4U && d.success_count==3U && d.error_count==1U);
    assert(d.error_kind==BSP_I2C2_ERR_PARAM && d.address==0x45U &&
        d.reg==0x1234U && d.operation==2U && d.last_hal_status==UINT32_MAX);
    kept=d;
    assert(bsp_i2c2_probe(0x46U)==0);
    d=snapshot(0x44U);
    assert(d.error_kind==kept.error_kind && d.address==kept.address && d.reg==kept.reg);

    mutex_fail=1U;calls=hal_calls;
    assert(bsp_i2c2_read_reg8(0x70U,2U,&data,1U)==-1 && hal_calls==calls);
    d=snapshot(0x70U);assert(d.attempt_count==1U && d.error_count==1U &&
        d.lock_timeout_count==1U && d.error_kind==BSP_I2C2_ERR_LOCK_WAIT);
    assert(d.last_hal_error==0U && d.last_elapsed_ms==20U);
    mutex_fail=0U;
    assert(bsp_i2c2_read_reg8(0x70U,2U,&data,1U)==0);
    d=snapshot(0x70U);assert(d.attempt_count==2U && d.success_count==1U &&
        d.error_kind==BSP_I2C2_ERR_LOCK_WAIT);

    next_status=HAL_ERROR;next_hal_error=HAL_I2C_ERROR_AF;
    assert(bsp_i2c2_read_reg16(0x34U,0x3000U,&data,1U)==-1);
    d=snapshot(0x34U);assert(d.attempt_count==1U && d.error_count==1U &&
        d.error_kind==BSP_I2C2_ERR_NACK && d.last_hal_error==HAL_I2C_ERROR_AF);
    next_status=HAL_OK;next_hal_error=0U;
    assert(bsp_i2c2_read_reg16(0x34U,0x3000U,&data,1U)==0);
    assert(snapshot(0x34U).success_count==1U && snapshot(0x70U).attempt_count==2U);

    mock_i2c2.ISR=I2C_FLAG_BUSY;
    assert(bsp_i2c2_init()==0 && reset_calls==0U);
    d=snapshot(0xFFFFU);assert(d.bus_recovery_count==1U &&
        d.error_count==2U && d.success_count==5U && d.attempt_count==6U);
    /* Recovered init contributes one failure event and one successful init. */
    assert(d.error_kind==BSP_I2C2_ERR_HOST_BUSY);
    assert(snapshot(0x70U).bus_recovery_count==1U);

    /* Verify saved interrupt state is restored, including nested critical use. */
    irq_depth=1U;
    assert(bsp_i2c2_read_reg8(0x70U,2U,NULL,1U)==-1 && irq_depth==1U);
    irq_depth=0U;observe_counts(1U);
    mock_context=1U;calls=hal_calls;
    assert(bsp_i2c2_probe(0x28U)==-1 && hal_calls==calls);
    mock_context=0U;assert(snapshot(0x28U).error_kind==BSP_I2C2_ERR_CONTEXT);

    /* Counter wrap is intentional uint32_t arithmetic, not saturation. */
    bsp_i2c2_diag[1].attempt_count=UINT32_MAX;
    bsp_i2c2_diag[1].success_count=UINT32_MAX;
    observe_counts(1U);
    assert(bsp_i2c2_probe(0x28U)==0);
    d=snapshot(0x28U);assert(d.attempt_count==0U && d.success_count==0U);
    bsp_i2c2_diag[1].error_count=UINT32_MAX;
    bsp_i2c2_diag[1].lock_timeout_count=UINT32_MAX;
    observe_counts(1U);mutex_fail=1U;
    assert(bsp_i2c2_probe(0x28U)==-1);
    d=snapshot(0x28U);assert(d.error_count==0U && d.lock_timeout_count==0U);
    assert(!irq_depth && !mutex_owned && critical_sections>30U);
    assert(bsp_i2c2_get_diag(0x70U,NULL)==-1);
    observe_counts(0U);
    puts("PASS: I2C2 counters protected at IRQ boundaries; shared-slot preemption, retained failures, NACK/lock/context/recovery, uint32 wrap, no masked HAL or waits.");
    return 0;
}
