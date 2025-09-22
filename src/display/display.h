/**
 * @file display.h
 * @brief LCD Display Driver Header
 *
 * This header provides the interface for LCD display management in Zephyr RTOS.
 * The driver supports PWM-controlled backlight and display power management.
 *
 * @author Yehuda@YehudaE.net
 */

#ifndef DISPLAY_H_
#define DISPLAY_H_

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup display_api Display Driver API
 * @brief LCD Display Driver API
 * @{
 */

/**
 * @brief Initialize and enable the LCD display
 *
 * This function performs complete display initialization including:
 * - Display device validation and setup
 * - PWM backlight initialization with default brightness (50%)
 * - Display blanking disable (turns display on)
 *
 * @retval 0 Success
 * @retval -ENODEV Display or PWM device not ready
 * @retval -EINVAL Invalid device configuration
 * @retval Other negative errno codes on other failures
 *
 * @note This function should be called once during system initialization
 */
int enable_display(void);

/**
 * @brief Disable and shutdown the LCD display
 *
 * This function performs graceful display shutdown:
 * - Enables display blanking (turns display off)
 * - Sets backlight to minimum brightness
 *
 * @retval 0 Success
 * @retval Negative errno codes on failure
 *
 * @note This function can be called during system shutdown or power saving
 */
int disable_display(void);

/**
 * @brief Change display backlight brightness
 *
 * Adjusts the PWM duty cycle to control backlight brightness.
 * Input values are automatically clamped to the valid range (5-100%).
 *
 * @param perc Desired brightness percentage (0-100)
 *             - Values < 5% are automatically set to 5%
 *             - Values > 100% are automatically set to 100%
 *
 * @retval 0 Success
 * @retval -ENODEV PWM device not ready
 * @retval -EINVAL Invalid parameters
 * @retval Other negative errno codes on PWM operation failure
 *
 * @note Minimum brightness is enforced to ensure display remains visible
 */
int change_brightness(uint8_t perc);

/**
 * @brief Get current display readiness status
 *
 * Checks if both display and PWM backlight devices are ready for operation.
 *
 * @retval true Both display and backlight are ready
 * @retval false One or both devices are not ready
 *
 * @note This function can be used to verify system state before operations
 */
bool is_display_ready(void);

/**
 * @brief Control display blanking state
 *
 * Enables or disables display blanking (screen on/off) without affecting
 * the backlight PWM settings.
 *
 * @param blank true to enable blanking (turn off display),
 *              false to disable blanking (turn on display)
 *
 * @retval 0 Success
 * @retval -ENODEV Display device not ready
 * @retval Other negative errno codes on display operation failure
 */
int set_display_blanking(bool blank);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* DISPLAY_H_ */
