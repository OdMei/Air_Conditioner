/**
 ****************************************************************************************************
 * @file        bsp_bus.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       BUS驱动文件
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 * 
 * 实验平台:正点原子 N647开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 * 
 ****************************************************************************************************
 */

#include "bsp_bus.h"
#include "main.h"
#include "tx_api.h"
#include "tx_thread.h"

extern I2C_HandleTypeDef hi2c2;

static TX_MUTEX bsp_i2c2_mutex;
static uint8_t bsp_i2c2_mutex_created;
static uint8_t bsp_i2c2_initialized;
static bsp_i2c2_diag_t bsp_i2c2_diag[4];
static uint32_t bsp_i2c2_recovery_count;
static uint32_t bsp_i2c2_last_reset_ms;
static uint8_t bsp_i2c2_reset_attempted;

#define BSP_I2C2_STOP_WAIT_MS     (2U)
#define BSP_I2C2_STOP_MAX_POLLS   (4096U)
#define BSP_I2C2_RESET_PERIOD_MS  (1000U)

static bsp_i2c2_diag_t *bsp_i2c2_diag_slot(uint16_t address)
{
    if (address == 0x70U) { return &bsp_i2c2_diag[0]; }
    if (address == 0x28U) { return &bsp_i2c2_diag[1]; }
    if (address == 0x34U) { return &bsp_i2c2_diag[2]; }
    return &bsp_i2c2_diag[3];
}

/* Attempts are recorded before acquiring the bus mutex. Different clients can
 * also share the fallback diagnostic slot, so serialize the read/modify/write
 * independently of bus ownership. Never wait or access I2C in this section. */
static void bsp_i2c2_increment_counter(uint32_t *counter)
{
    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE
    (*counter)++;
    TX_RESTORE
}

static uint8_t bsp_i2c2_host_bad(void)
{
    return ((__HAL_I2C_GET_FLAG(&hi2c2, I2C_FLAG_BUSY) != RESET) ||
            (HAL_I2C_GetState(&hi2c2) != HAL_I2C_STATE_READY) ||
            (hi2c2.Lock != HAL_UNLOCKED)) ? 1U : 0U;
}

static void bsp_i2c2_save_failure(bsp_i2c2_diag_t *diag, uint16_t address,
                                 uint16_t reg, uint16_t length, uint8_t operation,
                                 uint8_t kind, uint32_t status, uint32_t started,
                                 uint8_t owns_bus)
{
    TX_INTERRUPT_SAVE_AREA
    TX_DISABLE
    diag->error_kind = kind;
    diag->address = address;
    diag->reg = reg;
    diag->length = length;
    diag->operation = operation;
    diag->last_hal_status = status;
    /* A mutex failure has no HAL transaction. Do not attribute the current
       camera's registers/error to a touch request that never acquired it. */
    diag->last_hal_error = owns_bus ? hi2c2.ErrorCode : 0U;
    diag->last_hal_state = owns_bus ? (uint32_t)hi2c2.State : 0U;
    diag->last_isr = owns_bus ? hi2c2.Instance->ISR : 0U;
    diag->last_cr2 = owns_bus ? hi2c2.Instance->CR2 : 0U;
    diag->last_failure_ms = HAL_GetTick();
    diag->last_elapsed_ms = diag->last_failure_ms - started;
    diag->error_count++;
    if (kind == BSP_I2C2_ERR_LOCK_WAIT) { diag->lock_timeout_count++; }
    TX_RESTORE
}

static uint8_t bsp_i2c2_hal_error_kind(HAL_StatusTypeDef status)
{
    if ((hi2c2.ErrorCode & HAL_I2C_ERROR_TIMEOUT) != 0U) { return BSP_I2C2_ERR_TIMEOUT; }
    if ((hi2c2.ErrorCode & HAL_I2C_ERROR_AF) != 0U) { return BSP_I2C2_ERR_NACK; }
    if (status == HAL_BUSY) { return BSP_I2C2_ERR_HAL_BUSY; }
    return BSP_I2C2_ERR_HAL;
}

static void bsp_i2c2_clear_transfer(void)
{
    __HAL_I2C_CLEAR_FLAG(&hi2c2, I2C_FLAG_STOPF | I2C_FLAG_AF |
                         I2C_FLAG_BERR | I2C_FLAG_ARLO | I2C_FLAG_OVR);
    I2C_RESET_CR2(&hi2c2);
    hi2c2.State = HAL_I2C_STATE_READY;
    hi2c2.Mode = HAL_I2C_MODE_NONE;
    hi2c2.Lock = HAL_UNLOCKED;
}

