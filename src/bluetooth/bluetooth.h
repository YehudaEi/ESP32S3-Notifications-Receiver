/**
 * @file bluetooth.h
 * @brief Bluetooth Low Energy GATT Server for Notifications
 *
 * This module implements a BLE GATT server that receives notifications from
 * Android devices and forwards them to the notification system.
 *
 * Features:
 * - BLE GATT server with custom notification service
 * - Connection management and status reporting
 * - Protocol parsing for notification data
 * - Integration with notification UI system
 * - MTU negotiation support
 * - Connection parameter optimization
 *
 * @author Yehuda@YehudaE.net
 */

#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup bluetooth_api BLE Notifications API
 * @brief BLE GATT Server API for Notification Reception
 * @{
 */

/*==============================================================================
 * CONSTANTS AND CONFIGURATION
 *============================================================================*/

/** @brief Maximum notification data size */
#define BLE_MAX_NOTIFICATION_SIZE 512

/** @brief Maximum app name length */
#define BLE_MAX_APP_NAME_LEN 32

/** @brief Maximum notification title length */
#define BLE_MAX_TITLE_LEN 64

/** @brief Maximum notification text length */
#define BLE_MAX_TEXT_LEN 256

/*==============================================================================
 * TYPE DEFINITIONS
 *============================================================================*/

/**
 * @brief BLE connection status enumeration
 */
typedef enum {
    BLE_STATUS_DISCONNECTED, /**< No device connected */
    BLE_STATUS_ADVERTISING, /**< Advertising for connections */
    BLE_STATUS_CONNECTING, /**< Connection in progress */
    BLE_STATUS_CONNECTED, /**< Device connected but not ready */
    BLE_STATUS_READY /**< Connected and service discovered */
} ble_connection_status_t;

/**
 * @brief Notification command types (from Android protocol)
 */
typedef enum {
    BLE_CMD_ADD_NOTIFICATION = 0x01, /**< Add new notification */
    BLE_CMD_REMOVE_NOTIFICATION = 0x02, /**< Remove notification */
    BLE_CMD_CLEAR_ALL = 0x03, /**< Clear all notifications */
    BLE_CMD_ACTION = 0x04 /**< Perform action on notification */
} ble_notification_cmd_t;

/**
 * @brief Notification type categories (from Android protocol)
 */
typedef enum {
    BLE_TYPE_PHONE = 0, /**< Phone calls */
    BLE_TYPE_MESSAGE = 1, /**< Messages, SMS, WhatsApp, Telegram */
    BLE_TYPE_EMAIL = 2, /**< Email applications */
    BLE_TYPE_SOCIAL = 3, /**< Social media apps */
    BLE_TYPE_CALENDAR = 4, /**< Calendar events */
    BLE_TYPE_OTHER = 5 /**< Other applications */
} ble_notification_type_t;

/**
 * @brief Parsed notification data structure
 */
typedef struct {
    ble_notification_type_t type; /**< Notification type */
    char app_name[BLE_MAX_APP_NAME_LEN]; /**< Application name */
    char title[BLE_MAX_TITLE_LEN]; /**< Notification title */
    char text[BLE_MAX_TEXT_LEN]; /**< Notification text content */
    bool is_priority; /**< Priority flag */
} ble_notification_data_t;

/**
 * @brief BLE connection statistics
 */
typedef struct {
    uint32_t connection_count; /**< Total connection attempts */
    uint32_t successful_connections; /**< Successful connections */
    uint32_t notifications_received; /**< Total notifications received */
    uint32_t parse_errors; /**< Protocol parsing errors */
    uint32_t current_mtu; /**< Current MTU size */
    uint64_t last_connection_time; /**< Last connection timestamp */
    uint64_t total_connected_time; /**< Total time connected */
} ble_connection_stats_t;

/*==============================================================================
 * CALLBACK FUNCTION TYPES
 *============================================================================*/

/**
 * @brief Connection status change callback
 *
 * Called when BLE connection status changes.
 *
 * @param status New connection status
 * @param user_data User data pointer passed during initialization
 */
typedef void (*ble_status_callback_t)(ble_connection_status_t status, void* user_data);

/**
 * @brief Notification received callback
 *
 * Called when a valid notification is received and parsed.
 *
 * @param notification Parsed notification data
 * @param user_data User data pointer passed during initialization
 */
typedef void (*ble_notification_callback_t)(const ble_notification_data_t* notification, void* user_data);

/**
 * @brief BLE configuration structure
 */
typedef struct {
    ble_status_callback_t status_callback; /**< Connection status callback */
    ble_notification_callback_t notification_callback; /**< Notification callback */
    void* user_data; /**< User data for callbacks */
    bool enable_debug_logs; /**< Enable detailed debug logging */
    uint16_t preferred_mtu; /**< Preferred MTU size (23-517) */
} ble_config_t;

/*==============================================================================
 * PUBLIC FUNCTION DECLARATIONS
 *============================================================================*/

/**
 * @brief Initialize BLE GATT server for notifications
 *
 * Sets up the BLE stack, registers the notification service, and starts
 * advertising. Must be called before any other BLE functions.
 *
 * @param config BLE configuration structure
 * @return 0 on success, negative error code on failure
 * @retval -EINVAL Invalid configuration parameters
 * @retval -ENOMEM Insufficient memory for BLE stack
 * @retval -EIO BLE hardware initialization failed
 */
int ble_init_notification_server(const ble_config_t* config);

/**
 * @brief Start BLE advertising
 *
 * Begins advertising the notification service to allow Android devices
 * to discover and connect.
 *
 * @return 0 on success, negative error code on failure
 * @retval -EBUSY Already advertising
 * @retval -EIO Failed to start advertising
 */
int ble_start_advertising(void);

/**
 * @brief Stop BLE advertising
 *
 * Stops advertising while maintaining any existing connections.
 *
 * @return 0 on success, negative error code on failure
 */
int ble_stop_advertising(void);

/**
 * @brief Disconnect current BLE connection
 *
 * Gracefully disconnects the current client device if connected.
 *
 * @return 0 on success, negative error code on failure
 * @retval -ENOTCONN No device currently connected
 */
int ble_disconnect_device(void);

/**
 * @brief Get current BLE connection status
 *
 * @return Current BLE connection status
 */
ble_connection_status_t ble_get_connection_status(void);

/**
 * @brief Get BLE connection statistics
 *
 * Retrieves statistical information about BLE connections and data transfer.
 *
 * @param stats Pointer to statistics structure to fill
 * @return 0 on success, negative error code on failure
 * @retval -EINVAL Invalid stats pointer
 */
int ble_get_connection_stats(ble_connection_stats_t* stats);

/**
 * @brief Get connected device information
 *
 * Retrieves information about the currently connected device.
 *
 * @param device_addr Buffer to store device address (must be at least 18 bytes)
 * @param addr_size Size of device_addr buffer
 * @return 0 on success, negative error code on failure
 * @retval -ENOTCONN No device currently connected
 * @retval -EINVAL Invalid parameters
 * @retval -ENOSPC Buffer too small
 */
int ble_get_connected_device_info(char* device_addr, size_t addr_size);

/**
 * @brief Update preferred connection parameters
 *
 * Requests updated connection parameters for better performance or power usage.
 *
 * @param min_interval Minimum connection interval (1.25ms units)
 * @param max_interval Maximum connection interval (1.25ms units)
 * @param latency Connection latency (number of intervals)
 * @param timeout Supervision timeout (10ms units)
 * @return 0 on success, negative error code on failure
 * @retval -ENOTCONN No device currently connected
 * @retval -EINVAL Invalid parameters
 */
int ble_update_connection_params(uint16_t min_interval, uint16_t max_interval,
    uint16_t latency, uint16_t timeout);

/**
 * @brief Send acknowledgment to Android device
 *
 * Sends an acknowledgment response to confirm notification receipt.
 * This is optional but can be used for reliability.
 *
 * @param cmd Command being acknowledged
 * @param status Status code (0 = success, others = error)
 * @return 0 on success, negative error code on failure
 * @retval -ENOTCONN No device currently connected
 * @retval -EBUSY Previous transmission still in progress
 */
int ble_send_acknowledgment(ble_notification_cmd_t cmd, uint8_t status);

/**
 * @brief Shutdown BLE notification server
 *
 * Gracefully shuts down the BLE server, disconnects clients, and stops
 * advertising. Should be called during system shutdown.
 *
 * @return 0 on success, negative error code on failure
 */
int ble_shutdown_notification_server(void);

/**
 * @brief Check if BLE is ready for operation
 *
 * @return true if BLE is initialized and ready, false otherwise
 */
bool ble_is_ready(void);

/**
 * @brief Get current MTU size
 *
 * @return Current MTU size in bytes, 0 if not connected
 */
uint16_t ble_get_current_mtu(void);

/**
 * @brief Process BLE events (call from main loop)
 *
 * Processes pending BLE events and callbacks. Should be called periodically
 * from the main application loop if not using threading.
 *
 * @return 0 on success, negative error code on failure
 */
int ble_process_events(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* BLUETOOTH_H */
