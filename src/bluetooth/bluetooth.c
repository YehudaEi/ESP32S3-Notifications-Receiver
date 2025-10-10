/**
 * @file bluetooth.c
 * @brief Bluetooth Low Energy Management Implementation
 *
 * Implements BLE connectivity for receiving notifications from Android devices.
 * Compatible with Zephyr RTOS 4.1.0.
 *
 * @author Yehuda@YehudaE.net
 */

#include "bluetooth/bluetooth.h"
#include "bluetooth/pairing_screen.h"
#include "notifications/notifications.h"
#include "rtc/rtc.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(bluetooth, LOG_LEVEL_INF);

/*==============================================================================
 * CONSTANTS
 *============================================================================*/

#define DEVICE_NAME "YNotificator"
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

/* Service UUID: 12345678-1234-1234-1234-123456789abc */
#define BT_UUID_NOTIFICATION_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x1234, 0x1234, 0x123456789abc)

/* Characteristic UUID: 87654321-4321-4321-4321-cba987654321 */
#define BT_UUID_NOTIFICATION_CHAR_VAL \
    BT_UUID_128_ENCODE(0x87654321, 0x4321, 0x4321, 0x4321, 0xcba987654321)

#define BT_UUID_NOTIFICATION_SERVICE BT_UUID_DECLARE_128(BT_UUID_NOTIFICATION_SERVICE_VAL)
#define BT_UUID_NOTIFICATION_CHAR BT_UUID_DECLARE_128(BT_UUID_NOTIFICATION_CHAR_VAL)

#define MAX_MTU_SIZE 517
#define NOTIFICATION_BUFFER_SIZE 512

/*==============================================================================
 * STATIC VARIABLES
 *============================================================================*/

static struct bt_conn* current_conn = NULL;
static ble_state_t current_state = BLE_STATE_DISCONNECTED;
static bool advertising_active = false;

/* Error callbacks */
static ble_error_callback_t malformed_packet_cb = NULL;
static ble_error_callback_t connection_drop_cb = NULL;
static ble_error_callback_t buffer_overflow_cb = NULL;

/* Pairing state */
static bool pairing_in_progress = false;
static uint32_t pairing_passkey = 0;
static struct bt_conn* pairing_conn = NULL;

/* Notification buffer */
static uint8_t notification_buffer[NOTIFICATION_BUFFER_SIZE];
static size_t notification_buffer_len = 0;

/*==============================================================================
 * FORWARD DECLARATIONS
 *============================================================================*/

static void connected(struct bt_conn* conn, uint8_t err);
static void disconnected(struct bt_conn* conn, uint8_t reason);
static void security_changed(struct bt_conn* conn, bt_security_t level, enum bt_security_err err);
static void parse_notification_packet(const uint8_t* data, size_t len);
static ssize_t on_notification_write(struct bt_conn* conn, const struct bt_gatt_attr* attr,
    const void* buf, uint16_t len, uint16_t offset, uint8_t flags);

/*==============================================================================
 * ADVERTISING DATA
 *============================================================================*/

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NOTIFICATION_SERVICE_VAL),
};

/*==============================================================================
 * CONNECTION CALLBACKS
 *============================================================================*/

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
    .security_changed = security_changed,
};

/*==============================================================================
 * GATT SERVICE DEFINITION
 *============================================================================*/

BT_GATT_SERVICE_DEFINE(notification_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_NOTIFICATION_SERVICE),
    BT_GATT_CHARACTERISTIC(BT_UUID_NOTIFICATION_CHAR,
        BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
        BT_GATT_PERM_WRITE_ENCRYPT,
        NULL, on_notification_write, NULL), );

/*==============================================================================
 * PAIRING/SECURITY CALLBACKS
 *============================================================================*/

static void auth_passkey_display(struct bt_conn* conn, unsigned int passkey)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Passkey for %s: %06u", addr, passkey);

    pairing_in_progress = true;
    pairing_passkey = passkey;
    pairing_conn = conn;

    /* Show pairing screen */
    show_pairing_screen(passkey);
}

