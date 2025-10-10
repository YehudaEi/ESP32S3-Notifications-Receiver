#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <lvgl.h>
#include <zephyr/kernel.h>

#include "fonts/heb_fonts.h"
#include "notifications/notifications.h"
#include "rtc/rtc.h"

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240
#define SCREEN_RADIUS 120
#define MAX_NOTIFICATIONS 30

// Notification structure
typedef struct {
    char app_name[32];
    char sender[64];
    char content[256];
    uint32_t timestamp; // Unix timestamp
    bool is_read;
} notification_t;

// Global UI objects
static lv_obj_t* main_screen;
static lv_obj_t* time_label;
static lv_obj_t* status_circle;
static lv_obj_t* app_icon;
static lv_obj_t* app_name_label;
static lv_obj_t* message_unread;
static lv_obj_t* sender_label;
static lv_obj_t* notification_content;
static lv_obj_t* secondary_info;
static lv_obj_t* counter_label;

// Status colors - initialized in create_styles()
static lv_color_t status_colors[4];

// App icon colors - initialized in create_styles()
static lv_color_t app_colors[5];

// Notification data
static notification_t notifications[MAX_NOTIFICATIONS];
static int notification_count = 0;
static int current_notification = 0;

// Undo functionality
static bool delete_pending = false;
static int delete_pending_index = -1;
static int delete_timer_counter = 0;
static const int DELETE_TIMEOUT = 20; // 3 seconds (30 * 100ms)
static lv_obj_t* undo_arclabel; // Using lv_arclabel widget

// Forward declarations
static void update_notification_display(void);
static void next_notification(void);
static void prev_notification(void);
static void mark_current_as_read(void);
static void delete_current_notification(void);
static void undo_deletion(void);
static void complete_deletion(void);
static void handle_delete_timeout(void);
static bool is_rtl_text(const char* text);
static void sanitize_text_for_display(const char* input, char* output, size_t output_size);

/**
 * @brief Check if text contains Hebrew or Arabic characters (RTL languages)
 *
 * @param text UTF-8 encoded text to check
 * @return true if text contains RTL characters
 */
static bool is_rtl_text(const char* text)
{
    if (!text)
        return false;

    const uint8_t* p = (const uint8_t*)text;

    while (*p) {
        // Check for Hebrew characters (U+0590 to U+05FF)
        if (p[0] == 0xD6 || p[0] == 0xD7) {
            if ((p[0] == 0xD6 && p[1] >= 0x90) || (p[0] == 0xD7 && p[1] <= 0xBF)) {
                return true;
            }
        }

        // Check for Arabic characters (U+0600 to U+06FF)
        if (p[0] == 0xD8 || p[0] == 0xD9) {
            return true;
        }

        // Move to next UTF-8 character
        if ((*p & 0x80) == 0)
            p += 1; // ASCII
        else if ((*p & 0xE0) == 0xC0)
            p += 2; // 2-byte
        else if ((*p & 0xF0) == 0xE0)
            p += 3; // 3-byte
        else if ((*p & 0xF8) == 0xF0)
            p += 4; // 4-byte (emoji!)
        else
            p += 1; // Invalid, skip
    }

    return false;
}

/**
 * @brief Sanitize text by removing or replacing problematic characters like emoji
 *
 * This function filters out 4-byte UTF-8 characters (emoji and other extended Unicode)
 * that may not be supported by custom fonts and could cause crashes.
 *
 * @param input Source text (UTF-8)
 * @param output Destination buffer for sanitized text
 * @param output_size Size of output buffer
 */
