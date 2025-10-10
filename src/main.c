/**
 * @file main.c
 * @brief Notifications Receiver Main Application
 *
 * This is the main application file for YNotificator - a Zephyr RTOS based
 * notification receiver that gets notifications from Android apps
 * via Bluetooth Low Energy (BLE).
 *
 * The application manages:
 * - System initialization (watchdog, RTC, display, GUI, BLE)
 * - Main event loop with LVGL graphics processing
 * - Watchdog maintenance for system stability
 * - Time synchronization and display updates
 * - BLE communication for notification reception
 *
 * @author Yehuda@YehudaE.net
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include "bluetooth/bluetooth.h"
#include "display/display.h"
#include "graphics/graphics.h"
#include "notifications/notifications.h"
#include "rtc/rtc.h"
#include "watchdog/watchdog.h"

/* Register logging module */
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/** @brief Main thread sleep interval in milliseconds */
#define MAIN_THREAD_SLEEP_TIME_MS 100

/** @brief Application name for logging and identification */
#define APP_DEVICE_NAME "YNotificator"

/** @brief Maximum number of initialization retry attempts */
#define MAX_INIT_RETRIES 3

/*==============================================================================
 * ERROR CALLBACKS (Empty implementations as requested)
 *============================================================================*/

static void on_malformed_packet(const char* error_msg)
{
    LOG_WRN("Malformed packet: %s", error_msg);
    /* Empty callback - future implementation */
}

static void on_connection_drop(const char* error_msg)
{
    LOG_WRN("Connection dropped: %s", error_msg);
    /* Empty callback - future implementation */
}

static void on_buffer_overflow(const char* error_msg)
{
    LOG_ERR("Buffer overflow: %s", error_msg);
    /* Empty callback - future implementation */
}

/**
 * @brief Update time displays
 *
 * Updates the top bar time and notification relative timestamps.
 * Should be called periodically from main loop.
 */
static void update_time_displays(void)
{
    static uint32_t last_time_update = 0;
    uint32_t now = k_uptime_get_32();

    /* Update top bar time every minute (60000 ms) */
    if (now - last_time_update >= 60000) {
        char time_str[16];
        if (rtc_format_time(time_str, sizeof(time_str)) == 0) {
            notifications_update_time(time_str);
        }
        last_time_update = now;
    }
}

/**
 * @brief Initialize system watchdog
 *
 * Sets up and enables the hardware watchdog timer to ensure system stability.
 * The watchdog will reset the system if not periodically refreshed.
 *
 * @return 0 on success, negative error code on failure
 */
static int init_system_watchdog(void)
{
    int ret;
    int retry_count = 0;

    LOG_INF("Initializing system watchdog...");

    do {
        ret = enable_watchdog();
        if (ret == 0) {
            LOG_INF("Watchdog enabled successfully");
            return 0;
        }

        retry_count++;
        LOG_WRN("Watchdog initialization failed (attempt %d/%d), ret = %d",
            retry_count, MAX_INIT_RETRIES, ret);

        if (retry_count < MAX_INIT_RETRIES) {
            k_sleep(K_MSEC(100));
        }
    } while (retry_count < MAX_INIT_RETRIES);

    LOG_ERR("Failed to initialize watchdog after %d attempts", MAX_INIT_RETRIES);
    return ret;
}

/**
 * @brief Initialize RTC subsystem
 *
 * Sets up the Real-Time Clock for time keeping.
 *
 * @return 0 on success, negative error code on failure
 */
static int init_rtc_subsystem(void)
{
    int ret;

    LOG_INF("Initializing RTC subsystem...");

    ret = ENR_rtc_init();
    if (ret != 0) {
        LOG_ERR("RTC initialization failed, ret = %d", ret);
        return ret;
    }

    LOG_INF("RTC initialized successfully (time will sync via BLE)");
    return 0;
}

/**
 * @brief Initialize display subsystem
 *
 * Sets up the LCD display and backlight for the user interface.
 *
 * @return 0 on success, negative error code on failure
 */
static int init_display_subsystem(void)
{
    int ret;
    int retry_count = 0;

    LOG_INF("Initializing display subsystem...");

    do {
        ret = enable_display();
        if (ret == 0) {
            LOG_INF("Display enabled successfully");
            return 0;
        }

        retry_count++;
        LOG_WRN("Display initialization failed (attempt %d/%d), ret = %d",
            retry_count, MAX_INIT_RETRIES, ret);

        if (retry_count < MAX_INIT_RETRIES) {
            k_sleep(K_MSEC(200));
        }
    } while (retry_count < MAX_INIT_RETRIES);

    LOG_ERR("Failed to initialize display after %d attempts", MAX_INIT_RETRIES);
    return ret;
}

/**
 * @brief Initialize BLE communication
 *
 * Sets up Bluetooth Low Energy stack for receiving notifications from
 * connected Android devices.
 *
 * @return 0 on success, negative error code on failure
 */
