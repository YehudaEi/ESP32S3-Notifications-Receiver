/**
 * @file bluetooth.c
 * @brief Bluetooth Low Energy GATT Server Implementation
 *
 * This module implements a comprehensive BLE GATT server for receiving
 * notifications from Android devices and integrating with the notification UI.
 *
 * Protocol Details:
 * - Service UUID: 12345678-1234-1234-1234-123456789abc
 * - Characteristic UUID: 87654321-4321-4321-4321-cba987654321
 * - Packet Format: [CMD][TYPE][APP_LEN][TITLE_LEN][TEXT_LEN][DATA...]
 *
 * @author Yehuda@YehudaE.net
 */

#include "bluetooth/bluetooth.h"
#include "notifications/notifications.h"
#include "pairing/pairing_screen.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bluetooth, LOG_LEVEL_INF);

/*==============================================================================
 * CONSTANTS AND UUIDS
 *============================================================================*/

/* Service and Characteristic UUIDs - Must match Android app exactly */
#define NOTIFICATION_SERVICE_UUID_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x1234, 0x1234, 0x123456789abc)

#define NOTIFICATION_CHAR_UUID_VAL \
    BT_UUID_128_ENCODE(0x87654321, 0x4321, 0x4321, 0x4321, 0xcba987654321)

static struct bt_uuid_128 notification_service_uuid = BT_UUID_INIT_128(NOTIFICATION_SERVICE_UUID_VAL);
static struct bt_uuid_128 notification_char_uuid = BT_UUID_INIT_128(NOTIFICATION_CHAR_UUID_VAL);

/* Protocol constants */
#define MAX_NOTIFICATION_DATA_SIZE 512
#define MIN_PACKET_SIZE 5
#define DEFAULT_MTU_SIZE 23
#define PREFERRED_MTU_SIZE 185 /* ESP32-S3 safe MTU size */

/* Connection parameter constants (ESP32-S3 compatible) */
#define MIN_CONN_INTERVAL 6 /* 7.5ms */
#define MAX_CONN_INTERVAL 24 /* 30ms */
#define CONN_LATENCY 0
#define CONN_TIMEOUT 400 /* 4000ms */

/*==============================================================================
 * STATIC VARIABLES
 *============================================================================*/

static ble_config_t ble_config = { 0 };
static ble_connection_status_t current_status = BLE_STATUS_DISCONNECTED;
static ble_connection_stats_t connection_stats = { 0 };
static struct bt_conn* current_connection = NULL;
static uint16_t current_mtu = DEFAULT_MTU_SIZE;
static bool is_initialized = false;
static bool is_advertising = false;

/* Buffer for incoming notification data */
static uint8_t notification_buffer[MAX_NOTIFICATION_DATA_SIZE];
static size_t buffer_length = 0;

/* Timing for connection statistics */
static int64_t connection_start_time = 0;

/* Pairing state management */
static struct bt_conn* pairing_connection = NULL;
static uint32_t current_passkey = 0;
static bool pairing_in_progress = false;

/*==============================================================================
 * FORWARD DECLARATIONS
 *============================================================================*/

static void update_connection_status(ble_connection_status_t new_status);
static int parse_notification_packet(const uint8_t* data, size_t length);
static void handle_add_notification(const uint8_t* data, size_t length);
static void handle_clear_all_notifications(void);
static const char* get_app_name_from_type(ble_notification_type_t type);
static const char* status_to_string(ble_connection_status_t status);

/* BLE Pairing callbacks */
static void pairing_result_callback(pairing_action_t action, void* user_data);
static void auth_passkey_display(struct bt_conn* conn, unsigned int passkey);
static void auth_cancel(struct bt_conn* conn);

/*==============================================================================
 * BLE GATT SERVICE IMPLEMENTATION
 *============================================================================*/

/**
 * @brief GATT characteristic write callback
 */
