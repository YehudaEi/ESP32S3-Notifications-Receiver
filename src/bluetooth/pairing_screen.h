/**
 * @file pairing_screen.h
 * @brief BLE Pairing Screen UI Header
 *
 * Provides UI for displaying pairing codes and confirmation.
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
 * @brief Show pairing screen with passkey
 *
 * Displays a modal pairing screen showing the 6-digit passkey
 * that should be entered on the connecting device.
 *
 * @param passkey 6-digit pairing code to display
 */
void show_pairing_screen(uint32_t passkey);

/**
 * @brief Hide pairing screen
 *
 * Removes the pairing screen from display and returns to
 * the previous screen (usually notifications).
 */
void hide_pairing_screen(void);

/**
 * @brief Check if pairing screen is currently visible
 *
 * @return true if pairing screen is shown, false otherwise
 */
bool is_pairing_screen_visible(void);

/**
 * @brief Handle user confirmation of pairing
 *
 * Called when user accepts the pairing request.
 */
void handle_pairing_confirm(void);

/**
 * @brief Handle user rejection of pairing
 *
 * Called when user declines the pairing request.
 */
void handle_pairing_reject(void);

#ifdef __cplusplus
}
#endif

#endif /* PAIRING_SCREEN_H */
