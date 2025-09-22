/**
 * @file main.c
 * @brief Notifications Receiver Main Application with BLE Support
 *
 * This is the main application file for ZephyrWatch - a Zephyr RTOS based
 * smartwatch device that receives notifications from Android apps via
 * Bluetooth Low Energy (BLE).
 *
 * The application manages:
 * - System initialization (watchdog, display, GUI, BLE)
 * - Main event loop with LVGL graphics processing
 * - BLE GATT server for notification reception
 * - Watchdog maintenance for system stability
 * - Integration with notification UI system
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
#include "pairing/pairing_screen.h"
#include "watchdog/watchdog.h"

/* Register logging module */
LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/** @brief Main thread sleep interval in milliseconds */
#define MAIN_THREAD_SLEEP_TIME_MS 100

/** @brief Application name for logging and identification */
#define APP_DEVICE_NAME "ZephyrWatch"

/** @brief BLE service UUID for notification service */
#define BLE_SERVICE_UUID "12345678-1234-1234-1234-123456789abc"

/** @brief BLE characteristic UUID for notification data */
#define BLE_CHARACTERISTIC_UUID "87654321-4321-4321-4321-cba987654321"

/** @brief Maximum Bluetooth address string length */
#define BT_ADDR_STR_LEN 18

/** @brief Maximum number of initialization retry attempts */
#define MAX_INIT_RETRIES 3

/** @brief Time update interval in seconds */
#define TIME_UPDATE_INTERVAL_S 60

/*==============================================================================
 * STATIC VARIABLES
 *============================================================================*/

static bool ble_ready = false;
static ble_connection_stats_t last_ble_stats = { 0 };
static int64_t last_time_update = 0;

/*==============================================================================
 * BLE CALLBACK FUNCTIONS
 *============================================================================*/

/**
 * @brief BLE connection status change callback
 */
static void ble_status_changed(ble_connection_status_t status, void* user_data)
{
    const char* status_str;

    switch (status) {
    case BLE_STATUS_DISCONNECTED:
        status_str = "Disconnected";
        ble_ready = false;
        break;
    case BLE_STATUS_ADVERTISING:
        status_str = "Advertising";
        ble_ready = false;
        break;
    case BLE_STATUS_CONNECTING:
        status_str = "Connecting";
        ble_ready = false;
        break;
    case BLE_STATUS_CONNECTED:
        status_str = "Connected";
        ble_ready = false;
        break;
    case BLE_STATUS_READY:
        status_str = "Ready";
        ble_ready = true;
        break;
    default:
        status_str = "Unknown";
        ble_ready = false;
        break;
    }

    LOG_INF("BLE Status: %s", status_str);
}

/**
 * @brief BLE notification received callback
 */
static void ble_notification_received(const ble_notification_data_t* notification, void* user_data)
{
    LOG_INF("BLE Notification: [%s] %s - %s",
        notification->app_name, notification->title, notification->text);

    /* The notification has already been added to the UI by the BLE module,
     * but we could perform additional processing here if needed */
}

/*==============================================================================
 * SYSTEM INITIALIZATION FUNCTIONS
 *============================================================================*/

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
 * Sets up Bluetooth Low Energy GATT server for receiving notifications from
 * connected Android devices.
 *
 * @return 0 on success, negative error code on failure
 */
static int init_ble_communication(void)
{
    int ret;
    ble_config_t config = { 0 };

    LOG_INF("Initializing BLE communication...");

    /* Configure BLE callbacks and settings */
    config.status_callback = ble_status_changed;
    config.notification_callback = ble_notification_received;
    config.user_data = NULL;
    config.enable_debug_logs = true; /* Enable for development, disable for production */
    config.preferred_mtu = 247; /* Request larger MTU for better throughput */

    ret = ble_init_notification_server(&config);
    if (ret) {
        LOG_ERR("Failed to initialize BLE server: %d", ret);
        return ret;
    }

    LOG_INF("BLE communication initialized successfully");
    LOG_INF("BLE Service UUID: %s", BLE_SERVICE_UUID);
    LOG_INF("BLE Characteristic UUID: %s", BLE_CHARACTERISTIC_UUID);
    LOG_INF("Device advertising as: %s", CONFIG_BT_DEVICE_NAME);

    return 0;
}

/*==============================================================================
 * PERIODIC UPDATE FUNCTIONS
 *============================================================================*/

/**
 * @brief Update time display
 *
 * Updates the notification screen time display based on system uptime.
 */
static void update_time_display(void)
{
    int64_t uptime_ms = k_uptime_get();

    /* Simple time calculation based on uptime (for demo purposes) */
    int hours = ((uptime_ms / 3600000) + 14) % 24; /* Start at 14:00 */
    int minutes = (uptime_ms / 60000) % 60;

    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", hours, minutes);

    notifications_update_time(time_str);
}

/**
 * @brief Print BLE statistics periodically
 */
static void print_ble_statistics(void)
{
    ble_connection_stats_t stats;
    int ret = ble_get_connection_stats(&stats);
    if (ret != 0) {
        return;
    }

    /* Only print if statistics have changed significantly */
    if (stats.notifications_received != last_ble_stats.notifications_received || stats.connection_count != last_ble_stats.connection_count || stats.parse_errors != last_ble_stats.parse_errors) {

        LOG_INF("BLE Stats - Connections: %u/%u, Notifications: %u, Errors: %u, MTU: %u",
            stats.successful_connections, stats.connection_count,
            stats.notifications_received, stats.parse_errors, stats.current_mtu);

        if (ble_ready) {
            char device_addr[BT_ADDR_STR_LEN];
            if (ble_get_connected_device_info(device_addr, sizeof(device_addr)) == 0) {
                LOG_INF("Connected device: %s", device_addr);
            }
        }

        memcpy(&last_ble_stats, &stats, sizeof(stats));
    }
}