/* Called only with the shared mutex held, after saving the original failure.
 * No command is replayed and no camera/touch sensor reset occurs here. */
static uint8_t bsp_i2c2_recover_host(bsp_i2c2_diag_t *diag)
{
    uint32_t started;
    uint32_t attempt;
    uint32_t saved_cr1;
    HAL_StatusTypeDef status;

    if (bsp_i2c2_host_bad() == 0U) { return 1U; }
    if (__HAL_I2C_GET_FLAG(&hi2c2, I2C_FLAG_BUSY) != RESET)
    {
        SET_BIT(hi2c2.Instance->CR2, I2C_CR2_STOP);
        started = HAL_GetTick();
        for (attempt = 0U; attempt < BSP_I2C2_STOP_MAX_POLLS; attempt++)
        {
            if (__HAL_I2C_GET_FLAG(&hi2c2, I2C_FLAG_BUSY) == RESET)
            {
                if ((HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_14) == GPIO_PIN_SET) &&
                    (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_4) == GPIO_PIN_SET))
                {
                    bsp_i2c2_clear_transfer();
                    bsp_i2c2_increment_counter(&bsp_i2c2_recovery_count);
                    return 1U;
                }
                break;
            }
            if ((uint32_t)(HAL_GetTick() - started) >= BSP_I2C2_STOP_WAIT_MS) { break; }
            __NOP();
        }
    }

    started = HAL_GetTick();
    if ((bsp_i2c2_reset_attempted != 0U) &&
        ((uint32_t)(started - bsp_i2c2_last_reset_ms) < BSP_I2C2_RESET_PERIOD_MS))
    {
        diag->error_kind = BSP_I2C2_ERR_RECOVERY;
        return 0U;
    }
    bsp_i2c2_reset_attempted = 1U;
    bsp_i2c2_last_reset_ms = started;
    saved_cr1 = hi2c2.Instance->CR1;
    __HAL_I2C_DISABLE(&hi2c2);
    /* RM0486 63.4.6: write PE=0, read PE=0, then enable; PE reset releases
       SCL/SDA including controller RXDR clock stretching. Config is retained. */
    if ((hi2c2.Instance->CR1 & I2C_CR1_PE) != 0U)
    {
        diag->error_kind = BSP_I2C2_ERR_RECOVERY;
        return 0U;
    }
    for (attempt = 0U; attempt < 256U; attempt++) { __NOP(); }
    if ((HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_14) != GPIO_PIN_SET) ||
        (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_4) != GPIO_PIN_SET))
    {
        /* An external device is still holding a line after the host released
           both outputs. Restore PE/config, leave the original failure visible. */
        hi2c2.Instance->CR1 = saved_cr1;
        diag->error_kind = BSP_I2C2_ERR_LINE_LOW;
        return 0U;
    }

    /* Reset this peripheral only; do not HAL_DeInit/MSP-deinit its shared pins.
       Keep State non-RESET so HAL_Init preserves AF/security/MSP configuration. */
    __HAL_RCC_I2C2_FORCE_RESET();
    __HAL_RCC_I2C2_RELEASE_RESET();
    hi2c2.State = HAL_I2C_STATE_READY;
    hi2c2.Mode = HAL_I2C_MODE_NONE;
    hi2c2.Lock = HAL_UNLOCKED;
    status = HAL_I2C_Init(&hi2c2);
    if (status == HAL_OK)
    {
        status = HAL_I2CEx_ConfigAnalogFilter(&hi2c2, saved_cr1 & I2C_CR1_ANFOFF);
    }
    if (status == HAL_OK)
    {
        status = HAL_I2CEx_ConfigDigitalFilter(&hi2c2,
                    (saved_cr1 & I2C_CR1_DNF) >> 8U);
    }
    if ((status != HAL_OK) || (bsp_i2c2_host_bad() != 0U))
    {
        diag->error_kind = BSP_I2C2_ERR_RECOVERY;
        return 0U;
    }
    bsp_i2c2_clear_transfer();
    bsp_i2c2_initialized = 1U;
    bsp_i2c2_increment_counter(&bsp_i2c2_recovery_count);
    return 1U;
}

/* Round the millisecond budget up using the configured ThreadX tick rate. */
#define BSP_I2C2_MUTEX_WAIT_TICKS \
    ((ULONG)((BSP_I2C2_TIMEOUT_MS * TX_TIMER_TICKS_PER_SECOND + 999UL) / 1000UL))

