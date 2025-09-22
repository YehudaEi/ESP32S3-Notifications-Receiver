/**
 * @file notifications.h
 * @brief Notifications Screen Management Header
 *
 * @author Yehuda@YehudaE.net
 */

#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Connection status enum
typedef enum {
    CONN_CONNECTED, // Green
    CONN_WEAK_SIGNAL, // Yellow
    CONN_CONNECTING, // Blue
    CONN_DISCONNECTED // Red
} connection_status_t;

/**
 * @brief Create the main notification screen
 *
 * This function creates the complete notification UI including:
 * - Top bar with time and connection status
 * - App info and notification content
 * - Touch gesture handlers
 * - Sample notifications for testing
 */
void create_notification_screen(void);

/**
 * @brief Update connection status indicator
 *
 * @param status Connection status to display
 */
void notifications_update_connection_status(connection_status_t status);

/**
 * @brief Update time display
 *
 * @param time_str Time string in HH:MM format
 */
void notifications_update_time(const char* time_str);

/**
 * @brief Add a new notification
 *
 * @param app_name Name of the app (max 31 chars)
 * @param sender Sender name (max 63 chars)
 * @param content Notification content (max 255 chars)
 * @param timestamp Time string (max 15 chars)
 */
void notifications_add_notification(const char* app_name, const char* sender,
    const char* content, const char* timestamp);

/**
 * @brief Clear all notifications
 */
void notifications_clear_all(void);

/**
 * @brief Get count of unread notifications
 *
 * @return Number of unread notifications
 */
int notifications_get_unread_count(void);

/**
 * @brief Handle internal timers (call in main loop)
 *
 * This handles delete timeout functionality
 */
void notifications_handle_timers(void);

/**
 * @brief Demo function for testing status changes
 *
 * Optional function for testing different UI states
 */
void demo_status_changes(void);

#ifdef __cplusplus
}
#endif

#endif /* NOTIFICATIONS_H */