static int init_ble_communication(void)
{
    int ret;

    LOG_INF("Initializing BLE communication...");

    /* Initialize BLE */
    ret = init_bluetooth();
    if (ret != 0) {
        LOG_ERR("BLE initialization failed, ret = %d", ret);
        return ret;
    }

    /* Register error callbacks */
    register_malformed_packet_callback(on_malformed_packet);
    register_connection_drop_callback(on_connection_drop);
    register_buffer_overflow_callback(on_buffer_overflow);

    LOG_INF("BLE communication initialized successfully");
    return 0;
}

/**
 * @brief Print system information and status
 *
 * Displays important system information including device name,
 * BLE service details, and operational status.
 */
static void print_system_info(void)
{
    LOG_INF("=== %s Ready! ===", APP_DEVICE_NAME);
    LOG_INF("Device name: %s", APP_DEVICE_NAME);
    LOG_INF("Main loop interval: %d ms", MAIN_THREAD_SLEEP_TIME_MS);
    LOG_INF("Ready to receive notifications via BLE!");
    LOG_INF("Advertising and waiting for Android connection...");
    LOG_INF("Time will sync automatically when connected");
}

/**
 * @brief Perform system shutdown sequence
 *
 * Gracefully shuts down all subsystems before system restart or power off.
 */
static void shutdown_system(void)
{
    LOG_INF("Initiating system shutdown sequence...");

    /* Stop BLE advertising */
    stop_advertising();

    /* Disable display to save power and protect screen */
    if (disable_display() != 0) {
        LOG_WRN("Failed to properly disable display during shutdown");
    }

    LOG_INF("System shutdown sequence completed");
}

/**
 * @brief Main application entry point
 *
 * Initializes all system components and runs the main application loop.
 * The main loop handles:
 * - LVGL graphics processing
 * - Time display updates
 * - Notification timers
 * - Watchdog maintenance
 * - BLE communication processing
 *
 * @return 0 on normal exit (should not happen), error code on failure
 */
int main(void)
{
    int ret;

    LOG_INF("Starting %s Notification Receiver", APP_DEVICE_NAME);

    /* Initialize critical system components */

    /* 1. Initialize watchdog first for system protection */
    ret = init_system_watchdog();
    if (ret != 0) {
        LOG_ERR("Critical: Watchdog initialization failed, ret = %d", ret);
        goto error_exit;
    }

    /* 2. Initialize RTC subsystem */
    ret = init_rtc_subsystem();
    if (ret != 0) {
        LOG_ERR("Critical: RTC initialization failed, ret = %d", ret);
        goto error_exit;
    }

    /* 3. Initialize display subsystem */
    ret = init_display_subsystem();
    if (ret != 0) {
        LOG_ERR("Critical: Display initialization failed, ret = %d", ret);
        goto error_exit;
    }

    /* 4. Initialize LVGL graphics library */
    ret = init_lvgl_graphics();
    if (ret != 0) {
        LOG_ERR("Critical: LVGL initialization failed, ret = %d", ret);
        goto error_exit;
    }

    /* 5. Wait a moment for LVGL to be fully ready */
    k_sleep(K_MSEC(100));

    /* 6. Create the notification screen */
    LOG_INF("Creating notification screen...");
    create_notification_screen();
    LOG_INF("Notification screen created successfully");

    /* 7. Initialize BLE communication */
    ret = init_ble_communication();
    if (ret != 0) {
        LOG_ERR("Critical: BLE initialization failed, ret = %d", ret);
        goto error_exit;
    }

    /* All systems initialized successfully */
    print_system_info();

    /* Main application loop */
    LOG_INF("Entering main application loop...");

    while (true) {
        /* Update time displays (top bar and timestamps) */
        update_time_displays();

        /* Handle notification timers (delete timeout and timestamp updates) */
        notifications_handle_timers();

        /* Maintain watchdog to prevent system reset */
        ret = kick_watchdog();
        if (ret != 0) {
            LOG_ERR("Watchdog kick failed, ret = %d", ret);
            /* Continue operation but log the error */
        }

/* Optional: Run demo status changes for testing */
#ifdef CONFIG_DEMO_MODE
        demo_status_changes();
#endif

        /* TODO: Add other periodic tasks here:
         * - Process BLE notifications
         * - Update time display
         * - Handle user input
         * - Manage power states
         * - Check battery status
         */

        /* Sleep to allow other threads to run and save power */
        k_sleep(K_MSEC(MAIN_THREAD_SLEEP_TIME_MS));
    }

    /* This point should never be reached in normal operation */
    LOG_WRN("Main loop exited unexpectedly");
    return 0;

error_exit:
    LOG_ERR("System initialization failed, initiating shutdown");
    shutdown_system();

    /* Optionally restart the system after a delay */
    LOG_INF("System will restart in 5 seconds...");
    k_sleep(K_SECONDS(5));
    sys_reboot(SYS_REBOOT_COLD);

    /* Should never reach here */
    return ret;
}
