/**
 * @file rtc.c
 * @brief Real-Time Clock Management Implementation
 *
 * @author Yehuda@YehudaE.net
 */

#include "rtc/rtc.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(rtc_module, LOG_LEVEL_INF);

/*==============================================================================
 * STATIC VARIABLES
 *============================================================================*/

static const struct device* rtc_dev = DEVICE_DT_GET(DT_CHOSEN(nr_rtc));
static bool time_is_valid = false;
static uint32_t last_set_timestamp = 0;

/*==============================================================================
 * PUBLIC FUNCTIONS
 *============================================================================*/

int ENR_rtc_init(void)
{
    LOG_INF("Initializing RTC module...");

    if (!device_is_ready(rtc_dev)) {
        LOG_ERR("RTC device is not ready");
        return -ENODEV;
    }

    time_is_valid = false;
    last_set_timestamp = 0;

    LOG_INF("RTC module initialized (time not set)");
    return 0;
}

int ENR_rtc_set_time(uint32_t timestamp)
{
    int ret;
    struct rtc_time rtc_time_val;
    time_t time = (time_t)timestamp;
    struct tm* tm_val;

    LOG_INF("Setting RTC time from timestamp: %u", timestamp);

    /* Convert Unix timestamp to struct tm */
    tm_val = gmtime(&time);
    if (!tm_val) {
        LOG_ERR("Failed to convert timestamp to tm structure");
        return -EINVAL;
    }

    /* Convert to rtc_time structure */
    rtc_time_val.tm_sec = tm_val->tm_sec;
    rtc_time_val.tm_min = tm_val->tm_min;
    rtc_time_val.tm_hour = tm_val->tm_hour;
    rtc_time_val.tm_mday = tm_val->tm_mday;
    rtc_time_val.tm_mon = tm_val->tm_mon;
    rtc_time_val.tm_year = tm_val->tm_year;
    rtc_time_val.tm_wday = tm_val->tm_wday;
    rtc_time_val.tm_yday = tm_val->tm_yday;
    rtc_time_val.tm_isdst = tm_val->tm_isdst;
    rtc_time_val.tm_nsec = 0;

    /* Set RTC time */
    ret = rtc_set_time(rtc_dev, &rtc_time_val);
    if (ret < 0) {
        LOG_ERR("Failed to set RTC time (ret: %d)", ret);
        return ret;
    }

    time_is_valid = true;
    last_set_timestamp = timestamp;

    LOG_INF("RTC time set successfully: %04d-%02d-%02d %02d:%02d:%02d",
        tm_val->tm_year + 1900, tm_val->tm_mon + 1, tm_val->tm_mday,
        tm_val->tm_hour, tm_val->tm_min, tm_val->tm_sec);

    return 0;
}

uint32_t rtc_get_timestamp(void)
{
    struct rtc_time rtc_time_val;
    struct tm tm_val;
    int ret;

    if (!time_is_valid) {
        return 0;
    }

    ret = rtc_get_time(rtc_dev, &rtc_time_val);
    if (ret < 0) {
        LOG_ERR("Failed to get RTC time (ret: %d)", ret);
        return 0;
    }

    /* Convert rtc_time to struct tm */
    memset(&tm_val, 0, sizeof(tm_val));
    tm_val.tm_sec = rtc_time_val.tm_sec;
    tm_val.tm_min = rtc_time_val.tm_min;
    tm_val.tm_hour = rtc_time_val.tm_hour;
    tm_val.tm_mday = rtc_time_val.tm_mday;
    tm_val.tm_mon = rtc_time_val.tm_mon;
    tm_val.tm_year = rtc_time_val.tm_year;
    tm_val.tm_isdst = -1;

    /* Convert to Unix timestamp */
    time_t timestamp = mktime(&tm_val);
    if (timestamp == (time_t)-1) {
        LOG_ERR("Failed to convert RTC time to timestamp");
        return 0;
    }

    return (uint32_t)timestamp;
}