static void auth_passkey_confirm(struct bt_conn* conn, unsigned int passkey)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Confirm passkey for %s: %06u", addr, passkey);

    pairing_in_progress = true;
    pairing_passkey = passkey;
    pairing_conn = conn;

    /* Show pairing confirmation screen */
    show_pairing_screen(passkey);
}

static void auth_cancel(struct bt_conn* conn)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Pairing cancelled: %s", addr);

    pairing_in_progress = false;
    pairing_passkey = 0;
    pairing_conn = NULL;

    /* Hide pairing screen */
    hide_pairing_screen();
}

static void pairing_complete(struct bt_conn* conn, bool bonded)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Pairing completed: %s, bonded: %d", addr, bonded);

    pairing_in_progress = false;
    pairing_passkey = 0;
    pairing_conn = NULL;
    current_state = BLE_STATE_PAIRED;

    /* Hide pairing screen */
    hide_pairing_screen();

    /* Update connection status to paired */
    notifications_update_connection_status(CONN_CONNECTED);
}

static void pairing_failed(struct bt_conn* conn, enum bt_security_err reason)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_WRN("Pairing failed: %s, reason: %d", addr, reason);

    pairing_in_progress = false;
    pairing_passkey = 0;
    pairing_conn = NULL;

    /* Hide pairing screen */
    hide_pairing_screen();
}

static struct bt_conn_auth_cb auth_cb_display = {
    .passkey_display = auth_passkey_display,
    .passkey_confirm = auth_passkey_confirm,
    .cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb auth_info_cb = {
    .pairing_complete = pairing_complete,
    .pairing_failed = pairing_failed,
};

/*==============================================================================
 * CONNECTION MANAGEMENT
 *============================================================================*/

static void connected(struct bt_conn* conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (err) {
        LOG_ERR("Connection failed: %s (err %u)", addr, err);
        current_state = BLE_STATE_DISCONNECTED;
        notifications_update_connection_status(CONN_DISCONNECTED);
        start_advertising();
        return;
    }

    LOG_INF("Connected: %s", addr);
    current_conn = bt_conn_ref(conn);
    current_state = BLE_STATE_CONNECTED;
    advertising_active = false;

    /* Update connection status */
    notifications_update_connection_status(CONN_CONNECTING);

    /* MTU exchange is automatic in Zephyr 4.x when CONFIG_BT_L2CAP_TX_MTU is set */
    LOG_DBG("MTU will be negotiated automatically (configured: %d)", MAX_MTU_SIZE);

    /* Set connection to encryption required */
    int ret = bt_conn_set_security(conn, BT_SECURITY_L2);
    if (ret) {
        LOG_ERR("Failed to set security level (err %d)", ret);
    }
}

static void disconnected(struct bt_conn* conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Disconnected: %s (reason %u)", addr, reason);

    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }

    current_state = BLE_STATE_DISCONNECTED;
    pairing_in_progress = false;
    pairing_passkey = 0;
    pairing_conn = NULL;

    /* Update connection status */
    notifications_update_connection_status(CONN_DISCONNECTED);

    /* Hide pairing screen if shown */
    hide_pairing_screen();

    /* Call connection drop callback */
    if (connection_drop_cb) {
        connection_drop_cb("Connection dropped");
    }

    /* Restart advertising */
    start_advertising();
}

static void security_changed(struct bt_conn* conn, bt_security_t level, enum bt_security_err err)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (!err) {
        LOG_INF("Security changed: %s level %u", addr, level);
        if (level >= BT_SECURITY_L2) {
            current_state = BLE_STATE_PAIRED;
            notifications_update_connection_status(CONN_CONNECTED);
        }
    } else {
        LOG_ERR("Security failed: %s level %u err %u", addr, level, err);
    }
}

/*==============================================================================
 * GATT CALLBACKS
 *============================================================================*/

