package net.yehudae.esp32s3notificationsreceiver

import android.content.Intent
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import android.util.Log
import java.text.SimpleDateFormat
import java.util.*

class NotificationListener : NotificationListenerService() {

    companion object {
        private const val TAG = "NotificationListener"
    }
    
    private lateinit var settings: NotificationSettings

    override fun onCreate() {
        super.onCreate()
        settings = NotificationSettings(this)
    }

    override fun onNotificationPosted(sbn: StatusBarNotification) {
        try {
            val packageName = sbn.packageName
            
            // Use settings to check if app is enabled
            if (!settings.isAppEnabled(packageName)) {
                Log.d(TAG, "Notification from $packageName blocked by settings")
                return
            }
            
            // Skip ongoing notifications (like music players, navigation, etc.)
            if (sbn.isOngoing) {
                return
            }
            
            // Check quiet hours
            if (settings.isInQuietHours()) {
                Log.d(TAG, "Notification from $packageName blocked by quiet hours")
                return
            }
            
            val extras = sbn.notification.extras
            val title = extras.getString("android.title") ?: ""
            val text = extras.getString("android.text") ?: ""
            
            // Skip empty notifications
            if (title.isEmpty() && text.isEmpty()) {
                return
            }
            
            val appName = getAppName(packageName)
            val isPriority = settings.getPriorityApps().contains(packageName)
            
            val notificationData = NotificationData(
                appName = appName,
                title = title,
                text = text,
                timestamp = SimpleDateFormat("HH:mm", Locale.getDefault()).format(Date()),
                isPriority = isPriority,
                packageName = packageName
            )
            
            Log.d(TAG, "New notification: $appName - $title - $text ${if (isPriority) "(PRIORITY)" else ""}")
            
            // Send to BLE Service immediately (for real-time when connected)
            val intent = Intent(this, BLEService::class.java).apply {
                action = "SEND_NOTIFICATION"
                putExtra("notification_data", notificationData)
            }
            startService(intent)
            
            // ALSO queue with WorkManager for background reliability (Garmin-like behavior)
            // Priority notifications get shorter delay
            val delay = if (isPriority) 500L else 2000L
            NotificationWorker.enqueueWork(
                context = this,
                notificationData = notificationData,
                delay = delay
            )
            
            Log.d(TAG, "Notification queued for background delivery ${if (isPriority) "(priority)" else ""}")
            
        } catch (e: Exception) {
            Log.e(TAG, "Error processing notification", e)
        }
    }

    override fun onNotificationRemoved(sbn: StatusBarNotification) {
        // Handle notification removal if needed
        Log.d(TAG, "Notification removed: ${sbn.packageName}")
    }

    override fun onListenerConnected() {
        super.onListenerConnected()
        Log.d(TAG, "Notification listener connected")
        
        // Start periodic sync worker for background reliability
        NotificationWorker.startPeriodicSync(this)
        Log.d(TAG, "Background sync worker started")
    }

    override fun onListenerDisconnected() {
        super.onListenerDisconnected()
        Log.d(TAG, "Notification listener disconnected")
    }

    private fun getAppName(packageName: String): String {
        return try {
            val packageManager = packageManager
            val applicationInfo = packageManager.getApplicationInfo(packageName, 0)
            packageManager.getApplicationLabel(applicationInfo).toString()
        } catch (e: Exception) {
            // Fallback to extracting from package name
            packageName.split(".").lastOrNull()?.replaceFirstChar { 
                if (it.isLowerCase()) it.titlecase(Locale.getDefault()) else it.toString() 
            } ?: packageName
        }
    }
}
