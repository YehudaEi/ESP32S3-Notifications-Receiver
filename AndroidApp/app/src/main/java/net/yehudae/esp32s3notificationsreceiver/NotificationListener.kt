package net.yehudae.esp32s3notificationsreceiver

import android.content.Intent
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import java.text.SimpleDateFormat
import java.util.*

class NotificationListener : NotificationListenerService() {

    override fun onNotificationPosted(sbn: StatusBarNotification) {
        val packageName = sbn.packageName
        val extras = sbn.notification.extras
        
        val title = extras.getString("android.title") ?: ""
        val text = extras.getString("android.text") ?: ""
        val appName = packageName.split(".").lastOrNull() ?: packageName
        
        val notificationData = NotificationData(
            appName = appName.replaceFirstChar { 
                if (it.isLowerCase()) it.titlecase(Locale.getDefault()) else it.toString() 
            },
            title = title,
            text = text,
            timestamp = SimpleDateFormat("HH:mm", Locale.getDefault()).format(Date())
        )
        
        // Send to BLE Service
        val intent = Intent(this, BLEService::class.java).apply {
            action = "SEND_NOTIFICATION"
            putExtra("notification_data", notificationData)
        }
        startService(intent)
    }

    override fun onNotificationRemoved(sbn: StatusBarNotification) {
        // Handle notification removal if needed
    }
}
