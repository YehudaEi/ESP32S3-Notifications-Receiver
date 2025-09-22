/**
 * @file graphics.c
 * @brief LVGL Compatible Graphics Implementation for Zephyr
 *
 * This implementation is compatible with LVGL API changes.
 * Major changes from LVGL 8.x:
 * - lv_task_handler() -> lv_timer_handler()
 * - LV_NO_TASK_READY -> LV_NO_TIMER_READY
 * - Display driver API completely changed
 * - Input device API simplified
 *
 * @author Yehuda@YehudaE.net
 */

#include <lvgl.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "graphics/graphics.h"

LOG_MODULE_REGISTER(graphics, LOG_LEVEL_INF);

/** @brief LVGL display buffer size */
#define LVGL_BUFFER_SIZE (240 * 320 / 10) // Adjust for your display

/** @brief LVGL refresh period in milliseconds */
#define LVGL_REFRESH_PERIOD_MS 33 /* ~30 FPS */

/** @brief LVGL task handler thread priority */
#define LVGL_THREAD_PRIORITY 7

/** @brief LVGL task handler stack size */
#define LVGL_THREAD_STACK_SIZE 4096

/* Static display buffer for LVGL */
static lv_color_t lvgl_display_buf[LVGL_BUFFER_SIZE];

/* Display object pointer */
static lv_display_t* lvgl_display = NULL;

/* Timer for LVGL task handling */
static struct k_timer lvgl_timer;

/* Thread for LVGL task handling */
K_THREAD_STACK_DEFINE(lvgl_thread_stack, LVGL_THREAD_STACK_SIZE);
static struct k_thread lvgl_thread_data;
static k_tid_t lvgl_thread_tid;

/**
 * @brief LVGL timer callback function
 *
 * This callback is called periodically to update LVGL's internal timer.
 *
 * @param timer Pointer to the timer object
 */
static void lvgl_timer_callback(struct k_timer* timer)
{
    ARG_UNUSED(timer);
    lv_tick_inc(LVGL_REFRESH_PERIOD_MS);
}

/**
 * @brief LVGL task handler thread function
 *
 * This thread continuously processes LVGL tasks and handles display updates.
 */
static void lvgl_task_thread(void* arg1, void* arg2, void* arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    LOG_INF("LVGL task handler thread started");

    while (1) {
        /* Process LVGL timers and tasks */
        uint32_t sleep_time = lv_timer_handler();

        /* Sleep for the time recommended by LVGL or minimum period */
        if (sleep_time == LV_NO_TIMER_READY) {
            k_sleep(K_MSEC(LVGL_REFRESH_PERIOD_MS));
        } else {
            /* Ensure minimum sleep time */
            sleep_time = MAX(sleep_time, 5);
            k_sleep(K_MSEC(sleep_time));
        }
    }
}

/**
 * @brief Display flush callback for LVGL 9.x
 *
 * This function is called by LVGL when it needs to flush the display buffer
 * to the actual display hardware.
 *
 * @param disp Pointer to display object
 * @param area Area of the display to flush
 * @param px_map Pointer to pixel data
 */
static void display_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map)
{
    const struct device* display_dev = (const struct device*)lv_display_get_user_data(disp);
    struct display_buffer_descriptor desc;

    /* Calculate buffer parameters */
    uint16_t width = lv_area_get_width(area);
    uint16_t height = lv_area_get_height(area);

    desc.buf_size = width * height * sizeof(lv_color_t);
    desc.width = width;
    desc.height = height;
    desc.pitch = width;

    /* Write to display */
    int ret = display_write(display_dev, area->x1, area->y1, &desc, (void*)px_map);
    if (ret < 0) {
        LOG_ERR("Failed to write to display (ret: %d)", ret);
    }

    /* Inform LVGL that the flush is complete */
    lv_display_flush_ready(disp);
}

/**
 * @brief Input device read callback for LVGL (touchscreen)
 *
 * This function reads touch input data and provides it to LVGL.
 *
 * @param indev Pointer to input device object
 * @param data Pointer to input data structure
 */
static void input_read_cb(lv_indev_t* indev, lv_indev_data_t* data)
{
    ARG_UNUSED(indev);

    /* TODO: Implement actual touch input reading */
    /* This is a placeholder - replace with actual touch controller code */

    /* For now, just set to released state */
    data->state = LV_INDEV_STATE_RELEASED;
    data->point.x = 0;
    data->point.y = 0;
}

/**
 * @brief Initialize LVGL display driver for version 9.x
 *
 * Sets up the display driver for LVGL using Zephyr's display API.
 *
 * @return 0 on success, negative error code on failure
 */