static ssize_t notification_char_write(struct bt_conn* conn,
    const struct bt_gatt_attr* attr,
    const void* buf, uint16_t len,
    uint16_t offset, uint8_t flags)
{
    const uint8_t* data = (const uint8_t*)buf;

    if (ble_config.enable_debug_logs) {
        LOG_DBG("Received GATT write: len=%u, offset=%u, flags=0x%02x", len, offset, flags);
        LOG_HEXDUMP_DBG(data, len, "Raw data:");
    }

    if (len < MIN_PACKET_SIZE) {
        LOG_WRN("Packet too short: %u bytes (minimum %u)", len, MIN_PACKET_SIZE);
        connection_stats.parse_errors++;
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    if (offset != 0) {
        LOG_WRN("Non-zero offset not supported: %u", offset);
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    /* Copy data to buffer for processing */
    memcpy(notification_buffer, data, MIN(len, sizeof(notification_buffer)));
    buffer_length = MIN(len, sizeof(notification_buffer));

    /* Parse and process the notification */
    int ret = parse_notification_packet(notification_buffer, buffer_length);
    if (ret < 0) {
        LOG_ERR("Failed to parse notification packet: %d", ret);
        connection_stats.parse_errors++;
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    connection_stats.notifications_received++;
    LOG_INF("Successfully processed notification #%u", connection_stats.notifications_received);

    return len;
}

/**
 * @brief GATT Service Definition
 */
BT_GATT_SERVICE_DEFINE(notification_service,
    BT_GATT_PRIMARY_SERVICE(&notification_service_uuid),
    BT_GATT_CHARACTERISTIC(&notification_char_uuid.uuid,
        BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
        BT_GATT_PERM_WRITE,
        NULL, notification_char_write, NULL), );

/*==============================================================================
 * BLE AUTHENTICATION CALLBACKS
 *============================================================================*/

/**
 * @brief Pairing result callback from pairing screen
 */
static void pairing_result_callback(pairing_action_t action, void* user_data)
{
    LOG_INF("Pairing result: %d", action);

    if (!pairing_connection) {
        LOG_ERR("No pairing connection available");
        return;
    }

    switch (action) {
    case PAIRING_ACTION_ACCEPT:
        LOG_INF("User accepted pairing");
        /* Pairing will continue automatically */
        break;

    case PAIRING_ACTION_CANCEL:
    case PAIRING_ACTION_TIMEOUT:
        LOG_INF("User cancelled or timed out pairing");
        bt_conn_disconnect(pairing_connection, BT_HCI_ERR_AUTH_FAIL);
        break;
    }

    /* Clean up pairing state */
    if (pairing_connection) {
        bt_conn_unref(pairing_connection);
        pairing_connection = NULL;
    }
    pairing_in_progress = false;
    current_passkey = 0;
}

/**
 * @brief Display passkey for pairing
 */
static void auth_passkey_display(struct bt_conn* conn, unsigned int passkey)
{
    char addr[18];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Passkey display request from %s: %06u", addr, passkey);

    /* Store pairing state */
    if (pairing_connection) {
        bt_conn_unref(pairing_connection);
    }
    pairing_connection = bt_conn_ref(conn);
    current_passkey = passkey;
    pairing_in_progress = true;

    /* Prepare pairing request info */
    pairing_request_info_t request_info = {
        .passkey = passkey,
        .timeout_seconds = 30,
        .callback = pairing_result_callback,
        .user_data = NULL,
    };

    /* Set device name (try to get it, fallback to "Unknown Device") */
    strncpy(request_info.device_name, "Android Device", sizeof(request_info.device_name) - 1);
    strncpy(request_info.device_address, addr, sizeof(request_info.device_address) - 1);

    /* Show pairing screen */
    int ret = pairing_screen_show(&request_info);
    if (ret != 0) {
        LOG_ERR("Failed to show pairing screen: %d", ret);
        /* Continue without UI - pairing will timeout if not confirmed */
    }
}

/**
 * @brief Cancel pairing authentication
 */
static void auth_cancel(struct bt_conn* conn)
{
    char addr[18];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Pairing cancelled by %s", addr);

    /* Hide pairing screen if active */
    if (pairing_screen_is_active()) {
        pairing_screen_hide();
    }

    /* Clean up pairing state */
    if (pairing_connection) {
        bt_conn_unref(pairing_connection);
        pairing_connection = NULL;
    }
    pairing_in_progress = false;
    current_passkey = 0;
}

/**
 * @brief BLE authentication callback structure (ESP32-S3 compatible)
 */
static struct bt_conn_auth_cb auth_cb_display = {
    .passkey_display = auth_passkey_display,
    .cancel = auth_cancel,
};

/*==============================================================================
 * BLE CONNECTION CALLBACKS
 *============================================================================*/

/**
 * @brief Connection parameter update callback
 */
static void conn_le_param_updated(struct bt_conn* conn, uint16_t interval,
    uint16_t latency, uint16_t timeout)
{
    LOG_INF("Connection params updated: interval=%u, latency=%u, timeout=%u",
        interval, latency, timeout);
}

/**
 * @brief Connection event callback
 */
static void connected(struct bt_conn* conn, uint8_t err)
{
    char addr[18];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (err) {
        LOG_ERR("Failed to connect to %s (err %u)", addr, err);
        update_connection_status(BLE_STATUS_ADVERTISING);
        return;
    }

    LOG_INF("Connected to %s", addr);

    /* Store connection reference */
    current_connection = bt_conn_ref(conn);
    connection_start_time = k_uptime_get();
    connection_stats.connection_count++;
    connection_stats.successful_connections++;
    connection_stats.last_connection_time = k_uptime_get();

    update_connection_status(BLE_STATUS_CONNECTED);

    /* Stop advertising when connected */
    if (is_advertising) {
        ble_stop_advertising();
    }

    /* Start pairing process - this will trigger passkey display */
    int ret = bt_conn_set_security(conn, BT_SECURITY_L2);
    if (ret) {
        LOG_ERR("Failed to set security level: %d", ret);
        /* Continue without pairing - some devices might work */
        update_connection_status(BLE_STATUS_READY);
    } else {
        LOG_DBG("Security upgrade initiated - waiting for pairing");
        update_connection_status(BLE_STATUS_CONNECTING);
    }
}

/**
 * @brief Security changed callback - handles pairing completion
 */
static void security_changed(struct bt_conn* conn, bt_security_t level,
    enum bt_security_err err)
{
    char addr[18];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (err) {
        LOG_ERR("Security failed: %s level %u err %d", addr, level, err);

        /* Hide pairing screen if active */
        if (pairing_screen_is_active()) {
            pairing_screen_hide();
        }

        /* Clean up pairing state */
        if (pairing_connection) {
            bt_conn_unref(pairing_connection);
            pairing_connection = NULL;
        }
        pairing_in_progress = false;
        current_passkey = 0;

        /* Disconnect on security failure */
        bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
        return;
    }

    LOG_INF("Security changed: %s level %u", addr, level);

    if (level >= BT_SECURITY_L2) {
        LOG_INF("Pairing completed successfully with %s", addr);

        /* Hide pairing screen */
        if (pairing_screen_is_active()) {
            pairing_screen_hide();
        }

        /* Clean up pairing state */
        if (pairing_connection) {
            bt_conn_unref(pairing_connection);
            pairing_connection = NULL;
        }
        pairing_in_progress = false;
        current_passkey = 0;

        /* Connection is now ready */
        update_connection_status(BLE_STATUS_READY);
    }
}

/**
 * @brief Disconnection event callback
 */
static void disconnected(struct bt_conn* conn, uint8_t reason)
{
    char addr[18];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Disconnected from %s (reason %u)", addr, reason);

    /* Update connection statistics */
    if (connection_start_time > 0) {
        connection_stats.total_connected_time += (k_uptime_get() - connection_start_time);
        connection_start_time = 0;
    }

    /* Clean up connection reference */
    if (current_connection) {
        bt_conn_unref(current_connection);
        current_connection = NULL;
    }

    current_mtu = DEFAULT_MTU_SIZE;
    update_connection_status(BLE_STATUS_DISCONNECTED);

    /* Restart advertising */
    int ret = ble_start_advertising();
    if (ret) {
        LOG_ERR("Failed to restart advertising: %d", ret);
    }
}

/**
 * @brief MTU exchange callback
 */
static void mtu_updated(struct bt_conn* conn, uint16_t tx, uint16_t rx)
{
    current_mtu = MIN(tx, rx);
    connection_stats.current_mtu = current_mtu;

    LOG_INF("MTU updated: TX=%u, RX=%u, effective=%u", tx, rx, current_mtu);

    /* Connection is now ready for data transfer */
    update_connection_status(BLE_STATUS_READY);
}

/**
 * @brief GATT callback structure
 */
static struct bt_gatt_cb gatt_callbacks = {
    .att_mtu_updated = mtu_updated,
};

/**
 * @brief BLE connection callback structure
 */
static struct bt_conn_cb connection_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
    .le_param_updated = conn_le_param_updated,
    .security_changed = security_changed,
};

/*==============================================================================
 * ADVERTISING CONFIGURATION
 *============================================================================*/

/**
 * @brief Ultra-minimal advertising parameters for ESP32-S3
 */
static const struct bt_le_adv_param adv_param = {
    .id = BT_ID_DEFAULT,
    .options = BT_LE_ADV_OPT_CONN, /* Try the non-deprecated connectable option */
    .interval_min = 160, /* 100ms in 0.625ms units */
    .interval_max = 160, /* 100ms in 0.625ms units */
    .peer = NULL,
};

/**
 * @brief Minimal advertising data for ESP32-S3
 */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

/**
 * @brief Scan response data with device info
 */
static struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, "ZephyrWatch", 11), /* Fixed name to avoid CONFIG issues */
};

