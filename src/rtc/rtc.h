/**
 * @file rtc.h
 * @brief Real-Time Clock Management Header
 *
 * Provides RTC functionality for time keeping and display.
 *
 * @author Yehuda@YehudaE.net
 */

#ifndef RTC_H
#define RTC_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize RTC module
 *
 * Sets up RTC hardware and initializes to invalid time (--:--)
 *
 * @return 0 on success, negative error code on failure
 */
int ENR_rtc_init(void);

/**
 * @brief Set RTC time from Unix timestamp
 *
 * @param timestamp Unix timestamp (seconds since epoch)
 * @return 0 on success, negative error code on failure
 */
int ENR_rtc_set_time(uint32_t timestamp);

/**
 * @brief Get current Unix timestamp
 *
 * @return Unix timestamp, or 0 if time not set
 */
uint32_t rtc_get_timestamp(void);

/**
 * @brief Check if RTC time is valid (has been set)
 *
 * @return true if time is valid, false if not set
 */
bool rtc_is_time_valid(void);

/**
 * @brief Format current time as HH:MM
 *
 * @param buffer Buffer to store formatted time (min 6 bytes)
 * @param size Buffer size
 * @return 0 on success, negative if time not valid
 */
int rtc_format_time(char* buffer, size_t size);

/**
 * @brief Calculate relative time string
 *
 * Converts timestamp to relative time like "now", "5m ago", etc.
 *
 * @param timestamp Past timestamp to compare against current time
 * @param buffer Buffer to store result (min 16 bytes)
 * @param size Buffer size
 * @return 0 on success, negative error code on failure
 */
int rtc_format_relative_time(uint32_t timestamp, char* buffer, size_t size);

/**
 * @brief Get current time as struct tm
 *
 * @param tm Pointer to tm structure to fill
 * @return 0 on success, negative if time not valid
 */
int ENR_rtc_get_time(struct tm* tm);

#ifdef __cplusplus
}
#endif

#endif /* RTC_H */