static int init_lvgl_display(void)
{
    const struct device* display_dev;
    struct display_capabilities caps;

    LOG_INF("Initializing LVGL display driver...");

    /* Get display device - adjust this to match your device tree */
    display_dev = DEVICE_DT_GET(DT_CHOSEN(nr_lcd));
    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device is not ready");
        return -ENODEV;
    }

    /* Get display capabilities */
    display_get_capabilities(display_dev, &caps);
    LOG_INF("Display: %dx%d, format: %d",
        caps.x_resolution, caps.y_resolution, caps.current_pixel_format);

    /* Create LVGL display object */
    lvgl_display = lv_display_create(caps.x_resolution, caps.y_resolution);
    if (!lvgl_display) {
        LOG_ERR("Failed to create LVGL display object");
        return -ENOMEM;
    }

    /* Set default theme, primary color - light green, secondary - orange, dark = true. */
    lv_theme_t* theme = lv_theme_default_init(lvgl_display, lv_palette_main(LV_PALETTE_LIGHT_GREEN),
        lv_palette_main(LV_PALETTE_ORANGE), true, LV_FONT_DEFAULT);
    lv_display_set_theme(lvgl_display, theme);

    /* Set display buffer - LVGL way */
    lv_display_set_buffers(lvgl_display, lvgl_display_buf, NULL,
        sizeof(lvgl_display_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* Set flush callback and user data */
    lv_display_set_flush_cb(lvgl_display, display_flush_cb);
    lv_display_set_user_data(lvgl_display, (void*)display_dev);

    LOG_INF("LVGL display driver initialized successfully");
    return 0;
}

/**
 * @brief Initialize LVGL input device for version 9.x
 *
 * Sets up input device handling for LVGL (touchscreen, buttons, etc.).
 *
 * @return 0 on success, negative error code on failure
 */
static int init_lvgl_input(void)
{
    lv_indev_t* indev;

    LOG_INF("Initializing LVGL input device...");

    /* Create input device - LVGL way */
    indev = lv_indev_create();
    if (!indev) {
        LOG_ERR("Failed to create input device");
        return -ENOMEM;
    }

    /* Configure input device */
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER); /* Touchscreen */
    lv_indev_set_read_cb(indev, input_read_cb);

    LOG_INF("LVGL input device initialized successfully");
    return 0;
}

/**
 * @brief Create initial LVGL UI
 *
 * Creates a simple initial user interface for testing.
 */
static void create_initial_ui(void)
{
    // The notification screen will be created after LVGL is fully initialized
    // This is now done in main.c after init_lvgl_graphics() completes
    LOG_INF("LVGL ready for notification screen creation");
}

/**
 * @brief Initialize LVGL graphics library (LVGL compatible)
 *
 * Complete initialization of LVGL including:
 * - Core LVGL initialization
 * - Display driver setup
 * - Input device setup
 * - Threading and timing setup
 * - Initial UI creation
 *
 * @return 0 on success, negative error code on failure
 */
int init_lvgl_graphics(void)
{
    int ret;

    LOG_INF("Initializing LVGL graphics library");

    /* Initialize LVGL core */
    lv_init();
    LOG_DBG("LVGL core initialized");

    /* Initialize display driver */
    ret = init_lvgl_display();
    if (ret < 0) {
        LOG_ERR("Failed to initialize LVGL display (ret: %d)", ret);
        return ret;
    }

    /* Initialize input device */
    ret = init_lvgl_input();
    if (ret < 0) {
        LOG_ERR("Failed to initialize LVGL input (ret: %d)", ret);
        /* Continue without input - display-only mode */
        LOG_WRN("Continuing in display-only mode");
    }

    /* Initialize timer for LVGL tick */
    k_timer_init(&lvgl_timer, lvgl_timer_callback, NULL);
    k_timer_start(&lvgl_timer, K_MSEC(LVGL_REFRESH_PERIOD_MS),
        K_MSEC(LVGL_REFRESH_PERIOD_MS));
    LOG_DBG("LVGL timer initialized");

    /* Create LVGL task handler thread */
    lvgl_thread_tid = k_thread_create(&lvgl_thread_data, lvgl_thread_stack,
        K_THREAD_STACK_SIZEOF(lvgl_thread_stack),
        lvgl_task_thread, NULL, NULL, NULL,
        LVGL_THREAD_PRIORITY, 0, K_NO_WAIT);
    if (!lvgl_thread_tid) {
        LOG_ERR("Failed to create LVGL task thread");
        k_timer_stop(&lvgl_timer);
        return -EFAULT;
    }
    k_thread_name_set(lvgl_thread_tid, "lvgl_task");
    LOG_DBG("LVGL task thread created");

    /* Create initial UI */
    create_initial_ui();

    LOG_INF("LVGL graphics library initialized successfully");
    return 0;
}

/**
 * @brief Deinitialize LVGL graphics library
 *
 * Clean shutdown of LVGL components.
 *
 * @return 0 on success, negative error code on failure
 */
int deinit_lvgl_graphics(void)
{
    LOG_INF("Shutting down LVGL graphics library...");

    /* Stop the timer */
    k_timer_stop(&lvgl_timer);

    /* Abort the task thread */
    if (lvgl_thread_tid) {
        k_thread_abort(lvgl_thread_tid);
    }

/* Clean up LVGL - LVGL */
#if LV_VERSION_CHECK(9, 0, 0)
    lv_deinit();
#endif

    LOG_INF("LVGL graphics library shut down complete");
    return 0;
}

/**
 * @brief Check if LVGL is initialized and ready
 *
 * @return true if LVGL is ready, false otherwise
 */
bool is_lvgl_ready(void)
{
    return (lvgl_display != NULL);
}

/**
 * @brief Get LVGL display object
 *
 * @return Pointer to LVGL display object, NULL if not initialized
 */
lv_display_t* get_lvgl_display(void)
{
    return lvgl_display;
}