static ssize_t on_notification_write(struct bt_conn* conn, const struct bt_gatt_attr* attr,
    const void* buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    LOG_DBG("Notification received: %u bytes, offset: %u", len, offset);

    /* Check for buffer overflow */
    if (offset + len > NOTIFICATION_BUFFER_SIZE) {
        LOG_ERR("Buffer overflow: offset=%u, len=%u, max=%u", offset, len, NOTIFICATION_BUFFER_SIZE);
        if (buffer_overflow_cb) {
            buffer_overflow_cb("Notification buffer overflow");
        }
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    /* Copy data to buffer */
    memcpy(notification_buffer + offset, buf, len);
    notification_buffer_len = offset + len;

    /* If this is the final write (no offset), parse the packet */
    if (offset == 0) {
        parse_notification_packet(notification_buffer, notification_buffer_len);
        notification_buffer_len = 0;
    }

    return len;
}

/*==============================================================================
 * PACKET PARSING
 *============================================================================*/

static void parse_notification_packet(const uint8_t* data, size_t len)
{
    if (len < 1) {
        LOG_ERR("Malformed packet: too short (%zu bytes)", len);
        if (malformed_packet_cb) {
            malformed_packet_cb("Packet too short");
        }
        return;
    }

    ble_command_t cmd = (ble_command_t)data[0];

    switch (cmd) {
    case CMD_TIME_SYNC: {
        if (len < 5) {
            LOG_ERR("Time sync packet too short: %zu bytes", len);
            if (malformed_packet_cb) {
                malformed_packet_cb("Invalid time sync packet");
            }
            return;
        }

        /* Extract Unix timestamp (4 bytes, little-endian) */
        uint32_t timestamp = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);

        LOG_INF("Received time sync: timestamp=%u", timestamp);

        /* Set RTC time */
        int ret = ENR_rtc_set_time(timestamp);
        if (ret < 0) {
            LOG_ERR("Failed to set RTC time (ret: %d)", ret);
        } else {
            LOG_INF("RTC time synchronized successfully");

            /* Update display time */
            char time_str[16];
            if (rtc_format_time(time_str, sizeof(time_str)) == 0) {
                notifications_update_time(time_str);
            }
        }
        break;
    }

    case CMD_ADD_NOTIFICATION: {
        if (len < 9) {
            LOG_ERR("Notification packet too short: %zu bytes", len);
            if (malformed_packet_cb) {
                malformed_packet_cb("Packet too short");
            }
            return;
        }

        notification_type_t type = (notification_type_t)data[1];
        uint8_t app_len = data[2];
        uint8_t title_len = data[3];
        uint8_t text_len = data[4];

        LOG_DBG("Command: %d, Type: %d, Lengths: [%u, %u, %u]", cmd, type, app_len, title_len, text_len);

        /* Extract timestamp (4 bytes, little-endian) */
        size_t offset = 5;
        uint32_t timestamp = data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) | (data[offset + 3] << 24);
        offset += 4;

        /* Validate lengths */
        if (offset + app_len + title_len + text_len > len) {
            LOG_ERR("Malformed packet: invalid lengths");
            if (malformed_packet_cb) {
                malformed_packet_cb("Invalid packet lengths");
            }
            return;
        }

        char app_name[32] = { 0 };
        char title[64] = { 0 };
        char text[256] = { 0 };

        /* Extract strings */
        if (app_len > 0) {
            memcpy(app_name, data + offset, app_len < 31 ? app_len : 31);
            offset += app_len;
        }
        if (title_len > 0) {
            memcpy(title, data + offset, title_len < 63 ? title_len : 63);
            offset += title_len;
        }
        if (text_len > 0) {
            memcpy(text, data + offset, text_len < 255 ? text_len : 255);
        }

        LOG_INF("Notification: %s - %s (timestamp: %u)", app_name, title, timestamp);

        /* First update connection status to show activity */
        notifications_update_connection_status(CONN_CONNECTED);

        /* Add notification with timestamp */
        notifications_add_notification_with_timestamp(app_name, title, text, timestamp);
        break;
    }

    case CMD_CLEAR_ALL:
        LOG_INF("Clear all notifications");
        notifications_clear_all();
        break;

    case CMD_REMOVE_NOTIFICATION:
    case CMD_ACTION:
        LOG_DBG("Command %d not yet implemented", cmd);
        break;

    default:
        LOG_WRN("Unknown command: %d", cmd);
        if (malformed_packet_cb) {
            malformed_packet_cb("Unknown command");
        }
        break;
    }
}

