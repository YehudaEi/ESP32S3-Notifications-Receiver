/**
 * @file pairing_screen.h
 * @brief BLE Pairing Screen Management Header
 *
 * This module provides the user interface for BLE device pairing,
 * displaying pairing codes and handling user confirmation.
 *
 * @author Yehuda@YehudaE.net
 */

#ifndef PAIRING_SCREEN_H
#define PAIRING_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup pairing_screen_api BLE Pairing Screen API
 * @brief BLE Pairing User Interface
 * @{
 */

/*==============================================================================
 * TYPE DEFINITIONS
 *============================================================================*/

/**
 * @brief Pairing screen callback function types
 */
typedef enum {
    PAIRING_ACTION_ACCEPT, /**< User accepted pairing */
    PAIRING_ACTION_CANCEL, /**< User cancelled pairing */
    PAIRING_ACTION_TIMEOUT /**< Pairing timed out */
} pairing_action_t;

/**
 * @brief Pairing result callback function
 *
 * Called when user takes action on pairing screen
 *
 * @param action Action taken by user
 * @param user_data User data passed during initialization
 */
typedef void (*pairing_result_callback_t)(pairing_action_t action, void* user_data);

/**
 * @brief Pairing request information
 */
typedef struct {
    char device_name[32]; /**< Name of device requesting to pair */
    char device_address[18]; /**< Address of device requesting to pair */
    uint32_t passkey; /**< 6-digit passkey to display */
    uint32_t timeout_seconds; /**< Timeout in seconds (0 = no timeout) */
    pairing_result_callback_t callback; /**< Callback for user actions */
    void* user_data; /**< User data for callback */
} pairing_request_info_t;

/*==============================================================================
 * PUBLIC FUNCTION DECLARATIONS
 *============================================================================*/

/**
 * @brief Initialize pairing screen system
 *
 * Sets up the pairing screen infrastructure. Must be called once
 * during system initialization, after LVGL is initialized.
 *
 * @return 0 on success, negative error code on failure
 */
int pairing_screen_init(void);

/**
 * @brief Show pairing request screen
 *
 * Displays the pairing screen with device information and passkey.
 * The screen will show:
 * - Device name and address
 * - 6-digit passkey prominently displayed
 * - Accept/Cancel buttons
 * - Countdown timer (if timeout specified)
 *
 * @param request_info Pairing request information
 * @return 0 on success, negative error code on failure
 * @retval -EINVAL Invalid parameters
 * @retval -EBUSY Pairing screen already active
 */
int pairing_screen_show(const pairing_request_info_t* request_info);

/**
 * @brief Hide pairing screen and return to previous screen
 *
 * @return 0 on success, negative error code on failure
 */
int pairing_screen_hide(void);

/**
 * @brief Check if pairing screen is currently active
 *
 * @return true if pairing screen is visible, false otherwise
 */
bool pairing_screen_is_active(void);

/**
 * @brief Update pairing timeout countdown
 *
 * Updates the countdown display. Should be called periodically
 * from main loop when pairing screen is active.
 *
 * @return Remaining timeout in seconds, 0 if expired
 */
uint32_t pairing_screen_update_timeout(void);

/**
 * @brief Force timeout the current pairing request
 *
 * Immediately times out the pairing request and calls the callback
 * with PAIRING_ACTION_TIMEOUT.
 *
 * @return 0 on success, negative error code on failure
 */
int pairing_screen_force_timeout(void);

/**
 * @brief Get current pairing passkey
 *
 * Returns the currently displayed passkey, useful for validation.
 *
 * @return Current passkey, 0 if no pairing active
 */
uint32_t pairing_screen_get_current_passkey(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* PAIRING_SCREEN_H */
