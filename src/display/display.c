/**
 * @file display.c
 * @brief LCD Display Driver Implementation
 *
 * This module provides functionality for managing LCD displays in Zephyr RTOS,
 * including initialization, brightness control, and power management.
 *
 * The driver supports:
 * - Display initialization and shutdown
 * - PWM-controlled backlight brightness adjustment
 * - Display blanking control
 *
 * @author Yehuda@YehudaE.net
 */

#include "display/display.h"
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(display, LOG_LEVEL_INF);

/** @brief Default PWM period for backlight control in nanoseconds */
#define PWM_PERIOD_NS 50000U

/** @brief Default PWM duty cycle (50% brightness) */
#define PWM_DEFAULT_DUTY_CYCLE_NS 25000U

/** @brief Minimum brightness percentage */
#define MIN_BRIGHTNESS_PERCENT 5U

/** @brief Maximum brightness percentage */
#define MAX_BRIGHTNESS_PERCENT 100U

/**
 * @brief Get and validate PWM backlight device
 *
 * @param backlight Pointer to PWM device specification structure
 * @return 0 on success, negative error code on failure
 */
static int get_backlight_device(struct pwm_dt_spec* backlight)
{
    if (!backlight) {
        LOG_ERR("Invalid backlight parameter");
        return -EINVAL;
    }

    *backlight = (struct pwm_dt_spec)PWM_DT_SPEC_GET_BY_IDX(DT_CHOSEN(nr_lcd_backlight), 0);

    if (!pwm_is_ready_dt(backlight)) {
        LOG_ERR("PWM backlight device is not ready");
        return -ENODEV;
    }

    return 0;
}

/**
 * @brief Initialize and enable the LCD display
 *
 * This function performs the following operations:
 * 1. Validates display device availability
 * 2. Initializes PWM backlight with default settings
 * 3. Turns off display blanking (enables display)
 *
 * @return 0 on success, positive error code on failure
 */
int enable_display(void)
{
    int ret;
    const struct device* display_dev;
    struct pwm_dt_spec backlight;

    LOG_INF("Initializing LCD display...");

    /* Get and validate display device */
    display_dev = DEVICE_DT_GET(DT_CHOSEN(nr_lcd));
    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device is not ready");
        return -ENODEV;
    }
    LOG_DBG("Display device initialized successfully");

    /* Initialize PWM backlight */
    ret = get_backlight_device(&backlight);
    if (ret < 0) {
        LOG_ERR("Failed to initialize backlight device (ret: %d)", ret);
        return -ret; /* Convert to positive error code for legacy compatibility */
    }
    LOG_DBG("PWM backlight device initialized successfully");

    /* Set initial backlight brightness (50%) */
    ret = pwm_set_dt(&backlight, PWM_PERIOD_NS, PWM_DEFAULT_DUTY_CYCLE_NS);
    if (ret < 0) {
        LOG_ERR("Failed to set initial PWM brightness (ret: %d)", ret);
        return -ret;
    }
    LOG_DBG("Initial PWM brightness configured (50%%)");

    /* Enable display by turning off blanking */
    ret = display_blanking_off(display_dev);
    if (ret < 0) {
        LOG_ERR("Failed to disable display blanking (ret: %d)", ret);
        return -ret;
    }
    LOG_DBG("Display blanking disabled - display is now active");

    LOG_INF("LCD display initialization completed successfully");
    return 0;
}

/**
 * @brief Disable and shutdown the LCD display
 *
 * This function performs cleanup operations when shutting down the display:
 * 1. Enables display blanking (turns off display)
 * 2. Sets backlight brightness to minimum
 *
 * @return 0 on success, negative error code on failure
 */