/**
 * @brief Print system information and status
 *
 * Displays important system information including device name,
 * BLE service details, and operational status.
 */
static void print_system_info(void)
{
    LOG_INF("=== %s Notifications Receiver Ready! ===", APP_DEVICE_NAME);
    LOG_INF("Device name: %s", APP_DEVICE_NAME);
    LOG_INF("Service UUID: %s", BLE_SERVICE_UUID);
    LOG_INF("Characteristic UUID: %s", BLE_CHARACTERISTIC_UUID);
    LOG_INF("Main loop interval: %d ms", MAIN_THREAD_SLEEP_TIME_MS);
    LOG_INF("Time update interval: %d seconds", TIME_UPDATE_INTERVAL_S);
    LOG_INF("Ready to receive notifications from Android app!");
}

/**
 * @brief Perform system shutdown sequence
 *
 * Gracefully shuts down all subsystems before system restart or power off.
 */
static void shutdown_system(void)
{
    LOG_INF("Initiating system shutdown sequence...");

    /* Shutdown BLE first to disconnect clients gracefully */
    if (ble_is_ready()) {
        LOG_INF("Shutting down BLE server...");
        ble_shutdown_notification_server();
        k_sleep(K_MSEC(500)); /* Allow time for disconnection */
    }

    /* Disable display to save power and protect screen */
    if (disable_display() != 0) {
        LOG_WRN("Failed to properly disable display during shutdown");
    }

    /* Shutdown graphics library only if it was initialized */
    if (is_lvgl_ready()) {
        LOG_INF("Shutting down LVGL graphics...");
        deinit_lvgl_graphics();
    }

    LOG_INF("System shutdown sequence completed");
}

/*==============================================================================
 * MAIN APPLICATION
 *============================================================================*/

/**
 * @brief Main application entry point
 *
 * Initializes all system components and runs the main application loop.
 * The main loop handles:
 * - LVGL graphics processing (handled automatically by graphics thread)
 * - Notification timers
 * - Watchdog maintenance
 * - BLE event processing
 * - Periodic status updates
 * - Time display updates
 *
 * @return 0 on normal exit (should not happen), error code on failure
 */
int main(void)
{
    int ret;
    int loop_counter = 0;

    LOG_INF("Starting %s Notifications Receiver", APP_DEVICE_NAME);

    /* Initialize critical system components */

    /* 1. Initialize watchdog first for system protection */
    ret = init_system_watchdog();
    if (ret != 0) {
        LOG_ERR("Critical: Watchdog initialization failed, ret = %d", ret);
        goto error_exit;
    }

    /* 2. Initialize display subsystem */
    ret = init_display_subsystem();
    if (ret != 0) {
        LOG_ERR("Critical: Display initialization failed, ret = %d", ret);
        goto error_exit;
    }

    /* 3. Initialize LVGL graphics library */
    ret = init_lvgl_graphics();
    if (ret != 0) {
        LOG_ERR("Critical: LVGL initialization failed, ret = %d", ret);
        goto error_exit;
    }

    /* 4. Wait a moment for LVGL to be fully ready */
    k_sleep(K_MSEC(100));

    /* 5. Create the notification screen */
    LOG_INF("Creating notification screen...");
    create_notification_screen();
    LOG_INF("Notification screen created successfully");

    /* 6. Initialize BLE communication */
    ret = init_ble_communication();
    if (ret != 0) {
        LOG_ERR("Critical: BLE initialization failed, ret = %d", ret);
        goto error_exit;
    }

    /* All systems initialized successfully */
    print_system_info();

    /* Initialize time display */
    update_time_display();
    last_time_update = k_uptime_get();

    /* Main application loop */
    LOG_INF("Entering main application loop...");

    while (true) {
        /* Handle notification timers (delete timeout, etc.) */
        notifications_handle_timers();

        /* Handle pairing screen timeout if active */
        if (pairing_screen_is_active()) {
            uint32_t remaining = pairing_screen_update_timeout();
            if (remaining == 0) {
                LOG_INF("Pairing timed out");
            }
        }

        /* Maintain watchdog to prevent system reset */
        ret = kick_watchdog();
        if (ret != 0) {
            LOG_ERR("Watchdog kick failed, ret = %d", ret);
            /* Continue operation but log the error */
        }

        /* Process BLE events (though Zephyr handles this automatically) */
        ble_process_events();

        /* Update time display periodically */
        int64_t current_time = k_uptime_get();
        if (current_time - last_time_update >= (TIME_UPDATE_INTERVAL_S * 1000)) {
            update_time_display();
            last_time_update = current_time;
        }

        /* Print statistics every 10 seconds (100 loops * 100ms) */
        if (loop_counter % 100 == 0) {
            print_ble_statistics();

            /* Print unread notification count */
            int unread_count = notifications_get_unread_count();
            if (unread_count > 0) {
                LOG_DBG("Unread notifications: %d", unread_count);
            }

            /* Print pairing status if active */
            if (pairing_screen_is_active()) {
                LOG_DBG("Pairing in progress - passkey: %06u",
                    pairing_screen_get_current_passkey());
            }
        }

        loop_counter++;

        /* Reset counter to prevent overflow */
        if (loop_counter >= 10000) {
            loop_counter = 0;
        }

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
