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
        const val ACTION_READ_EXISTING = "ACTION_READ_EXISTING"
    }
    
    private lateinit var settings: NotificationSettings

    override fun onCreate() {
        super.onCreate()
        settings = NotificationSettings(this)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_READ_EXISTING -> {
                readExistingNotifications()
            }
        }
        return super.onStartCommand(intent, flags, startId)
    }

    override fun onNotificationPosted(sbn: StatusBarNotification) {
        try {
            processNotification(sbn, isExisting = false)
        } catch (e: Exception) {
            Log.e(TAG, "Error processing new notification", e)
        }
    }

    /**
     * Reads all currently active notifications and sends them to ESP32S3
     */
    fun readExistingNotifications() {
        try {
            Log.d(TAG, "Reading existing notifications...")
            
            val activeNotifications = activeNotifications
            if (activeNotifications == null) {
                Log.w(TAG, "Cannot access active notifications - permission may be missing")
                return
            }

            Log.d(TAG, "Found ${activeNotifications.size} active notifications")
            
            var processedCount = 0
            var sentCount = 0
            
            // Process each active notification
            activeNotifications.forEach { sbn ->
                try {
                    val wasProcessed = processNotification(sbn, isExisting = true)
                    processedCount++
                    if (wasProcessed) sentCount++
                } catch (e: Exception) {
                    Log.e(TAG, "Error processing existing notification from ${sbn.packageName}", e)
                }
            }
            
            Log.d(TAG, "Existing notifications sync completed: $sentCount/$processedCount sent to ESP32S3")
            
            // Notify BLE Service about sync completion
            val intent = Intent(this, BLEService::class.java).apply {
                action = "SYNC_COMPLETED"
                putExtra("processed_count", processedCount)
                putExtra("sent_count", sentCount)
            }
            startService(intent)
            
        } catch (e: Exception) {
            Log.e(TAG, "Error reading existing notifications", e)
        }
    }

    /**
     * Process a notification (either new or existing)
     * @param sbn StatusBarNotification to process
     * @param isExisting true if this is an existing notification, false if new
     * @return true if notification was sent to ESP32S3, false if filtered out
     */
    private fun processNotification(sbn: StatusBarNotification, isExisting: Boolean): Boolean {
        try {
            val packageName = sbn.packageName
            
            // Use settings to check if app is enabled
            if (!settings.isAppEnabled(packageName)) {
                Log.d(TAG, "Notification from $packageName blocked by settings")
                return false
            }
            
            // Skip ongoing notifications (like music players, navigation, etc.)
            if (sbn.isOngoing) {
                Log.d(TAG, "Skipping ongoing notification from $packageName")
                return false
            }
            
            // Check quiet hours (but allow existing notifications to be synced)
            if (!isExisting && settings.isInQuietHours()) {
                Log.d(TAG, "Notification from $packageName blocked by quiet hours")
                return false
            }
            
            val extras = sbn.notification.extras
            val title = extras.getString("android.title") ?: ""
            val text = extras.getString("android.text") ?: ""
            
            // Skip empty notifications
            if (title.isEmpty() && text.isEmpty()) {
                Log.d(TAG, "Skipping empty notification from $packageName")
                return false
            }
            
            val appName = getAppName(packageName)
            val isPriority = settings.getPriorityApps().contains(packageName)
            
            // For existing notifications, use the notification's post time if available
            val timestamp = if (isExisting && sbn.postTime > 0) {
                SimpleDateFormat("HH:mm", Locale.getDefault()).format(Date(sbn.postTime))
            } else {
                SimpleDateFormat("HH:mm", Locale.getDefault()).format(Date())
            }
            
            val notificationData = NotificationData(
                appName = appName,
                title = title,
                text = text,
                timestamp = timestamp,
                isPriority = isPriority,
                packageName = packageName
            )
            
            Log.d(TAG, "${if (isExisting) "Existing" else "New"} notification: $appName - $title ${if (isPriority) "(PRIORITY)" else ""}")
            
            // Send to BLE Service immediately (for real-time when connected)
            val intent = Intent(this, BLEService::class.java).apply {
                action = "SEND_NOTIFICATION"
                putExtra("notification_data", notificationData)
                putExtra("is_existing", isExisting)
            }
            startService(intent)
            
            // For new notifications, also queue with WorkManager for background reliability
            if (!isExisting) {
                // Priority notifications get shorter delay
                val delay = if (isPriority) 500L else 2000L
                NotificationWorker.enqueueWork(
                    context = this,
                    notificationData = notificationData,
                    delay = delay
                )
                
                Log.d(TAG, "New notification queued for background delivery ${if (isPriority) "(priority)" else ""}")
            }
            
            return true
            
        } catch (e: Exception) {
            Log.e(TAG, "Error processing notification", e)
            return false
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