static int32_t bsp_i2c2_acquire(void)
{
    UINT status = TX_SUCCESS;
    TX_INTERRUPT_SAVE_AREA

    if ((TX_THREAD_GET_SYSTEM_STATE() != 0UL) ||
        (tx_thread_identify() == TX_NULL))
    {
        return -(int32_t)BSP_I2C2_ERR_CONTEXT;
    }
    /* app_run initializes before starting clients. This short critical section
     * also prevents concurrent clients from creating the mutex twice. */
    TX_DISABLE
    if (bsp_i2c2_mutex_created == 0U)
    {
        status = tx_mutex_create(&bsp_i2c2_mutex, "Shared I2C2", TX_INHERIT);
        if (status == TX_SUCCESS)
        {
            bsp_i2c2_mutex_created = 1U;
        }
    }
    TX_RESTORE
    if ((status != TX_SUCCESS) ||
        (tx_mutex_get(&bsp_i2c2_mutex, BSP_I2C2_MUTEX_WAIT_TICKS) != TX_SUCCESS))
    {
        return -(int32_t)BSP_I2C2_ERR_LOCK_WAIT;
    }
    if (bsp_i2c2_initialized == 0U)
    {
        /* Reuse the global handle. MX alone configures PD4/PD14 as AF4; keep
         * the application pin security attributes and all camera state. */
        if ((hi2c2.Instance != I2C2) ||
            (HAL_I2C_GetState(&hi2c2) == HAL_I2C_STATE_RESET))
        {
            MX_I2C2_Init();
        }
        if (HAL_I2C_GetState(&hi2c2) == HAL_I2C_STATE_READY) { bsp_i2c2_initialized = 1U; }
    }
    return 0;
}

int32_t bsp_i2c2_init(void)
{
    uint32_t started = HAL_GetTick();
    bsp_i2c2_diag_t *diag = bsp_i2c2_diag_slot(0xFFFFU);
    int32_t acquired;
    uint8_t ready = 1U;
    bsp_i2c2_increment_counter(&diag->attempt_count);
    acquired = bsp_i2c2_acquire();
    if (acquired != 0)
    {
        bsp_i2c2_save_failure(diag, 0xFFFFU, 0U, 0U, 0U,
                              (uint8_t)-acquired, UINT32_MAX, started, 0U);
        return -1;
    }
    if (bsp_i2c2_host_bad() != 0U)
    {
        bsp_i2c2_save_failure(diag, 0xFFFFU, 0U, 0U, 0U,
                              BSP_I2C2_ERR_HOST_BUSY, HAL_BUSY, started, 1U);
        ready = bsp_i2c2_recover_host(diag);
    }
    if (ready != 0U) { bsp_i2c2_increment_counter(&diag->success_count); }
    tx_mutex_put(&bsp_i2c2_mutex);
    return ready ? 0 : -1;
}

int32_t bsp_i2c2_deinit(void)
{
    /* Logical detach only. A failed camera probe or touch transfer must not
     * stop the other client. Hardware and mutex live until application reset. */
    return 0;
}

static int32_t bsp_i2c2_transfer(uint16_t address, uint16_t reg,
                               uint16_t reg_size, uint8_t *data,
                               uint16_t length, uint8_t read)
{
    HAL_StatusTypeDef status;
    uint32_t started = HAL_GetTick();
    bsp_i2c2_diag_t *diag = bsp_i2c2_diag_slot(address);
    int32_t acquired;
    uint8_t operation = read ? 1U : 2U;
    bsp_i2c2_increment_counter(&diag->attempt_count);
    if ((data == NULL) || (length == 0U) || (address > 0xFEU) ||
        ((address & 1U) != 0U))
    {
        bsp_i2c2_save_failure(diag, address, reg, length, operation,
                              BSP_I2C2_ERR_PARAM, UINT32_MAX, started, 0U);
        return -1;
    }
    acquired = bsp_i2c2_acquire();
    if (acquired != 0)
    {
        bsp_i2c2_save_failure(diag, address, reg, length, operation,
                              (uint8_t)-acquired, UINT32_MAX, started, 0U);
        return -1;
    }
    if (bsp_i2c2_host_bad() != 0U)
    {
        bsp_i2c2_save_failure(diag, address, reg, length, operation,
                              BSP_I2C2_ERR_HOST_BUSY, HAL_BUSY, started, 1U);
        (void)bsp_i2c2_recover_host(diag);
        tx_mutex_put(&bsp_i2c2_mutex);
        return -1;
    }

    if (read != 0U)
    {
        status = HAL_I2C_Mem_Read(&hi2c2, address, reg, reg_size,
                                  data, length, BSP_I2C2_TIMEOUT_MS);
    }
    else
    {
        status = HAL_I2C_Mem_Write(&hi2c2, address, reg, reg_size,
                                   data, length, BSP_I2C2_TIMEOUT_MS);
    }
    if (status != HAL_OK)
    {
        bsp_i2c2_save_failure(diag, address, reg, length, operation,
                              bsp_i2c2_hal_error_kind(status), status, started, 1U);
        (void)bsp_i2c2_recover_host(diag);
    }
    else { bsp_i2c2_increment_counter(&diag->success_count); }
    tx_mutex_put(&bsp_i2c2_mutex);
    return (status == HAL_OK) ? 0 : -1;
}

