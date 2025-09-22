/**
 * @file watchdog.c
 * @brief Watchdog Timer Management Module for Zephyr RTOS
 *
 * This module provides a comprehensive interface for managing hardware watchdog
 * timers in Zephyr RTOS applications. It handles initialization, configuration,
 * feeding, and deinitialization of the watchdog timer to prevent system resets
 * during normal operation and trigger resets when the system becomes unresponsive.
 *
 * Features:
 * - Configurable timeout period (default: 30 seconds)
 * - Automatic system reset on timeout
 * - Debug mode support (pauses during debugging)
 * - Comprehensive error handling and logging
 *
 * Usage:
 * 1. Call enable_watchdog() during system initialization
 * 2. Call kick_watchdog() periodically from your main loop
 * 3. Call disable_watchdog() before system shutdown (if needed)
 *
 * @author Yehuda@YehudaE.net
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "watchdog/watchdog.h"

LOG_MODULE_REGISTER(watchdog, LOG_LEVEL_INF);

/*==============================================================================
 * CONSTANTS AND CONFIGURATION
 *============================================================================*/

/** @brief Watchdog timeout in milliseconds (10 seconds) */
#define WATCHDOG_TIMEOUT_MS 10000

/** @brief Minimum watchdog window in milliseconds */
#define WATCHDOG_MIN_WINDOW_MS 0

/** @brief Maximum number of watchdog setup retries */
#define WATCHDOG_SETUP_RETRIES 3

/*==============================================================================
 * STATIC VARIABLES
 *============================================================================*/

/** @brief Handle to the watchdog device */
static const struct device* watchdog_device = DEVICE_DT_GET(DT_CHOSEN(nr_wdt));

/** @brief Channel ID assigned by the watchdog driver */
static int channel_id = -1;

/** @brief Flag indicating if watchdog is currently enabled */
static bool watchdog_enabled = false;

/*==============================================================================
 * PRIVATE FUNCTIONS
 *============================================================================*/

/**
 * @brief Validates watchdog device availability
 * @return true if device is ready, false otherwise
 */
static bool is_watchdog_device_valid(void)
{
    if (!watchdog_device) {
        LOG_ERR("Watchdog device not found in devicetree");
        return false;
    }

    if (!device_is_ready(watchdog_device)) {
        LOG_ERR("Watchdog device is not ready");
        return false;
    }

    return true;
}

/*==============================================================================
 * PUBLIC FUNCTIONS
 *============================================================================*/

/**
 * @brief Enables and configures the hardware watchdog timer
 *
 * This function initializes the watchdog timer with the following configuration:
 * - Timeout: WATCHDOG_TIMEOUT_MS milliseconds
 * - Window: 0 to WATCHDOG_TIMEOUT_MS (allows feeding at any time)
 * - Flags: Reset entire SoC on timeout
 * - Debug: Pause during debugging sessions
 *
 * @return 0 on success
 * @return -EIO if watchdog device is not ready
 * @return -EINVAL if configuration parameters are invalid
 * @return -ENOTSUP if watchdog features are not supported
 * @return Negative error code from watchdog driver on other failures
 */
int enable_watchdog(void)
{
    int ret;

    /* Validate device availability */
    if (!is_watchdog_device_valid()) {
        return -EIO;
    }

    /* Check if already enabled */
    if (watchdog_enabled) {
        LOG_WRN("Watchdog is already enabled");
        return 0;
    }

    LOG_INF("Initializing watchdog timer (timeout: %d ms)", WATCHDOG_TIMEOUT_MS);

    /* Configure watchdog timeout parameters */
    struct wdt_timeout_cfg watchdog_config = {
        .window = {
            .min = WATCHDOG_MIN_WINDOW_MS,
            .max = WATCHDOG_TIMEOUT_MS,
        },
        .callback = NULL, /* Use reset instead of callback */
        .flags = WDT_FLAG_RESET_SOC, /* Reset entire SoC on timeout */
    };

    /* Install timeout configuration */
    ret = wdt_install_timeout(watchdog_device, &watchdog_config);
    if (ret < 0) {
        LOG_ERR("Failed to install watchdog timeout configuration (ret: %d)", ret);
        return ret;
    }

    channel_id = ret;
    LOG_DBG("Watchdog timeout installed successfully, channel ID: %d", channel_id);

    /* Setup watchdog with debug support */
    ret = wdt_setup(watchdog_device, WDT_OPT_PAUSE_HALTED_BY_DBG);
    if (ret != 0) {
        LOG_ERR("Failed to complete watchdog setup (ret: %d)", ret);
        return ret;
    }

    watchdog_enabled = true;
    LOG_INF("Watchdog timer enabled successfully");

    return 0;
}

/**
 * @brief Disables the hardware watchdog timer
 *
 * Stops all watchdog timers and disables the watchdog functionality.
 * This should typically be called before system shutdown or when
 * entering a mode where the watchdog is not needed.
 *
 * @return 0 on success
 * @return Negative error code from watchdog driver on failure
 */
int disable_watchdog(void)
{
    int ret;

    if (!watchdog_enabled) {
        LOG_WRN("Watchdog is already disabled");
        return 0;
    }

    LOG_INF("Disabling watchdog timer");

    ret = wdt_disable(watchdog_device);
    if (ret != 0) {
        LOG_ERR("Failed to disable watchdog timer (ret: %d)", ret);
        return ret;
    }

    watchdog_enabled = false;
    channel_id = -1;
    LOG_INF("Watchdog timer disabled successfully");

    return 0;
}

/**
 * @brief Feeds (kicks) the watchdog timer to prevent timeout
 *
 * This function must be called periodically (within WATCHDOG_TIMEOUT_MS)
 * to prevent the watchdog from resetting the system. It should be called
 * from the main application loop or a dedicated watchdog thread.
 *
 * @note This function should be called frequently but not too frequently
 *       to avoid unnecessary overhead. A good practice is to call it
 *       at 1/4 to 1/2 of the timeout interval.
 *
 * @return 0 on success
 * @return -EINVAL if watchdog is not enabled or channel_id is invalid
 * @return Negative error code from watchdog driver on other failures
 */
int kick_watchdog(void)
{
    int ret;

    if (!watchdog_enabled) {
        LOG_WRN("Cannot feed watchdog: watchdog is not enabled");
        return -EINVAL;
    }

    if (channel_id < 0) {
        LOG_ERR("Invalid watchdog channel ID: %d", channel_id);
        return -EINVAL;
    }

    ret = wdt_feed(watchdog_device, channel_id);
    if (ret != 0) {
        LOG_ERR("Failed to feed watchdog timer (ret: %d)", ret);
        return ret;
    }

    LOG_DBG("Watchdog timer fed successfully");
    return 0;
}

/**
 * @brief Gets the current status of the watchdog timer
 *
 * @return true if watchdog is enabled, false otherwise
 */
bool is_watchdog_enabled(void)
{
    return watchdog_enabled;
}

/**
 * @brief Gets the configured watchdog timeout in milliseconds
 *
 * @return Timeout value in milliseconds
 */
uint32_t get_watchdog_timeout_ms(void)
{
    return WATCHDOG_TIMEOUT_MS;
}

/**
 * @brief Gets the watchdog channel ID
 *
 * @return Channel ID if watchdog is enabled, -1 otherwise
 */
int get_watchdog_channel_id(void)
{
    return channel_id;
}
