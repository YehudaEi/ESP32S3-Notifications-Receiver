/**
 * @file graphics.h
 * @brief LVGL Initialization Header for Zephyr
 *
 * Header file for LVGL initialization functions and utilities
 * for Zephyr RTOS applications.
 *
 * @author Yehuda@YehudaE.net
 */

#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

#include <stdbool.h>

#include <lvgl.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup lvgl_zephyr LVGL Zephyr Integration
 * @brief LVGL integration functions for Zephyr RTOS
 * @{
 */

/**
 * @brief Initialize LVGL graphics library
 *
 * Performs complete LVGL initialization including:
 * - Core LVGL library initialization
 * - Display driver setup with Zephyr display API
 * - Input device configuration (touchscreen/buttons)
 * - Threading and timing setup
 * - Initial user interface creation
 *
 * @retval 0 Success
 * @retval -ENODEV Display device not ready
 * @retval -ENOMEM Insufficient memory for buffers
 * @retval -EFAULT Failed to register drivers with LVGL
 *
 * @note This function should be called after display hardware is initialized
 * @note Requires CONFIG_LVGL=y in prj.conf
 */
int init_lvgl_graphics(void);

/**
 * @brief Deinitialize LVGL graphics library
 *
 * Performs graceful shutdown of LVGL components:
 * - Stops LVGL task handler thread
 * - Stops LVGL timer
 * - Cleans up LVGL core
 *
 * @retval 0 Success
 * @retval Negative errno codes on failure
 *
 * @note Call during system shutdown or before system reset
 */
int deinit_lvgl_graphics(void);

/**
 * @brief Check if LVGL is initialized and ready
 *
 * @retval true LVGL is initialized and ready for use
 * @retval false LVGL is not initialized
 *
 * @note Use this to verify LVGL state before creating UI elements
 */
bool is_lvgl_ready(void);

/**
 * @brief Get LVGL display object
 *
 * Returns the main LVGL display object for advanced operations.
 *
 * @return Pointer to LVGL display object, NULL if not initialized
 */
lv_display_t* get_lvgl_display(void);

/**
 * @brief Create a simple notification UI
 *
 * Creates a basic notification display with title and message.
 *
 * @param title Notification title (max 64 characters)
 * @param message Notification message (max 256 characters)
 * @param timeout_ms Auto-dismiss timeout in milliseconds (0 = no timeout)
 *
 * @return Pointer to created notification object, NULL on failure
 */
lv_obj_t* create_notification_ui(const char* title, const char* message, uint32_t timeout_ms);

/**
 * @brief Update display brightness via LVGL
 *
 * Updates display brightness and adjusts LVGL theme accordingly.
 *
 * @param brightness Brightness level (0-100%)
 * @return 0 on success, negative error code on failure
 */
int lvgl_set_brightness(uint8_t brightness);

/**
 * @brief Set LVGL theme mode
 *
 * @param dark_mode true for dark theme, false for light theme
 * @return 0 on success, negative error code on failure
 */
int lvgl_set_theme_mode(bool dark_mode);

/**
 * @brief Force LVGL display refresh
 *
 * Forces an immediate refresh of the entire display.
 * Use sparingly as it bypasses LVGL's optimization.
 */
void lvgl_force_refresh(void);

/**
 * @brief LVGL task handler function (for manual integration)
 *
 * Call this function periodically if not using the built-in thread.
 * Not needed when using init_lvgl_graphics() with automatic threading.
 *
 * @return Time to sleep before next call (milliseconds)
 */
uint32_t lvgl_task_handler_manual(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __GRAPHICS_H__ */