int32_t bsp_i2c2_write_reg8(uint16_t address, uint8_t reg, uint8_t *data, uint16_t length)
{
    return bsp_i2c2_transfer(address, reg, I2C_MEMADD_SIZE_8BIT, data, length, 0U);
}

int32_t bsp_i2c2_read_reg8(uint16_t address, uint8_t reg, uint8_t *data, uint16_t length)
{
    return bsp_i2c2_transfer(address, reg, I2C_MEMADD_SIZE_8BIT, data, length, 1U);
}

int32_t bsp_i2c2_write_reg16(uint16_t address, uint16_t reg, uint8_t *data, uint16_t length)
{
    return bsp_i2c2_transfer(address, reg, I2C_MEMADD_SIZE_16BIT, data, length, 0U);
}

int32_t bsp_i2c2_read_reg16(uint16_t address, uint16_t reg, uint8_t *data, uint16_t length)
{
    return bsp_i2c2_transfer(address, reg, I2C_MEMADD_SIZE_16BIT, data, length, 1U);
}

int32_t bsp_i2c2_probe(uint16_t address)
{
    HAL_StatusTypeDef status;
    uint32_t started = HAL_GetTick();
    bsp_i2c2_diag_t *diag = bsp_i2c2_diag_slot(address);
    int32_t acquired;
    bsp_i2c2_increment_counter(&diag->attempt_count);
    if ((address > 0xFEU) || ((address & 1U) != 0U))
    {
        bsp_i2c2_save_failure(diag, address, 0U, 0U, 3U,
                              BSP_I2C2_ERR_PARAM, UINT32_MAX, started, 0U);
        return -1;
    }
    acquired = bsp_i2c2_acquire();
    if (acquired != 0)
    {
        bsp_i2c2_save_failure(diag, address, 0U, 0U, 3U,
                              (uint8_t)-acquired, UINT32_MAX, started, 0U);
        return -1;
    }
    if (bsp_i2c2_host_bad() != 0U)
    {
        bsp_i2c2_save_failure(diag, address, 0U, 0U, 3U,
                              BSP_I2C2_ERR_HOST_BUSY, HAL_BUSY, started, 1U);
        (void)bsp_i2c2_recover_host(diag);
        tx_mutex_put(&bsp_i2c2_mutex);
        return -1;
    }

    status = HAL_I2C_IsDeviceReady(&hi2c2, address, 1U, BSP_I2C2_TIMEOUT_MS);
    if (status != HAL_OK)
    {
        bsp_i2c2_save_failure(diag, address, 0U, 0U, 3U,
                              bsp_i2c2_hal_error_kind(status), status, started, 1U);
        (void)bsp_i2c2_recover_host(diag);
    }
    else { bsp_i2c2_increment_counter(&diag->success_count); }
    tx_mutex_put(&bsp_i2c2_mutex);
    return (status == HAL_OK) ? 0 : -1;
}

int32_t bsp_i2c2_get_diag(uint16_t address, bsp_i2c2_diag_t *diag)
{
    TX_INTERRUPT_SAVE_AREA
    if (diag == NULL) { return -1; }
    TX_DISABLE
    *diag = *bsp_i2c2_diag_slot(address);
    diag->bus_recovery_count = bsp_i2c2_recovery_count;
    TX_RESTORE
    return 0;
}

int32_t bsp_bus_get_tick(void)
{
    return (int32_t)HAL_GetTick();
}