/*==============================================================================
 * NOTIFICATION PARSING AND PROCESSING
 *============================================================================*/

/**
 * @brief Parse incoming notification packet
 */
static int parse_notification_packet(const uint8_t* data, size_t length)
{
    if (!data || length < MIN_PACKET_SIZE) {
        return -EINVAL;
    }

    ble_notification_cmd_t cmd = (ble_notification_cmd_t)data[0];

    LOG_DBG("Parsing packet: cmd=0x%02x, length=%zu", cmd, length);

    switch (cmd) {
    case BLE_CMD_ADD_NOTIFICATION:
        handle_add_notification(data, length);
        break;

    case BLE_CMD_CLEAR_ALL:
        handle_clear_all_notifications();
        break;

    case BLE_CMD_REMOVE_NOTIFICATION:
        LOG_DBG("Remove notification command received (not implemented)");
        break;

    case BLE_CMD_ACTION:
        LOG_DBG("Action command received (not implemented)");
        break;

    default:
        LOG_WRN("Unknown command: 0x%02x", cmd);
        return -ENOTSUP;
    }

    return 0;
}

/**
 * @brief Handle add notification command
 */
static void handle_add_notification(const uint8_t* data, size_t length)
{
    if (length < MIN_PACKET_SIZE) {
        LOG_ERR("Add notification packet too short");
        return;
    }

    ble_notification_type_t type = (ble_notification_type_t)data[1];
    uint8_t app_len = data[2];
    uint8_t title_len = data[3];
    uint8_t text_len = data[4];

    /* Validate packet size */
    size_t expected_size = MIN_PACKET_SIZE + app_len + title_len + text_len;
    if (length < expected_size) {
        LOG_ERR("Packet size mismatch: expected %zu, got %zu", expected_size, length);
        return;
    }

    /* Extract data fields */
    ble_notification_data_t notification = { 0 };
    notification.type = type;
    notification.is_priority = (type == BLE_TYPE_PHONE || type == BLE_TYPE_MESSAGE);

    size_t offset = MIN_PACKET_SIZE;

    /* Extract app name */
    if (app_len > 0 && offset + app_len <= length) {
        size_t copy_len = MIN(app_len, sizeof(notification.app_name) - 1);
        memcpy(notification.app_name, &data[offset], copy_len);
        notification.app_name[copy_len] = '\0';
        offset += app_len;
    } else {
        strncpy(notification.app_name, get_app_name_from_type(type), sizeof(notification.app_name) - 1);
    }

    /* Extract title */
    if (title_len > 0 && offset + title_len <= length) {
        size_t copy_len = MIN(title_len, sizeof(notification.title) - 1);
        memcpy(notification.title, &data[offset], copy_len);
        notification.title[copy_len] = '\0';
        offset += title_len;
    }

    /* Extract text */
    if (text_len > 0 && offset + text_len <= length) {
        size_t copy_len = MIN(text_len, sizeof(notification.text) - 1);
        memcpy(notification.text, &data[offset], copy_len);
        notification.text[copy_len] = '\0';
    }

    LOG_INF("New notification: [%s] %s - %s",
        notification.app_name, notification.title, notification.text);

    /* Generate timestamp */
    char timestamp[16];
    int64_t uptime_ms = k_uptime_get();
    int hours = (uptime_ms / 3600000) % 24;
    int minutes = (uptime_ms / 60000) % 60;
    snprintf(timestamp, sizeof(timestamp), "%02d:%02d", hours, minutes);

    /* Add notification to the notification system */
    notifications_add_notification(notification.app_name,
        notification.title,
        notification.text,
        timestamp);

    /* Call user callback if registered */
    if (ble_config.notification_callback) {
        ble_config.notification_callback(&notification, ble_config.user_data);
    }
}