int disable_display(void)
{
    int ret;
    const struct device* display_dev;
    struct pwm_dt_spec backlight;

    LOG_INF("Shutting down LCD display...");

    /* Get display device */
    display_dev = DEVICE_DT_GET(DT_CHOSEN(nr_lcd));
    if (!device_is_ready(display_dev)) {
        LOG_WRN("Display device not available for shutdown");
        /* Continue with backlight shutdown anyway */
    } else {
        /* Enable blanking to turn off display */
        ret = display_blanking_on(display_dev);
        if (ret < 0) {
            LOG_ERR("Failed to enable display blanking (ret: %d)", ret);
            return ret;
        }
        LOG_DBG("Display blanking enabled");
    }

    /* Turn off backlight */
    ret = get_backlight_device(&backlight);
    if (ret < 0) {
        LOG_ERR("Failed to get backlight device for shutdown (ret: %d)", ret);
        return ret;
    }

    /* Set backlight to minimum (effectively off) */
    ret = pwm_set_dt(&backlight, PWM_PERIOD_NS, 0);
    if (ret < 0) {
        LOG_ERR("Failed to turn off backlight (ret: %d)", ret);
        return ret;
    }
    LOG_DBG("Backlight turned off");

    LOG_INF("LCD display shutdown completed successfully");
    return 0;
}

/**
 * @brief Change display backlight brightness
 *
 * Adjusts the PWM duty cycle to control backlight brightness.
 * The brightness percentage is automatically clamped to valid range.
 *
 * @param perc Brightness percentage (0-100)
 *             Values below 5% are clamped to 5%
 *             Values above 100% are clamped to 100%
 *
 * @return 0 on success, negative error code on failure
 */
int change_brightness(uint8_t perc)
{
    int ret;
    struct pwm_dt_spec backlight;
    uint32_t pulse_ns;

    LOG_DBG("Changing brightness to %u%%", perc);

    /* Get and validate backlight device */
    ret = get_backlight_device(&backlight);
    if (ret < 0) {
        return ret;
    }

    /* Clamp brightness to valid range */
    if (perc > MAX_BRIGHTNESS_PERCENT) {
        perc = MAX_BRIGHTNESS_PERCENT;
        LOG_WRN("Brightness clamped to maximum (%u%%)", MAX_BRIGHTNESS_PERCENT);
    }
    if (perc < MIN_BRIGHTNESS_PERCENT) {
        perc = MIN_BRIGHTNESS_PERCENT;
        LOG_WRN("Brightness clamped to minimum (%u%%)", MIN_BRIGHTNESS_PERCENT);
    }

    /* Calculate PWM pulse width based on percentage */
    pulse_ns = (PWM_PERIOD_NS * perc) / 100U;

    /* Apply PWM settings */
    ret = pwm_set_dt(&backlight, PWM_PERIOD_NS, pulse_ns);
    if (ret < 0) {
        LOG_ERR("Failed to set PWM brightness (ret: %d)", ret);
        return ret;
    }

    LOG_INF("Brightness successfully set to %u%% (pulse: %u ns)", perc, pulse_ns);
    return 0;
}

/**
 * @brief Get current display status
 *
 * @return true if display is enabled and ready, false otherwise
 */
bool is_display_ready(void)
{
    const struct device* display_dev = DEVICE_DT_GET(DT_CHOSEN(nr_lcd));
    struct pwm_dt_spec backlight = PWM_DT_SPEC_GET_BY_IDX(DT_CHOSEN(nr_lcd_backlight), 0);

    return device_is_ready(display_dev) && pwm_is_ready_dt(&backlight);
}

/**
 * @brief Set display blanking state
 *
 * @param blank true to enable blanking (turn off display), false to disable
 * @return 0 on success, negative error code on failure
 */
int set_display_blanking(bool blank)
{
    int ret;
    const struct device* display_dev = DEVICE_DT_GET(DT_CHOSEN(nr_lcd));

    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device is not ready");
        return -ENODEV;
    }

    if (blank) {
        ret = display_blanking_on(display_dev);
        LOG_DBG("Display blanking enabled");
    } else {
        ret = display_blanking_off(display_dev);
        LOG_DBG("Display blanking disabled");
    }

    if (ret < 0) {
        LOG_ERR("Failed to set display blanking state (ret: %d)", ret);
    }

    return ret;
}
