/**
 * @file pairing_screen.c
 * @brief BLE Pairing Screen UI Implementation
 *
 * Provides LVGL-based pairing screen for BLE security.
 *
 * @author Yehuda@YehudaE.net
 */

#include "bluetooth/pairing_screen.h"
#include "bluetooth/bluetooth.h"
#include "fonts/heb_fonts.h"

#include <lvgl.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pairing_screen, LOG_LEVEL_INF);

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240

/*==============================================================================
 * STATIC VARIABLES
 *============================================================================*/

static lv_obj_t* pairing_screen = NULL;
static lv_obj_t* passkey_label = NULL;
static lv_obj_t* instruction_label = NULL;
static lv_obj_t* confirm_btn = NULL;
static lv_obj_t* reject_btn = NULL;
static bool screen_visible = false;
static lv_obj_t* previous_screen = NULL;

/*==============================================================================
 * BUTTON CALLBACKS
 *============================================================================*/

static void confirm_btn_event_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        LOG_INF("User confirmed pairing");
        confirm_pairing();
        hide_pairing_screen();
    }
}

static void reject_btn_event_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        LOG_INF("User rejected pairing");
        reject_pairing();
        hide_pairing_screen();
    }
}

/*==============================================================================
 * SCREEN CREATION
 *============================================================================*/

static void create_pairing_ui(uint32_t passkey)
{
    if (pairing_screen != NULL) {
        LOG_DBG("Pairing screen already exists, updating passkey");
        char passkey_str[32];
        snprintf(passkey_str, sizeof(passkey_str), "%06u", passkey);
        lv_label_set_text(passkey_label, passkey_str);
        return;
    }

    LOG_DBG("Creating pairing screen UI");

    /* Create screen */
    pairing_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(pairing_screen, lv_color_hex(0x1a1a1a), 0);

    /* Title label */
    lv_obj_t* title = lv_label_create(pairing_screen);
    lv_label_set_text(title, "BLE Pairing");
    lv_obj_set_style_text_font(title, &heb_font_10, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    /* Instruction label */
    instruction_label = lv_label_create(pairing_screen);
    lv_label_set_text(instruction_label, "Enter this code\non your device:");
    lv_obj_set_style_text_font(instruction_label, &heb_font_12, 0);
    lv_obj_set_style_text_color(instruction_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_align(instruction_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(instruction_label, LV_ALIGN_TOP_MID, 0, 50);

    /* Passkey display - large and prominent */
    passkey_label = lv_label_create(pairing_screen);
    char passkey_str[32];
    snprintf(passkey_str, sizeof(passkey_str), "%06u", passkey);
    lv_label_set_text(passkey_label, passkey_str);
    lv_obj_set_style_text_font(passkey_label, &heb_font_46, 0);
    lv_obj_set_style_text_color(passkey_label, lv_color_hex(0x00FF00), 0);
    lv_obj_align(passkey_label, LV_ALIGN_CENTER, 0, -10);

    /* Confirm button */
    confirm_btn = lv_btn_create(pairing_screen);
    lv_obj_set_size(confirm_btn, 90, 40);
    lv_obj_align(confirm_btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_set_style_bg_color(confirm_btn, lv_color_hex(0x00AA00), 0);
    lv_obj_add_event_cb(confirm_btn, confirm_btn_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t* confirm_label = lv_label_create(confirm_btn);
    lv_label_set_text(confirm_label, "Accept");
    lv_obj_center(confirm_label);

    /* Reject button */
    reject_btn = lv_btn_create(pairing_screen);
    lv_obj_set_size(reject_btn, 90, 40);
    lv_obj_align(reject_btn, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_set_style_bg_color(reject_btn, lv_color_hex(0xAA0000), 0);
    lv_obj_add_event_cb(reject_btn, reject_btn_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t* reject_label = lv_label_create(reject_btn);
    lv_label_set_text(reject_label, "Reject");
    lv_obj_center(reject_label);

    LOG_DBG("Pairing screen UI created");
}

/*==============================================================================
 * PUBLIC FUNCTIONS
 *============================================================================*/

void show_pairing_screen(uint32_t passkey)
{
    LOG_INF("Showing pairing screen with passkey: %06u", passkey);

    /* Save current screen */
    previous_screen = lv_scr_act();

    /* Create and show pairing screen */
    create_pairing_ui(passkey);
    lv_scr_load(pairing_screen);
    screen_visible = true;

    LOG_DBG("Pairing screen loaded");
}

void hide_pairing_screen(void)
{
    if (!screen_visible) {
        LOG_DBG("Pairing screen not visible, nothing to hide");
        return;
    }

    LOG_INF("Hiding pairing screen");

    /* Return to previous screen */
    if (previous_screen != NULL) {
        lv_scr_load(previous_screen);
    }

    /* Clean up pairing screen */
    if (pairing_screen != NULL) {
        lv_obj_del(pairing_screen);
        pairing_screen = NULL;
        passkey_label = NULL;
        instruction_label = NULL;
        confirm_btn = NULL;
        reject_btn = NULL;
    }

    screen_visible = false;
    previous_screen = NULL;

    LOG_DBG("Pairing screen hidden");
}

bool is_pairing_screen_visible(void)
{
    return screen_visible;
}

void handle_pairing_confirm(void)
{
    LOG_INF("Handling pairing confirmation");
    confirm_pairing();
}

void handle_pairing_reject(void)
{
    LOG_INF("Handling pairing rejection");
    reject_pairing();
}