/**
 * @brief Handle clear all notifications command
 */
static void handle_clear_all_notifications(void)
{
    LOG_INF("Clearing all notifications");
    notifications_clear_all();
}

/**
 * @brief Get default app name based on notification type
 */
static const char* get_app_name_from_type(ble_notification_type_t type)
{
    switch (type) {
    case BLE_TYPE_PHONE:
        return "Phone";
    case BLE_TYPE_MESSAGE:
        return "Messages";
    case BLE_TYPE_EMAIL:
        return "Email";
    case BLE_TYPE_SOCIAL:
        return "Social";
    case BLE_TYPE_CALENDAR:
        return "Calendar";
    default:
        return "Unknown";
    }
}

/*==============================================================================
 * STATUS MANAGEMENT
 *============================================================================*/

/**
 * @brief Update connection status and notify callbacks
 */
static void update_connection_status(ble_connection_status_t new_status)
{
    if (current_status == new_status) {
        return;
    }

    ble_connection_status_t old_status = current_status;
    current_status = new_status;

    LOG_INF("BLE status changed: %s -> %s",
        status_to_string(old_status), status_to_string(new_status));

    /* Update notification UI */
    connection_status_t ui_status;
    switch (new_status) {
    case BLE_STATUS_READY:
    case BLE_STATUS_CONNECTED:
        ui_status = CONN_CONNECTED;
        break;
    case BLE_STATUS_CONNECTING:
        ui_status = CONN_CONNECTING;
        break;
    case BLE_STATUS_ADVERTISING:
        ui_status = CONN_WEAK_SIGNAL;
        break;
    default:
        ui_status = CONN_DISCONNECTED;
        break;
    }
    notifications_update_connection_status(ui_status);

    /* Call user callback if registered */
    if (ble_config.status_callback) {
        ble_config.status_callback(new_status, ble_config.user_data);
    }
}

