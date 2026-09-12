/**
 ****************************************************************************************************
 * @file        rtc.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-13
 * @brief       RTC驱动代码
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

#include "rtc.h"

extern RTC_HandleTypeDef hrtc;  /* RTC句柄 */

/**
 * @brief   读取后备寄存器
 * @param   bkrx: 后备寄存器编号
 * @retval  后备寄存器值
 */
uint32_t rtc_read_bkr(uint32_t bkrx)
{
    return HAL_RTCEx_BKUPRead(&hrtc, bkrx);
}

/**
 * @brief   写后背寄存器
 * @param   bkrx: 后备寄存器编号
 * @param   data: 后备寄存器值
 * @retval  无
 */
void rtc_write_bkr(uint32_t bkrx, uint32_t data)
{
    HAL_RTCEx_BKUPWrite(&hrtc, bkrx, data);
}

/**
 * @brief   初始化RTC
 * @param   无
 * @retval  初始化结果
 * @arg     0: 初始化成功
 * @arg     1: 初始化失败
 */
uint8_t rtc_init(void)
{
    uint32_t flag;

    /* 从后备寄存器读取RTC初始化标志 */
    flag = rtc_read_bkr(0);

    /* RTC第一次初始化 */
    if ((flag != 0x5050) && (flag != 0x5051))
    {
        /* 设置RTC时间和日期信息 */
        rtc_set_time(8, 0, 0, 0);
        rtc_set_date(25, 1, 13, 1);

        /* 写入RTC初始化标志 */
        switch (LL_RCC_GetRTCClockSource())
        {
            case LL_RCC_RTC_CLKSOURCE_LSI:
            {
                rtc_write_bkr(0, 0x5051);
                break;
            }
            case LL_RCC_RTC_CLKSOURCE_LSE:
            {
                rtc_write_bkr(0, 0x5050);
                break;
            }
        }
    }

    return 0;
}

/**
 * @brief   设置RTC时间信息
 * @param   hour: 时
 * @param   minute: 分
 * @param   second: 秒
 * @param   ampm: 上下午
 * @arg     0: 上午
 * @arg     1: 下午
 * @retval  设置结果
 * @arg     0: 设置成功
 * @arg     1: 设置失败
 */
uint8_t rtc_set_time(uint8_t hour, uint8_t minute, uint8_t second, uint8_t ampm)
{
    RTC_TimeTypeDef rtc_time_struct = {0};

    rtc_time_struct.Hours = hour;
    rtc_time_struct.Minutes = minute;
    rtc_time_struct.Seconds = second;
    rtc_time_struct.TimeFormat = ampm;
    rtc_time_struct.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    rtc_time_struct.StoreOperation = RTC_STOREOPERATION_RESET;
    if (HAL_RTC_SetTime(&hrtc, &rtc_time_struct, RTC_FORMAT_BIN) != HAL_OK)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief   设置RTC日期信息
 * @param   year: 年
 * @param   month: 月
 * @param   date: 日
 * @param   week: 星期
 * @retval  设置结果
 * @arg     0: 设置成功
 * @arg     1: 设置失败
 */
uint8_t rtc_set_date(uint8_t year, uint8_t month, uint8_t date, uint8_t week)
{
    RTC_DateTypeDef rtc_date_struct = {0};

    rtc_date_struct.WeekDay = week;
    rtc_date_struct.Month = month;
    rtc_date_struct.Date = date;
    rtc_date_struct.Year = year;
    if (HAL_RTC_SetDate(&hrtc, &rtc_date_struct, RTC_FORMAT_BIN) != HAL_OK)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief   获取RTC时间信息
 * @param   hour: 时
 * @param   minute: 分
 * @param   second: 秒
 * @param   ampm: 上下午
 * @arg     0: 上午
 * @arg     1: 下午
 * @retval  无
 */
void rtc_get_time(uint8_t *hour, uint8_t *minute, uint8_t *second, uint8_t *ampm)
{
    RTC_TimeTypeDef rtc_time_struct = {0};

    HAL_RTC_GetTime(&hrtc, &rtc_time_struct, RTC_FORMAT_BIN);

    *hour = rtc_time_struct.Hours;
    *minute = rtc_time_struct.Minutes;
    *second = rtc_time_struct.Seconds;
    *ampm = rtc_time_struct.TimeFormat;
}

/**
 * @brief   获取RTC日期信息
 * @param   year: 年
 * @param   month: 月
 * @param   date: 日
 * @param   week: 星期
 * @retval  无
 */
void rtc_get_date(uint8_t *year, uint8_t *month, uint8_t *date, uint8_t *week)
{
    RTC_DateTypeDef rtc_date_struct = {0};

    HAL_RTC_GetDate(&hrtc, &rtc_date_struct, RTC_FORMAT_BIN);

    *year = rtc_date_struct.Year;
    *month = rtc_date_struct.Month;
    *date = rtc_date_struct.Date;
    *week = rtc_date_struct.WeekDay;
}

/**
 * @brief   设置RTC闹钟时间信息
 * @param   week: 星期
 * @param   hour: 时
 * @param   minute: 分
 * @param   second: 秒
 * @retval  无
 */
void rtc_set_alarm(uint8_t week, uint8_t hour, uint8_t minute, uint8_t second)
{
    RTC_AlarmTypeDef rtc_alarm_struct = {0};

    /* 设置闹钟中断 */
    rtc_alarm_struct.AlarmTime.Hours = hour;
    rtc_alarm_struct.AlarmTime.Minutes = minute;
    rtc_alarm_struct.AlarmTime.Seconds = second;
    rtc_alarm_struct.AlarmTime.TimeFormat = RTC_HOURFORMAT12_AM;
    rtc_alarm_struct.AlarmTime.SubSeconds = 0;
    rtc_alarm_struct.AlarmMask = RTC_ALARMMASK_NONE;
    rtc_alarm_struct.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_NONE;
    rtc_alarm_struct.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_WEEKDAY;
    rtc_alarm_struct.AlarmDateWeekDay = week;
    rtc_alarm_struct.Alarm = RTC_ALARM_A;
    HAL_RTC_SetAlarm_IT(&hrtc, &rtc_alarm_struct, RTC_FORMAT_BIN);
}

/**
 * @brief   设置RTC周期性唤醒中断
 * @param   clock: 唤醒时钟
 * @param   count: 唤醒计数器
 * @retval  无
 */
void rtc_set_wakeup(uint8_t clock, uint8_t count)
{
    HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, count, clock, 0);
}