static void sanitize_text_for_display(const char* input, char* output, size_t output_size)
{
    if (!input || !output || output_size == 0)
        return;

    const uint8_t* src = (const uint8_t*)input;
    uint8_t* dst = (uint8_t*)output;
    size_t dst_idx = 0;

    while (*src && dst_idx < output_size - 1) {
        uint8_t byte = *src;

        // ASCII - copy as-is
        if ((byte & 0x80) == 0) {
            dst[dst_idx++] = *src++;
        }
        // 2-byte UTF-8 character - copy as-is
        else if ((byte & 0xE0) == 0xC0 && dst_idx < output_size - 2) {
            dst[dst_idx++] = *src++;
            if (*src)
                dst[dst_idx++] = *src++;
        }
        // 3-byte UTF-8 character - copy as-is
        else if ((byte & 0xF0) == 0xE0 && dst_idx < output_size - 3) {
            dst[dst_idx++] = *src++;
            if (*src)
                dst[dst_idx++] = *src++;
            if (*src)
                dst[dst_idx++] = *src++;
        }
        // 4-byte UTF-8 character (EMOJI!) - replace with a box or skip
        else if ((byte & 0xF8) == 0xF0) {
            // Option 1: Replace with [?] or similar placeholder
            if (dst_idx < output_size - 3) {
                dst[dst_idx++] = '[';
                dst[dst_idx++] = '?';
                dst[dst_idx++] = ']';
            }
            // Skip the 4-byte emoji
            src++;
            if (*src)
                src++;
            if (*src)
                src++;
            if (*src)
                src++;
        }
        // Invalid UTF-8 - skip
        else {
            src++;
        }
    }

    dst[dst_idx] = '\0';
}

// Sample notifications for testing
static void init_sample_notifications(void)
{
    notification_count = 5;
    uint32_t current_time = rtc_get_timestamp();

    /* If RTC not set yet, use placeholder values */
    if (current_time == 0) {
        current_time = 1700000000; /* Some arbitrary timestamp */
    }

    // Notification 1: WhatsApp (5 minutes ago)
    strcpy(notifications[0].app_name, "WhatsApp");
    strcpy(notifications[0].sender, "Mom");
    strcpy(notifications[0].content, "Hi honey! How are you today?");
    notifications[0].timestamp = current_time - 300; /* 5 minutes ago */
    notifications[0].is_read = false;

    // Notification 2: Email (30 minutes ago, Hebrew text)
    strcpy(notifications[1].app_name, "Gmail");
    strcpy(notifications[1].sender, "משה כהן");
    strcpy(notifications[1].content, "בדיקה של הודעה בעברית משהו משהו.");
    notifications[1].timestamp = current_time - 1800; /* 30 minutes ago */
    notifications[1].is_read = false;

    // Notification 3: SMS (2 hours ago)
    strcpy(notifications[2].app_name, "Messages");
    strcpy(notifications[2].sender, "John");
    strcpy(notifications[2].content, "Are we still meeting tonight?");
    notifications[2].timestamp = current_time - 7200; /* 2 hours ago */
    notifications[2].is_read = true;

    // Notification 4: Discord (5 hours ago)
    strcpy(notifications[3].app_name, "Discord");
    strcpy(notifications[3].sender, "Dev Team");
    strcpy(notifications[3].content, "New commit pushed to main branch. Please review the changes in the notification system implementation.");
    notifications[3].timestamp = current_time - 18000; /* 5 hours ago */
    notifications[3].is_read = false;

    // Notification 5: Telegram (yesterday, with emoji that will be sanitized)
    strcpy(notifications[4].app_name, "Telegram");
    strcpy(notifications[4].sender, "Sarah");
    strcpy(notifications[4].content, "Check this out 🚀!");
    notifications[4].timestamp = current_time - 86400; /* 1 day ago */
    notifications[4].is_read = false;
}

static void create_styles(void)
{
    // Initialize color arrays
    status_colors[CONN_CONNECTED] = lv_color_hex(0x00FF00); // Green
    status_colors[CONN_WEAK_SIGNAL] = lv_color_hex(0xFFFF00); // Yellow
    status_colors[CONN_CONNECTING] = lv_color_hex(0x0096FF); // Blue
    status_colors[CONN_DISCONNECTED] = lv_color_hex(0xFF0000); // Red

    app_colors[0] = lv_color_hex(0x25D366); // WhatsApp green
    app_colors[1] = lv_color_hex(0x1877F2); // Facebook blue
    app_colors[2] = lv_color_hex(0xFF0000); // Gmail red
    app_colors[3] = lv_color_hex(0x9146FF); // Discord purple
    app_colors[4] = lv_color_hex(0x0088CC); // Telegram blue

    // Main screen style (black background)
    static lv_style_t screen_style;
    lv_style_init(&screen_style);
    lv_style_set_bg_color(&screen_style, lv_color_hex(0x000000));
    lv_style_set_text_color(&screen_style, lv_color_hex(0xFFFFFF));

    // Time label style
    static lv_style_t time_style;
    lv_style_init(&time_style);
    lv_style_set_text_font(&time_style, &heb_font_16);
    lv_style_set_text_color(&time_style, lv_color_hex(0xFFFFFF));

    // App name style
    static lv_style_t app_name_style;
    lv_style_init(&app_name_style);
    lv_style_set_text_font(&app_name_style, &heb_font_12);
    lv_style_set_text_color(&app_name_style, lv_color_hex(0xC8C8C8));

    // Main content style
    static lv_style_t content_style;
    lv_style_init(&content_style);
    lv_style_set_text_font(&content_style, &heb_font_16);
    lv_style_set_text_color(&content_style, lv_color_hex(0xFFFFFF));
    lv_style_set_text_align(&content_style, LV_TEXT_ALIGN_CENTER);

    // Secondary info style
    static lv_style_t secondary_style;
    lv_style_init(&secondary_style);
    lv_style_set_text_font(&secondary_style, &heb_font_10);
    lv_style_set_text_color(&secondary_style, lv_color_hex(0x969696));
}