/**
 * @brief Convert status enum to string
 */
static const char* status_to_string(ble_connection_status_t status)
{
    switch (status) {
    case BLE_STATUS_DISCONNECTED:
        return "DISCONNECTED";
    case BLE_STATUS_ADVERTISING:
        return "ADVERTISING";
    case BLE_STATUS_CONNECTING:
        return "CONNECTING";
    case BLE_STATUS_CONNECTED:
        return "CONNECTED";
    case BLE_STATUS_READY:
        return "READY";
    default:
        return "UNKNOWN";
    }
}

/*==============================================================================
 * PUBLIC API IMPLEMENTATION
 *============================================================================*/

int ble_init_notification_server(const ble_config_t* config)
{
    int ret;

    if (!config) {
        LOG_ERR("Invalid configuration");
        return -EINVAL;
    }

    if (is_initialized) {
        LOG_WRN("BLE server already initialized");
        return 0;
    }

    LOG_INF("Initializing BLE notification server");

    /* Store configuration */
    memcpy(&ble_config, config, sizeof(ble_config_t));

    /* Initialize statistics */
    memset(&connection_stats, 0, sizeof(connection_stats));
    connection_stats.current_mtu = DEFAULT_MTU_SIZE;
    current_mtu = DEFAULT_MTU_SIZE;

    /* Register callbacks first, before enabling BLE */
    bt_conn_cb_register(&connection_callbacks);
    bt_gatt_cb_register(&gatt_callbacks);

    /* Register authentication callbacks for pairing */
    ret = bt_conn_auth_cb_register(&auth_cb_display);
    if (ret) {
        LOG_ERR("Failed to register authentication callbacks: %d", ret);
        return ret;
    }
    LOG_INF("BLE authentication callbacks registered");

    /* Initialize pairing screen */
    ret = pairing_screen_init();
    if (ret) {
        LOG_ERR("Failed to initialize pairing screen: %d", ret);
        return ret;
    }

    /* Enable Bluetooth */
    ret = bt_enable(NULL);
    if (ret) {
        LOG_ERR("Failed to enable Bluetooth: %d", ret);
        return ret;
    }

    LOG_INF("Bluetooth enabled successfully");

    /* Wait longer for ESP32-S3 BLE stack to be fully ready */
    k_sleep(K_MSEC(500));

    is_initialized = true;
    update_connection_status(BLE_STATUS_DISCONNECTED);

    LOG_DBG("Attempting to start advertising after BLE initialization...");

    /* Start advertising */
    ret = ble_start_advertising();
    if (ret) {
        LOG_ERR("Failed to start initial advertising: %d", ret);
        /* Don't fail completely - BLE is initialized, just not advertising */
        LOG_WRN("BLE initialized but not advertising, continuing...");
        return 0; /* Return success so the app can continue */
    }

    LOG_INF("BLE notification server initialized successfully");
    return 0;
}

