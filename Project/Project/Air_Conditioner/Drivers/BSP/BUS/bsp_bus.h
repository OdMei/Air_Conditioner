/**
 ****************************************************************************************************
 * @file        bsp_bus.h
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

#ifndef __BSP_BUS_H
#define __BSP_BUS_H

#include "stm32n6xx_hal.h"

/* Addresses use the HAL convention: the 7-bit address shifted left once.
 * Blocking APIs are thread-only; I2C2 stays alive for both clients. */
#define BSP_I2C2_TIMEOUT_MS (20U)

typedef enum {
    BSP_I2C2_ERR_NONE = 0,
    BSP_I2C2_ERR_LOCK_WAIT = 1,
    BSP_I2C2_ERR_TIMEOUT = 2,
    BSP_I2C2_ERR_NACK = 3,
    BSP_I2C2_ERR_HOST_BUSY = 4,
    BSP_I2C2_ERR_HAL_BUSY = 5,
    BSP_I2C2_ERR_LINE_LOW = 6,
    BSP_I2C2_ERR_RECOVERY = 7,
    BSP_I2C2_ERR_CONTEXT = 8,
    BSP_I2C2_ERR_PARAM = 9,
    BSP_I2C2_ERR_HAL = 10
} bsp_i2c2_error_t;

/* Addresses 0x70, 0x28 and 0x34 have dedicated diagnostic slots; all other
 * addresses, including init (0xFFFF), share one fallback slot. The last failure
 * keeps its originating address and is retained even after another success.
 * operation: 0 init, 1 read, 2 write, 3 probe. HAL status UINT32_MAX means
 * HAL was not called (for example a mutex timeout). No bus traffic in getter.
 * Counters wrap as uint32_t. Attempts include pending/invalid requests; errors
 * count failure events. A recovered init records both an error and a success,
 * so attempt_count need not equal success_count + error_count. */
typedef struct {
    uint8_t error_kind;
    uint16_t address, reg, length;
    uint8_t operation;
    uint32_t last_hal_status, last_hal_error, last_hal_state;
    uint32_t last_isr, last_cr2, last_elapsed_ms, last_failure_ms;
    uint32_t attempt_count, success_count, error_count, lock_timeout_count;
    uint32_t bus_recovery_count;
} bsp_i2c2_diag_t;

int32_t bsp_i2c2_init(void);
int32_t bsp_i2c2_deinit(void);
int32_t bsp_i2c2_write_reg8(uint16_t address, uint8_t reg, uint8_t *data, uint16_t length);
int32_t bsp_i2c2_read_reg8(uint16_t address, uint8_t reg, uint8_t *data, uint16_t length);
int32_t bsp_i2c2_write_reg16(uint16_t address, uint16_t reg, uint8_t *data, uint16_t length);
int32_t bsp_i2c2_read_reg16(uint16_t address, uint16_t reg, uint8_t *data, uint16_t length);
int32_t bsp_i2c2_probe(uint16_t address);
int32_t bsp_i2c2_get_diag(uint16_t address, bsp_i2c2_diag_t *diag);
int32_t bsp_bus_get_tick(void);

#endif