// Touch event handler
static void screen_event_handler(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

        switch (dir) {
        case LV_DIR_LEFT:
            if (!delete_pending) {
                mark_current_as_read();
                next_notification(); // Next notification
            }
            break;
        case LV_DIR_RIGHT:
            if (!delete_pending) {
                mark_current_as_read();
                prev_notification(); // Previous notification
            }
            break;
        case LV_DIR_TOP:
            if (!delete_pending) {
                delete_current_notification(); // Delete notification (with undo)
            }
            break;
        case LV_DIR_BOTTOM:
            // Optional: could add another action here
            break;
        default:
            break;
        }
    } else if (code == LV_EVENT_CLICKED) {
        if (delete_pending) {
            // Undo the deletion
            undo_deletion();
        }
    } else if (code == LV_EVENT_DOUBLE_CLICKED) {
        if (!delete_pending) {
            mark_current_as_read(); // Mark as read
        }
    }
}

static void create_top_bar(void)
{
    // Create container for top bar
    lv_obj_t* top_container = lv_obj_create(main_screen);
    lv_obj_set_size(top_container, SCREEN_WIDTH - 20, 30);
    lv_obj_align(top_container, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_bg_opa(top_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(top_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(top_container, 0, 0);

    // Time label (left-center)
    time_label = lv_label_create(top_container);
    lv_label_set_text(time_label, "--:--");
    lv_obj_align(time_label, LV_ALIGN_CENTER, -15, 0);

    // Status circle (right of time)
    status_circle = lv_obj_create(top_container);
    lv_obj_set_size(status_circle, 10, 10);
    lv_obj_align(status_circle, LV_ALIGN_CENTER, 15, 0);
    lv_obj_set_style_radius(status_circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(status_circle, status_colors[CONN_DISCONNECTED], 0);
    lv_obj_set_style_border_opa(status_circle, LV_OPA_TRANSP, 0);
}

static lv_color_t get_app_color(const char* app_name)
{
    if (strcmp(app_name, "WhatsApp") == 0)
        return app_colors[0];
    if (strcmp(app_name, "Gmail") == 0)
        return app_colors[2];
    if (strcmp(app_name, "Messages") == 0)
        return lv_color_hex(0x34C759);
    if (strcmp(app_name, "Discord") == 0)
        return app_colors[3];
    if (strcmp(app_name, "Telegram") == 0)
        return app_colors[4];
    return lv_color_hex(0x666666); // Default gray
}

static void create_app_info(void)
{
    // Create container for app info (icon + name)
    lv_obj_t* app_container = lv_obj_create(main_screen);
    lv_obj_set_size(app_container, SCREEN_WIDTH - 40, 30);
    lv_obj_align(app_container, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_opa(app_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(app_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(app_container, 0, 0);

    // App icon (left side of container)
    app_icon = lv_obj_create(app_container);
    lv_obj_set_size(app_icon, 20, 20);
    lv_obj_align(app_icon, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_radius(app_icon, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_opa(app_icon, LV_OPA_TRANSP, 0);

    // App name (centered in container)
    app_name_label = lv_label_create(app_container);
    lv_obj_align(app_name_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(app_name_label, &heb_font_12, 0);
    lv_obj_set_style_text_color(app_name_label, lv_color_hex(0xC8C8C8), 0);

    message_unread = lv_obj_create(app_container);
    lv_obj_set_size(message_unread, 10, 10);
    lv_obj_align(message_unread, LV_ALIGN_RIGHT_MID, -15, 0);
    lv_obj_set_style_radius(message_unread, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(message_unread, status_colors[CONN_WEAK_SIGNAL], 0);
    lv_obj_set_style_border_opa(message_unread, LV_OPA_TRANSP, 0);
}

static void create_notification_content(void)
{
    // Main notification content area
    lv_obj_t* content_container = lv_obj_create(main_screen);
    lv_obj_set_size(content_container, SCREEN_WIDTH - 40, 100);
    lv_obj_align(content_container, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_opa(content_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(content_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(content_container, 5, 0);

    // Sender name
    sender_label = lv_label_create(content_container);
    lv_obj_align(sender_label, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_font(sender_label, &heb_font_16, 0);
    lv_obj_set_style_text_color(sender_label, lv_color_hex(0xFFFFFF), 0);

    // Message content
    notification_content = lv_label_create(content_container);
    lv_label_set_long_mode(notification_content, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(notification_content, SCREEN_WIDTH - 50);
    lv_obj_align_to(notification_content, sender_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
    lv_obj_set_style_text_align(notification_content, LV_TEXT_ALIGN_AUTO, 0);
    lv_obj_set_style_text_font(notification_content, &heb_font_16, 0);
    lv_obj_set_style_text_color(notification_content, lv_color_hex(0xE0E0E0), 0);
}

static void create_bottom_info(void)
{
    // Secondary information (timestamp, etc.)
    secondary_info = lv_label_create(main_screen);
    lv_obj_align(secondary_info, LV_ALIGN_BOTTOM_MID, 0, -35);
    lv_obj_set_style_text_font(secondary_info, &heb_font_10, 0);
    lv_obj_set_style_text_color(secondary_info, lv_color_hex(0x969696), 0);

    // Counter (shows current/total)
    counter_label = lv_label_create(main_screen);
    lv_obj_align(counter_label, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_text_font(counter_label, &heb_font_10, 0);
    lv_obj_set_style_text_color(counter_label, lv_color_hex(0x969696), 0);

    // Undo message using LVGL arclabel widget
    undo_arclabel = lv_arclabel_create(main_screen);
    lv_arclabel_set_text(undo_arclabel, "Deleting... Tap to cancel");
    lv_arclabel_set_angle_start(undo_arclabel, 180);
    lv_arclabel_set_angle_size(undo_arclabel, 180);
    lv_arclabel_set_dir(undo_arclabel, LV_ARCLABEL_DIR_CLOCKWISE);
    lv_arclabel_set_radius(undo_arclabel, SCREEN_RADIUS - 15);
    lv_arclabel_set_center_offset_y(undo_arclabel, 0);
    lv_arclabel_set_text_vertical_align(undo_arclabel, LV_ARCLABEL_TEXT_ALIGN_CENTER);
    lv_arclabel_set_text_horizontal_align(undo_arclabel, LV_ARCLABEL_TEXT_ALIGN_CENTER);
    lv_arclabel_set_recolor(undo_arclabel, true);
    lv_obj_set_style_text_font(undo_arclabel, &heb_font_16, 0);
    lv_obj_set_style_text_color(undo_arclabel, lv_color_hex(0xFFAA00), 0);
    lv_obj_set_size(undo_arclabel, lv_obj_get_width(main_screen), lv_obj_get_height(main_screen));
    lv_obj_center(undo_arclabel);
    lv_obj_set_style_bg_color(undo_arclabel, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(undo_arclabel, LV_OPA_40, 0);
    lv_obj_add_flag(undo_arclabel, LV_OBJ_FLAG_HIDDEN); // Hidden by default
}

static void update_connection_status(connection_status_t status)
{
    lv_obj_set_style_bg_color(status_circle, status_colors[status], 0);
}

static void update_time(const char* time_str)
{
    lv_label_set_text(time_label, time_str);
}

static void update_notification_display(void)
{
    if (notification_count == 0) {
        lv_label_set_text(app_name_label, "No notifications");
        lv_label_set_text(sender_label, "");
        lv_label_set_text(notification_content, "All clear!");
        lv_label_set_text(secondary_info, "");
        lv_label_set_text(counter_label, "");
        lv_obj_add_flag(message_unread, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(app_icon, lv_color_hex(0x666666), 0);
        return;
    }

    notification_t* notif = &notifications[current_notification];

    // Update app info
    lv_label_set_text(app_name_label, notif->app_name);
    lv_obj_set_style_bg_color(app_icon, get_app_color(notif->app_name), 0);

    // Update sender with RTL support
    if (is_rtl_text(notif->sender)) {
        lv_obj_set_style_base_dir(sender_label, LV_BASE_DIR_RTL, 0);
    } else {
        lv_obj_set_style_base_dir(sender_label, LV_BASE_DIR_LTR, 0);
    }
    lv_label_set_text(sender_label, notif->sender);

    // Update read state
    if (notif->is_read) {
        lv_obj_add_flag(message_unread, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(message_unread, LV_OBJ_FLAG_HIDDEN);
    }

    // Sanitize and update content with RTL support
    static char sanitized_content[256];
    sanitize_text_for_display(notif->content, sanitized_content, sizeof(sanitized_content));

    if (is_rtl_text(sanitized_content)) {
        lv_obj_set_style_base_dir(notification_content, LV_BASE_DIR_RTL, 0);
        lv_obj_set_style_text_align(notification_content, LV_TEXT_ALIGN_RIGHT, 0);
    } else {
        lv_obj_set_style_base_dir(notification_content, LV_BASE_DIR_LTR, 0);
        lv_obj_set_style_text_align(notification_content, LV_TEXT_ALIGN_LEFT, 0);
    }
    lv_label_set_text(notification_content, sanitized_content);

    // Update secondary info with relative time
    static char time_str[32];
    if (rtc_format_relative_time(notif->timestamp, time_str, sizeof(time_str)) == 0) {
        lv_label_set_text(secondary_info, time_str);
    } else {
        lv_label_set_text(secondary_info, "unknown");
    }

    // Update counter
    static char counter_text[20];
    snprintf(counter_text, sizeof(counter_text), "%d of %d",
        current_notification + 1, notification_count);
    lv_label_set_text(counter_label, counter_text);
}

static void next_notification(void)
{
    if (notification_count > 0) {
        current_notification = (current_notification + 1) % notification_count;
        update_notification_display();
    }
}

static void prev_notification(void)
{
    if (notification_count > 0) {
        current_notification = (current_notification - 1 + notification_count) % notification_count;
        update_notification_display();
    }
}

static void mark_current_as_read(void)
{
    if (notification_count > 0) {
        notifications[current_notification].is_read = true;
        update_notification_display();
    }
}

static void delete_current_notification(void)
{
    if (notification_count == 0)
        return;

    // Start delete pending process
    delete_pending = true;
    delete_pending_index = current_notification;
    delete_timer_counter = 0;

    // Show undo message
    lv_obj_clear_flag(undo_arclabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(undo_arclabel);
}

static void undo_deletion(void)
{
    if (delete_pending && delete_pending_index >= 0) {
        delete_pending = false;
        delete_pending_index = -1;
        delete_timer_counter = 0;

        // Hide undo message
        lv_obj_add_flag(undo_arclabel, LV_OBJ_FLAG_HIDDEN);

        // Refresh display
        update_notification_display();
    }
}

static void complete_deletion(void)
{
    if (!delete_pending || delete_pending_index < 0)
        return;

    int index_to_delete = delete_pending_index;

    // Shift notifications down
    for (int i = index_to_delete; i < notification_count - 1; i++) {
        notifications[i] = notifications[i + 1];
    }

    notification_count--;

    // Adjust current notification index
    if (notification_count == 0) {
        current_notification = 0;
    } else if (current_notification >= notification_count) {
        current_notification = notification_count - 1;
    }

    // Clear delete pending state
    delete_pending = false;
    delete_pending_index = -1;
    delete_timer_counter = 0;

    // Hide undo message
    lv_obj_add_flag(undo_arclabel, LV_OBJ_FLAG_HIDDEN);

    // Update display
    update_notification_display();
}

static void handle_delete_timeout(void)
{
    if (delete_pending) {
        delete_timer_counter++;
        if (delete_timer_counter >= DELETE_TIMEOUT) {
            complete_deletion();
        }
    }
}

// Public API functions
void notifications_update_connection_status(connection_status_t status)
{
    update_connection_status(status);
}

void notifications_update_time(const char* time_str)
{
    update_time(time_str);
}

void notifications_add_notification_with_timestamp(const char* app_name, const char* sender,
    const char* content, uint32_t timestamp)
{
    if (notification_count >= MAX_NOTIFICATIONS) {
        // Remove oldest notification to make room
        for (int i = 0; i < notification_count - 1; i++) {
            notifications[i] = notifications[i + 1];
        }
        notification_count--;
        if (current_notification > 0) {
            current_notification--;
        }
    }

    // Add new notification
    notification_t* new_notif = &notifications[notification_count];
    strncpy(new_notif->app_name, app_name, sizeof(new_notif->app_name) - 1);
    new_notif->app_name[sizeof(new_notif->app_name) - 1] = '\0';

    strncpy(new_notif->sender, sender, sizeof(new_notif->sender) - 1);
    new_notif->sender[sizeof(new_notif->sender) - 1] = '\0';

    strncpy(new_notif->content, content, sizeof(new_notif->content) - 1);
    new_notif->content[sizeof(new_notif->content) - 1] = '\0';

    new_notif->timestamp = timestamp;
    new_notif->is_read = false;

    notification_count++;
    current_notification = notification_count - 1; // Show newest notification
    update_notification_display();
}

void notifications_add_notification(const char* app_name, const char* sender,
    const char* content, const char* timestamp)
{
    /* Legacy function - use current time if RTC is set */
    uint32_t ts = rtc_get_timestamp();
    if (ts == 0) {
        /* RTC not set, use a placeholder */
        ts = 1700000000;
    }

    notifications_add_notification_with_timestamp(app_name, sender, content, ts);
}

void notifications_clear_all(void)
{
    notification_count = 0;
    current_notification = 0;
    update_notification_display();
}

int notifications_get_unread_count(void)
{
    int count = 0;
    for (int i = 0; i < notification_count; i++) {
        if (!notifications[i].is_read) {
            count++;
        }
    }
    return count;
}

void notifications_handle_timers(void)
{
    static uint32_t last_update = 0;
    uint32_t now = k_uptime_get_32();

    /* Handle delete timeout */
    handle_delete_timeout();

    /* Update relative timestamps periodically (every 30 seconds) */
    if (now - last_update >= 30000) {
        /* Trigger display refresh to update relative times */
        if (notification_count > 0) {
            update_notification_display();
        }
        last_update = now;
    }
}

void create_notification_screen(void)
{
    // Initialize sample data
    init_sample_notifications();

    // Create main screen
    main_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(main_screen, lv_color_hex(0x000000), 0);

    // Create styles
    create_styles();

    // Create UI components
    create_top_bar();
    create_app_info();
    create_notification_content();
    create_bottom_info();

    // Enable gesture detection and add event handler
    lv_obj_add_event_cb(main_screen, screen_event_handler, LV_EVENT_ALL, NULL);
    lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_GESTURE_BUBBLE);

    // Initial display update
    update_connection_status(CONN_DISCONNECTED);

    // Update time from RTC if available
    char time_str[16];
    if (rtc_format_time(time_str, sizeof(time_str)) == 0) {
        update_time(time_str);
    } else {
        update_time("--:--");
    }

    update_notification_display();

    // Load the screen
    lv_scr_load(main_screen);
}

void demo_status_changes(void)
{
    static connection_status_t current_status = CONN_CONNECTED;
    static int counter = 0;
    static int demo_step = 0;

    counter++;

    // Handle delete timeout every cycle
    handle_delete_timeout();

    // Every 5 seconds, do something different
    if (counter % 50 == 0) {
        switch (demo_step % 6) {
        case 0:
            current_status = (current_status + 1) % 4;
            update_connection_status(current_status);
            break;
        case 1: {
            static char time_buf[10];
            int hours = 14 + (demo_step / 8) % 10;
            int minutes = 23 + (demo_step * 7) % 60;
            snprintf(time_buf, sizeof(time_buf), "%02d:%02d", hours, minutes);
            update_time(time_buf);
        } break;
        case 2:
            notifications_add_notification("Instagram", "Alice", "Liked your photo!", "now");
            break;
        case 3:
        case 4:
        case 5:
            break;
        }
        demo_step++;
    }
}