int ble_start_advertising(void)
{
    int ret;
    int retry_count = 0;
    const int max_retries = 3;

    if (!is_initialized) {
        LOG_ERR("BLE not initialized");
        return -EINVAL;
    }

    if (is_advertising) {
        LOG_DBG("Already advertising");
        return 0;
    }

    LOG_INF("Starting BLE advertising");
    LOG_DBG("Advertising params: id=%d, options=0x%02x, interval_min=%d, interval_max=%d",
        adv_param.id, adv_param.options, adv_param.interval_min, adv_param.interval_max);
    LOG_DBG("AD data entries: %d, SD data entries: %d", ARRAY_SIZE(ad), ARRAY_SIZE(sd));

    while (retry_count < max_retries) {
        /* Try with device name in scan response */
        ret = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
        if (ret == 0) {
            LOG_INF("Started advertising with device name (attempt %d)", retry_count + 1);
            break;
        }

        LOG_WRN("Failed to start advertising with scan response: %d (attempt %d)", ret, retry_count + 1);

        /* Fallback: try with no scan response */
        ret = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
        if (ret == 0) {
            LOG_INF("Started advertising without scan response (attempt %d)", retry_count + 1);
            break;
        }

        LOG_WRN("Failed to start basic advertising: %d (attempt %d)", ret, retry_count + 1);

        retry_count++;
        if (retry_count < max_retries) {
            LOG_INF("Retrying advertising in 100ms...");
            k_sleep(K_MSEC(100));
        }
    }

    if (ret != 0) {
        LOG_ERR("Failed to start advertising after %d attempts, error: %d", max_retries, ret);
        LOG_ERR("This might indicate ESP32-S3 BLE stack issues or configuration problems");
        return ret;
    }

    is_advertising = true;
    update_connection_status(BLE_STATUS_ADVERTISING);

    LOG_INF("BLE advertising started successfully");
    return 0;
}

