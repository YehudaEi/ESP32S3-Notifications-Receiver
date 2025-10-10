/**
 * @file bluetooth.h
 * @brief Bluetooth Low Energy Management Header
 *
 * This header provides the interface for BLE communication with Android devices
 * for receiving notifications.
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
 * @defgroup bluetooth_api Bluetooth API
 * @brief BLE notification receiver API
 * @{
 */

/** @brief BLE connection states */
typedef enum {
    BLE_STATE_DISCONNECTED,
    BLE_STATE_ADVERTISING,
    BLE_STATE_CONNECTING,
    BLE_STATE_CONNECTED,
    BLE_STATE_PAIRED
} ble_state_t;

/** @brief Notification types from Android */
typedef enum {
    NOTIF_TYPE_PHONE = 0,
    NOTIF_TYPE_MESSAGE = 1,
    NOTIF_TYPE_EMAIL = 2,
    NOTIF_TYPE_SOCIAL = 3,
    NOTIF_TYPE_CALENDAR = 4,
    NOTIF_TYPE_OTHER = 5
} notification_type_t;

/** @brief BLE command types */
typedef enum {
    CMD_ADD_NOTIFICATION = 0x01,
    CMD_REMOVE_NOTIFICATION = 0x02,
    CMD_CLEAR_ALL = 0x03,
    CMD_ACTION = 0x04,
    CMD_TIME_SYNC = 0x05
} ble_command_t;

/** @brief Notification packet structure */
typedef struct {
    ble_command_t command;
    notification_type_t type;
    uint8_t app_name_len;
    uint8_t title_len;
    uint8_t text_len;
    char app_name[32];
    char title[64];
    char text[256];
} notification_packet_t;

/** @brief BLE error callback types */
typedef void (*ble_error_callback_t)(const char* error_msg);

/**
 * @brief Initialize BLE subsystem
 *
 * Sets up BLE stack, security, GATT services, and starts advertising.
 *
 * @return 0 on success, negative error code on failure
 */
int init_bluetooth(void);

/**
 * @brief Start BLE advertising
 *
 * Begins advertising the device as "YNotificator" for connections.
 *
 * @return 0 on success, negative error code on failure
 */
int start_advertising(void);

/**
 * @brief Stop BLE advertising
 *
 * Stops advertising (useful before shutdown or during connection).
 *
 * @return 0 on success, negative error code on failure
 */
int stop_advertising(void);

/**
 * @brief Get current BLE connection state
 *
 * @return Current BLE state
 */
ble_state_t get_ble_state(void);

/**
 * @brief Get connected device address
 *
 * @param addr Buffer to store address (must be at least 18 bytes)
 * @return 0 on success, negative if not connected
 */
int get_connected_device_address(char* addr, size_t len);

/**
 * @brief Disconnect current BLE connection
 *
 * @return 0 on success, negative error code on failure
 */
int disconnect_ble(void);

/**
 * @brief Register callback for malformed packet errors
 *
 * @param callback Function to call on malformed packet
 */
void register_malformed_packet_callback(ble_error_callback_t callback);

/**
 * @brief Register callback for connection drop errors
 *
 * @param callback Function to call on connection drop
 */
void register_connection_drop_callback(ble_error_callback_t callback);

/**
 * @brief Register callback for buffer overflow errors
 *
 * @param callback Function to call on buffer overflow
 */
void register_buffer_overflow_callback(ble_error_callback_t callback);

/**
 * @brief Check if pairing is in progress
 *
 * @return true if pairing screen should be shown
 */
bool is_pairing_in_progress(void);

/**
 * @brief Get current pairing code
 *
 * @param code Buffer to store 6-digit pairing code
 * @param len Buffer length (should be at least 7 for null terminator)
 * @return 0 on success, negative if no pairing in progress
 */
int get_pairing_code(char* code, size_t len);

/**
 * @brief Confirm pairing (user accepted)
 *
 * @return 0 on success, negative error code on failure
 */
int confirm_pairing(void);

/**
 * @brief Reject pairing (user declined)
 *
 * @return 0 on success, negative error code on failure
 */
int reject_pairing(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* BLUETOOTH_H */
