/**
 * @file pairing_screen.c
 * @brief BLE Pairing Screen Implementation
 *
 * This module provides a comprehensive pairing interface for BLE connections,
 * including passkey display, user confirmation, and timeout handling.
 *
 * @author Yehuda@YehudaE.net
 */

#include "pairing/pairing_screen.h"

#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pairing_screen, LOG_LEVEL_INF);

/*==============================================================================
 * CONSTANTS AND CONFIGURATION
 *============================================================================*/

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240
#define SCREEN_RADIUS 120

/** @brief Default pairing timeout in seconds */
#define DEFAULT_PAIRING_TIMEOUT_S 30

/** @brief Passkey digit display size */
#define PASSKEY_DIGIT_SIZE 32

/*==============================================================================
 * STATIC VARIABLES
 *============================================================================*/

/* UI Objects */
static lv_obj_t* pairing_screen = NULL;
static lv_obj_t* title_label = NULL;
static lv_obj_t* device_info_label = NULL;
static lv_obj_t* passkey_container = NULL;
static lv_obj_t* passkey_digits[6] = { NULL };
static lv_obj_t* instruction_label = NULL;
static lv_obj_t* timeout_label = NULL;
static lv_obj_t* accept_button = NULL;
static lv_obj_t* cancel_button = NULL;

/* State Management */
static bool is_initialized = false;
static bool is_active = false;
static pairing_request_info_t current_request = { 0 };
static int64_t pairing_start_time = 0;
static lv_obj_t* previous_screen = NULL;

/*==============================================================================
 * FORWARD DECLARATIONS
 *============================================================================*/

static void create_pairing_screen_ui(void);
static void update_passkey_display(uint32_t passkey);
static void update_device_info(const char* name, const char* address);
static void update_timeout_display(uint32_t remaining_seconds);
static void accept_button_event_handler(lv_event_t* e);
static void cancel_button_event_handler(lv_event_t* e);
static void cleanup_pairing_request(pairing_action_t action);

/*==============================================================================
 * UI CREATION AND STYLING
 *============================================================================*/

/**
 * @brief Create the pairing screen user interface
 */