int ble_stop_advertising(void)
{
    int ret;

    if (!is_advertising) {
        return 0;
    }

    LOG_INF("Stopping BLE advertising");

    ret = bt_le_adv_stop();
    if (ret) {
        LOG_ERR("Failed to stop advertising: %d", ret);
        return ret;
    }

    is_advertising = false;

    if (current_status == BLE_STATUS_ADVERTISING) {
        update_connection_status(BLE_STATUS_DISCONNECTED);
    }

    LOG_INF("BLE advertising stopped");
    return 0;
}

int ble_disconnect_device(void)
{
    if (!current_connection) {
        LOG_WRN("No device connected to disconnect");
        return -ENOTCONN;
    }

    LOG_INF("Disconnecting BLE device");

    int ret = bt_conn_disconnect(current_connection, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    if (ret) {
        LOG_ERR("Failed to disconnect: %d", ret);
        return ret;
    }

    return 0;
}

ble_connection_status_t ble_get_connection_status(void)
{
    return current_status;
}

int ble_get_connection_stats(ble_connection_stats_t* stats)
{
    if (!stats) {
        return -EINVAL;
    }

    memcpy(stats, &connection_stats, sizeof(ble_connection_stats_t));

    /* Update current connection time if connected */
    if (current_connection && connection_start_time > 0) {
        stats->total_connected_time += (k_uptime_get() - connection_start_time);
    }

    return 0;
}

int ble_get_connected_device_info(char* device_addr, size_t addr_size)
{
    if (!device_addr || addr_size < 18) {
        return -EINVAL;
    }

    if (!current_connection) {
        return -ENOTCONN;
    }

    bt_addr_le_to_str(bt_conn_get_dst(current_connection), device_addr, addr_size);
    return 0;
}

int ble_update_connection_params(uint16_t min_interval, uint16_t max_interval,
    uint16_t latency, uint16_t timeout)
{
    if (!current_connection) {
        return -ENOTCONN;
    }

    struct bt_le_conn_param param = {
        .interval_min = min_interval,
        .interval_max = max_interval,
        .latency = latency,
        .timeout = timeout,
    };

    return bt_conn_le_param_update(current_connection, &param);
}

int ble_send_acknowledgment(ble_notification_cmd_t cmd, uint8_t status)
{
    /* This could be implemented if needed by the protocol */
    /* For now, just return success as the Android app doesn't expect ACKs */
    return 0;
}

int ble_shutdown_notification_server(void)
{
    LOG_INF("Shutting down BLE notification server");

    if (current_connection) {
        ble_disconnect_device();
        /* Wait for disconnection to complete */
        k_sleep(K_MSEC(100));
    }

    if (is_advertising) {
        ble_stop_advertising();
    }

    /* Note: Zephyr doesn't have bt_disable() in all versions */
    /* The BLE stack will remain active but not advertising */

    is_initialized = false;
    update_connection_status(BLE_STATUS_DISCONNECTED);

    LOG_INF("BLE notification server shut down");
    return 0;
}

bool ble_is_ready(void)
{
    return is_initialized && (current_status != BLE_STATUS_DISCONNECTED);
}

uint16_t ble_get_current_mtu(void)
{
    return current_connection ? current_mtu : 0;
}

int ble_process_events(void)
{
    /* In Zephyr, BLE events are handled automatically by the BLE stack */
    /* This function is provided for API completeness but doesn't need implementation */
    return 0;
}