bool rtc_is_time_valid(void)
{
    return time_is_valid;
}

int rtc_format_time(char* buffer, size_t size)
{
    struct rtc_time rtc_time_val;
    int ret;

    if (!buffer || size < 6) {
        return -EINVAL;
    }

    if (!time_is_valid) {
        snprintf(buffer, size, "--:--");
        return -EAGAIN;
    }

    ret = rtc_get_time(rtc_dev, &rtc_time_val);
    if (ret < 0) {
        LOG_ERR("Failed to get RTC time (ret: %d)", ret);
        snprintf(buffer, size, "--:--");
        return ret;
    }

    snprintf(buffer, size, "%02d:%02d", rtc_time_val.tm_hour, rtc_time_val.tm_min);
    return 0;
}

int rtc_format_relative_time(uint32_t timestamp, char* buffer, size_t size)
{
    uint32_t current_time;
    int32_t diff_seconds;
    int diff_minutes;
    int diff_hours;
    int diff_days;

    if (!buffer || size < 16) {
        return -EINVAL;
    }

    if (!time_is_valid || timestamp == 0) {
        snprintf(buffer, size, "unknown");
        return -EAGAIN;
    }

    current_time = rtc_get_timestamp();
    if (current_time == 0) {
        snprintf(buffer, size, "unknown");
        return -EAGAIN;
    }

    diff_seconds = (int32_t)(current_time - timestamp);

    /* Future time (shouldn't happen) */
    if (diff_seconds < 0) {
        snprintf(buffer, size, "now");
        return 0;
    }

    /* Less than 10 seconds */
    if (diff_seconds < 10) {
        snprintf(buffer, size, "now");
        return 0;
    }

    /* Less than 60 seconds */
    if (diff_seconds < 60) {
        snprintf(buffer, size, "%ds ago", diff_seconds);
        return 0;
    }

    diff_minutes = diff_seconds / 60;

    /* Less than 60 minutes */
    if (diff_minutes < 60) {
        snprintf(buffer, size, "%dm ago", diff_minutes);
        return 0;
    }

    diff_hours = diff_minutes / 60;

    /* Less than 24 hours */
    if (diff_hours < 24) {
        snprintf(buffer, size, "%dh ago", diff_hours);
        return 0;
    }

    diff_days = diff_hours / 24;

    /* Less than 7 days */
    if (diff_days < 7) {
        if (diff_days == 1) {
            snprintf(buffer, size, "yesterday");
        } else {
            snprintf(buffer, size, "%dd ago", diff_days);
        }
        return 0;
    }

    /* More than a week - show date */
    time_t time = (time_t)timestamp;
    struct tm* tm_val = gmtime(&time);
    if (tm_val) {
        snprintf(buffer, size, "%02d/%02d", tm_val->tm_mday, tm_val->tm_mon + 1);
    } else {
        snprintf(buffer, size, "old");
    }

    return 0;
}

int ENR_rtc_get_time(struct tm* tm)
{
    struct rtc_time rtc_time_val;
    int ret;

    if (!tm) {
        return -EINVAL;
    }

    if (!time_is_valid) {
        return -EAGAIN;
    }

    ret = rtc_get_time(rtc_dev, &rtc_time_val);
    if (ret < 0) {
        LOG_ERR("Failed to get RTC time (ret: %d)", ret);
        return ret;
    }

    memset(tm, 0, sizeof(*tm));
    tm->tm_sec = rtc_time_val.tm_sec;
    tm->tm_min = rtc_time_val.tm_min;
    tm->tm_hour = rtc_time_val.tm_hour;
    tm->tm_mday = rtc_time_val.tm_mday;
    tm->tm_mon = rtc_time_val.tm_mon;
    tm->tm_year = rtc_time_val.tm_year;
    tm->tm_wday = rtc_time_val.tm_wday;
    tm->tm_yday = rtc_time_val.tm_yday;
    tm->tm_isdst = rtc_time_val.tm_isdst;

    return 0;
}
