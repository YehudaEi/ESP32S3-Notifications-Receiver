/**
 * @file watchdog.h
 * @brief Watchdog Timer Management Module Header
 *
 * This header defines the public interface for the watchdog timer management
 * module in Zephyr RTOS applications.
 *
 * @author Yehuda@YehudaE.net
 */

#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * PUBLIC FUNCTION DECLARATIONS
 *============================================================================*/

/**
 * @brief Enables and configures the hardware watchdog timer
 *
 * Initializes the watchdog timer with predefined configuration parameters.
 * Must be called before using kick_watchdog().
 *
 * @return 0 on success, negative error code on failure
 */
int enable_watchdog(void);

/**
 * @brief Disables the hardware watchdog timer
 *
 * Stops the watchdog timer and disables watchdog functionality.
 * Should be called before system shutdown if needed.
 *
 * @return 0 on success, negative error code on failure
 */
int disable_watchdog(void);

/**
 * @brief Feeds (kicks) the watchdog timer to prevent timeout
 *
 * Must be called periodically to prevent system reset.
 * Call frequency should be less than the configured timeout period.
 *
 * @return 0 on success, negative error code on failure
 */
int kick_watchdog(void);

/**
 * @brief Gets the current status of the watchdog timer
 *
 * @return true if watchdog is enabled and running, false otherwise
 */
bool is_watchdog_enabled(void);

/**
 * @brief Gets the configured watchdog timeout in milliseconds
 *
 * @return Timeout value in milliseconds
 */
uint32_t get_watchdog_timeout_ms(void);

/**
 * @brief Gets the watchdog channel ID
 *
 * @return Channel ID if watchdog is enabled, -1 if disabled
 */
int get_watchdog_channel_id(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCHDOG_H */