/*==============================================================================
 * PUBLIC FUNCTIONS
 *============================================================================*/

int init_bluetooth(void)
{
    int ret;

    LOG_INF("Initializing Bluetooth subsystem...");

    /* Enable Bluetooth */
    ret = bt_enable(NULL);
    if (ret) {
        LOG_ERR("Bluetooth init failed (err %d)", ret);
        return ret;
    }
    LOG_INF("Bluetooth initialized");

    /* Load settings (for bonding) */
    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
        LOG_DBG("Settings loaded");
    }

    /* Register authentication callbacks */
    ret = bt_conn_auth_cb_register(&auth_cb_display);
    if (ret) {
        LOG_ERR("Failed to register auth callbacks (err %d)", ret);
        return ret;
    }

    ret = bt_conn_auth_info_cb_register(&auth_info_cb);
    if (ret) {
        LOG_ERR("Failed to register auth info callbacks (err %d)", ret);
        return ret;
    }

    LOG_INF("Authentication callbacks registered");

    /* Start advertising */
    ret = start_advertising();
    if (ret) {
        LOG_ERR("Failed to start advertising (err %d)", ret);
        return ret;
    }

    LOG_INF("Bluetooth subsystem ready");
    return 0;
}

int start_advertising(void)
{
    int ret;

    if (advertising_active) {
        LOG_DBG("Advertising already active");
        return 0;
    }

    LOG_INF("Starting advertising as '%s'", DEVICE_NAME);

    /* Use BT_LE_ADV_CONN_FAST which is the non-deprecated macro */
    ret = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (ret) {
        LOG_ERR("Advertising failed to start (err %d)", ret);
        return ret;
    }

    advertising_active = true;
    current_state = BLE_STATE_ADVERTISING;
    notifications_update_connection_status(CONN_DISCONNECTED);

    LOG_INF("Advertising started");
    return 0;
}

int stop_advertising(void)
{
    int ret;

    if (!advertising_active) {
        LOG_DBG("Advertising not active");
        return 0;
    }

    LOG_INF("Stopping advertising");

    ret = bt_le_adv_stop();
    if (ret) {
        LOG_ERR("Failed to stop advertising (err %d)", ret);
        return ret;
    }

    advertising_active = false;
    if (current_state == BLE_STATE_ADVERTISING) {
        current_state = BLE_STATE_DISCONNECTED;
    }

    LOG_INF("Advertising stopped");
    return 0;
}

ble_state_t get_ble_state(void)
{
    return current_state;
}

int get_connected_device_address(char* addr, size_t len)
{
    if (!current_conn || len < BT_ADDR_LE_STR_LEN) {
        return -EINVAL;
    }

    bt_addr_le_to_str(bt_conn_get_dst(current_conn), addr, len);
    return 0;
}

int disconnect_ble(void)
{
    if (!current_conn) {
        LOG_WRN("No active connection to disconnect");
        return -ENOTCONN;
    }

    LOG_INF("Disconnecting BLE connection");
    return bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

void register_malformed_packet_callback(ble_error_callback_t callback)
{
    malformed_packet_cb = callback;
}

void register_connection_drop_callback(ble_error_callback_t callback)
{
    connection_drop_cb = callback;
}

void register_buffer_overflow_callback(ble_error_callback_t callback)
{
    buffer_overflow_cb = callback;
}

bool is_pairing_in_progress(void)
{
    return pairing_in_progress;
}

int get_pairing_code(char* code, size_t len)
{
    if (!pairing_in_progress || len < 7) {
        return -EINVAL;
    }

    snprintf(code, len, "%06u", pairing_passkey);
    return 0;
}

int confirm_pairing(void)
{
    if (!pairing_in_progress || !pairing_conn) {
        return -EINVAL;
    }

    LOG_INF("Pairing confirmed by user");
    bt_conn_auth_passkey_confirm(pairing_conn);
    return 0;
}

int reject_pairing(void)
{
    if (!pairing_in_progress || !pairing_conn) {
        return -EINVAL;
    }

    LOG_INF("Pairing rejected by user");
    bt_conn_auth_cancel(pairing_conn);
    return 0;
}