static void create_pairing_screen_ui(void)
{
    if (pairing_screen) {
        return; /* Already created */
    }

    /* Create main pairing screen */
    pairing_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(pairing_screen, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_text_color(pairing_screen, lv_color_hex(0xffffff), 0);

    /* Title */
    title_label = lv_label_create(pairing_screen);
    lv_label_set_text(title_label, "Pairing Request");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x00d4ff), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 15);

    /* Device information */
    device_info_label = lv_label_create(pairing_screen);
    lv_label_set_text(device_info_label, "Unknown Device\n00:00:00:00:00:00");
    lv_obj_set_style_text_font(device_info_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(device_info_label, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_text_align(device_info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(device_info_label, title_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    /* Passkey container */
    passkey_container = lv_obj_create(pairing_screen);
    lv_obj_set_size(passkey_container, 220, 60);
    lv_obj_set_style_bg_color(passkey_container, lv_color_hex(0x2d2d44), 0);
    lv_obj_set_style_border_color(passkey_container, lv_color_hex(0x00d4ff), 0);
    lv_obj_set_style_border_width(passkey_container, 2, 0);
    lv_obj_set_style_radius(passkey_container, 10, 0);
    lv_obj_set_style_pad_all(passkey_container, 8, 0);
    lv_obj_align(passkey_container, LV_ALIGN_CENTER, 0, -20);

    /* Create 6 digit displays */
    for (int i = 0; i < 6; i++) {
        passkey_digits[i] = lv_label_create(passkey_container);
        lv_label_set_text(passkey_digits[i], "0");
        lv_obj_set_style_text_font(passkey_digits[i], &lv_font_montserrat_46, 0);
        lv_obj_set_style_text_color(passkey_digits[i], lv_color_hex(0x00ff88), 0);

        /* Position digits in a row */
        lv_obj_align(passkey_digits[i], LV_ALIGN_CENTER, (i - 2.5f) * 32, 0);
    }

    /* Instructions */
    instruction_label = lv_label_create(pairing_screen);
    lv_label_set_text(instruction_label, "Enter this code on your device");
    lv_obj_set_style_text_font(instruction_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(instruction_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_align(instruction_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(instruction_label, passkey_container, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);

    /* Timeout display */
    timeout_label = lv_label_create(pairing_screen);
    lv_label_set_text(timeout_label, "Timeout: 30s");
    lv_obj_set_style_text_font(timeout_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(timeout_label, lv_color_hex(0xff6b6b), 0);
    lv_obj_align_to(timeout_label, instruction_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    /* Button container */
    lv_obj_t* button_container = lv_obj_create(pairing_screen);
    lv_obj_set_size(button_container, 200, 50);
    lv_obj_set_style_bg_opa(button_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(button_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(button_container, 0, 0);
    lv_obj_align(button_container, LV_ALIGN_BOTTOM_MID, 0, -10);

    /* Accept button */
    accept_button = lv_btn_create(button_container);
    lv_obj_set_size(accept_button, 85, 35);
    lv_obj_set_style_bg_color(accept_button, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_radius(accept_button, 8, 0);
    lv_obj_align(accept_button, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(accept_button, accept_button_event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_t* accept_label = lv_label_create(accept_button);
    lv_label_set_text(accept_label, "Accept");
    lv_obj_set_style_text_font(accept_label, &lv_font_montserrat_12, 0);
    lv_obj_center(accept_label);

    /* Cancel button */
    cancel_button = lv_btn_create(button_container);
    lv_obj_set_size(cancel_button, 85, 35);
    lv_obj_set_style_bg_color(cancel_button, lv_color_hex(0xf44336), 0);
    lv_obj_set_style_radius(cancel_button, 8, 0);
    lv_obj_align(cancel_button, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(cancel_button, cancel_button_event_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_t* cancel_label = lv_label_create(cancel_button);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_12, 0);
    lv_obj_center(cancel_label);

    LOG_INF("Pairing screen UI created successfully");
}

/*==============================================================================
 * UPDATE FUNCTIONS
 *============================================================================*/

/**
 * @brief Update passkey digit display
 */
static void update_passkey_display(uint32_t passkey)
{
    char digits[7];
    snprintf(digits, sizeof(digits), "%06u", passkey);

    for (int i = 0; i < 6; i++) {
        char digit_str[2] = { digits[i], '\0' };
        lv_label_set_text(passkey_digits[i], digit_str);
    }

    LOG_DBG("Updated passkey display: %06u", passkey);
}

/**
 * @brief Update device information display
 */
static void update_device_info(const char* name, const char* address)
{
    static char info_text[80];

    if (name && strlen(name) > 0) {
        snprintf(info_text, sizeof(info_text), "%s\n%s", name, address);
    } else {
        snprintf(info_text, sizeof(info_text), "Unknown Device\n%s", address);
    }

    lv_label_set_text(device_info_label, info_text);
    LOG_DBG("Updated device info: %s", info_text);
}

/**
 * @brief Update timeout countdown display
 */
static void update_timeout_display(uint32_t remaining_seconds)
{
    static char timeout_text[32]; /* Increased size to prevent truncation */

    if (remaining_seconds > 0) {
        snprintf(timeout_text, sizeof(timeout_text), "Timeout: %us", remaining_seconds);
        lv_obj_set_style_text_color(timeout_label,
            remaining_seconds <= 10 ? lv_color_hex(0xff6b6b) : lv_color_hex(0xffa726), 0);
    } else {
        snprintf(timeout_text, sizeof(timeout_text), "Timeout: Expired");
        lv_obj_set_style_text_color(timeout_label, lv_color_hex(0xff3d00), 0);
    }

    lv_label_set_text(timeout_label, timeout_text);
}

/*==============================================================================
 * EVENT HANDLERS
 *============================================================================*/

/**
 * @brief Accept button event handler
 */
static void accept_button_event_handler(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        LOG_INF("User accepted pairing");
        cleanup_pairing_request(PAIRING_ACTION_ACCEPT);
    }
}

/**
 * @brief Cancel button event handler
 */
static void cancel_button_event_handler(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        LOG_INF("User cancelled pairing");
        cleanup_pairing_request(PAIRING_ACTION_CANCEL);
    }
}

/**
 * @brief Clean up pairing request and call callback
 */
static void cleanup_pairing_request(pairing_action_t action)
{
    if (!is_active) {
        return;
    }

    LOG_INF("Cleaning up pairing request with action: %d", action);

    /* Call user callback */
    if (current_request.callback) {
        current_request.callback(action, current_request.user_data);
    }

    /* Hide pairing screen */
    pairing_screen_hide();

    /* Clear current request */
    memset(&current_request, 0, sizeof(current_request));
    is_active = false;
}

/*==============================================================================
 * PUBLIC API IMPLEMENTATION
 *============================================================================*/

int pairing_screen_init(void)
{
    if (is_initialized) {
        LOG_WRN("Pairing screen already initialized");
        return 0;
    }

    LOG_INF("Initializing pairing screen system");

    create_pairing_screen_ui();

    is_initialized = true;
    is_active = false;

    LOG_INF("Pairing screen system initialized successfully");
    return 0;
}

int pairing_screen_show(const pairing_request_info_t* request_info)
{
    if (!is_initialized) {
        LOG_ERR("Pairing screen not initialized");
        return -EINVAL;
    }

    if (!request_info) {
        LOG_ERR("Invalid request info");
        return -EINVAL;
    }

    if (is_active) {
        LOG_WRN("Pairing screen already active");
        return -EBUSY;
    }

    LOG_INF("Showing pairing screen for device: %s (%s)",
        request_info->device_name, request_info->device_address);

    /* Store current screen */
    previous_screen = lv_scr_act();

    /* Copy request information */
    memcpy(&current_request, request_info, sizeof(pairing_request_info_t));

    /* Update UI with request data */
    update_device_info(current_request.device_name, current_request.device_address);
    update_passkey_display(current_request.passkey);

    /* Set timeout */
    uint32_t timeout = current_request.timeout_seconds;
    if (timeout == 0) {
        timeout = DEFAULT_PAIRING_TIMEOUT_S;
    }
    update_timeout_display(timeout);

    /* Show the pairing screen */
    lv_scr_load_anim(pairing_screen, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);

    /* Mark as active and record start time */
    is_active = true;
    pairing_start_time = k_uptime_get();

    LOG_INF("Pairing screen displayed with passkey: %06u", current_request.passkey);
    return 0;
}

int pairing_screen_hide(void)
{
    if (!is_active) {
        LOG_DBG("Pairing screen not active");
        return 0;
    }

    LOG_INF("Hiding pairing screen");

    /* Return to previous screen */
    if (previous_screen) {
        lv_scr_load_anim(previous_screen, LV_SCR_LOAD_ANIM_FADE_OUT, 300, 0, false);
        previous_screen = NULL;
    }

    is_active = false;
    return 0;
}

bool pairing_screen_is_active(void)
{
    return is_active;
}

uint32_t pairing_screen_update_timeout(void)
{
    if (!is_active || pairing_start_time == 0) {
        return 0;
    }

    uint32_t timeout = current_request.timeout_seconds;
    if (timeout == 0) {
        timeout = DEFAULT_PAIRING_TIMEOUT_S;
    }

    int64_t elapsed_ms = k_uptime_get() - pairing_start_time;
    uint32_t elapsed_s = elapsed_ms / 1000;

    if (elapsed_s >= timeout) {
        LOG_INF("Pairing request timed out");
        cleanup_pairing_request(PAIRING_ACTION_TIMEOUT);
        return 0;
    }

    uint32_t remaining = timeout - elapsed_s;
    update_timeout_display(remaining);
    return remaining;
}

int pairing_screen_force_timeout(void)
{
    if (!is_active) {
        return -EINVAL;
    }

    LOG_INF("Forcing pairing timeout");
    cleanup_pairing_request(PAIRING_ACTION_TIMEOUT);
    return 0;
}

uint32_t pairing_screen_get_current_passkey(void)
{
    return is_active ? current_request.passkey : 0;
}
