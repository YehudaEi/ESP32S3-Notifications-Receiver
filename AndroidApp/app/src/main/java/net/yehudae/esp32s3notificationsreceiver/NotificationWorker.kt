package net.yehudae.esp32s3notificationsreceiver

import android.content.Context
import android.content.Intent
import android.util.Log
import androidx.work.*
import kotlinx.coroutines.delay
import java.util.concurrent.TimeUnit

class NotificationWorker(
    context: Context,
    params: WorkerParameters
) : CoroutineWorker(context, params) {

    companion object {
        private const val TAG = "NotificationWorker"
        const val WORK_NAME = "notification_worker"
        
        fun enqueueWork(
            context: Context,
            notificationData: NotificationData,
            delay: Long = 0
        ) {
            try {
                val inputData = Data.Builder()
                    .putString("appName", notificationData.appName)
                    .putString("title", notificationData.title)
                    .putString("text", notificationData.text)
                    .putString("timestamp", notificationData.timestamp)
                    .build()

                val workRequest = if (delay > 0) {
                    OneTimeWorkRequestBuilder<NotificationWorker>()
                        .setInputData(inputData)
                        .setInitialDelay(delay, TimeUnit.MILLISECONDS)
                        .setConstraints(
                            Constraints.Builder()
                                .setRequiredNetworkType(NetworkType.NOT_REQUIRED)
                                .build()
                        )
                        .setBackoffCriteria(
                            BackoffPolicy.EXPONENTIAL,
                            10000, // 10 seconds minimum backoff
                            TimeUnit.MILLISECONDS
                        )
                        .build()
                } else {
                    OneTimeWorkRequestBuilder<NotificationWorker>()
                        .setInputData(inputData)
                        .setConstraints(
                            Constraints.Builder()
                                .setRequiredNetworkType(NetworkType.NOT_REQUIRED)
                                .build()
                        )
                        .setBackoffCriteria(
                            BackoffPolicy.EXPONENTIAL,
                            10000, // 10 seconds minimum backoff
                            TimeUnit.MILLISECONDS
                        )
                        .build()
                }

                WorkManager.getInstance(context)
                    .enqueueUniqueWork(
                        "${WORK_NAME}_${System.currentTimeMillis()}",
                        ExistingWorkPolicy.KEEP,
                        workRequest
                    )
                    
                Log.d(TAG, "Notification work enqueued: ${notificationData.appName}")
            } catch (e: Exception) {
                Log.e(TAG, "Error enqueuing work", e)
            }
        }
        
        fun startPeriodicSync(context: Context) {
            try {
                val workRequest = PeriodicWorkRequestBuilder<NotificationSyncWorker>(
                    15, TimeUnit.MINUTES // Minimum interval for periodic work
                )
                    .setConstraints(
                        Constraints.Builder()
                            .setRequiredNetworkType(NetworkType.NOT_REQUIRED)
                            .build()
                    )
                    .build()

                WorkManager.getInstance(context)
                    .enqueueUniquePeriodicWork(
                        "notification_sync_worker",
                        ExistingPeriodicWorkPolicy.KEEP,
                        workRequest
                    )
                    
                Log.d(TAG, "Periodic sync worker started")
            } catch (e: Exception) {
                Log.e(TAG, "Error starting periodic sync", e)
            }
        }
    }

    override suspend fun doWork(): Result {
        return try {
            val notificationData = NotificationData(
                appName = inputData.getString("appName") ?: "",
                title = inputData.getString("title") ?: "",
                text = inputData.getString("text") ?: "",
                timestamp = inputData.getString("timestamp") ?: ""
            )

            Log.d(TAG, "Processing notification: ${notificationData.appName} - ${notificationData.title}")

            // Send notification to BLE Service
            val intent = Intent(applicationContext, BLEService::class.java).apply {
                action = "SEND_NOTIFICATION"
                putExtra("notification_data", notificationData)
            }
            applicationContext.startService(intent)

            // Add some retry logic with delay
            var retryCount = 0
            val maxRetries = 3
            
            while (retryCount < maxRetries) {
                delay(1000) // Wait 1 second between retries
                
                // Here you could check if the notification was successfully sent
                // For now, we'll assume it worked after the first try
                break
            }

            Log.d(TAG, "Notification work completed successfully")
            Result.success()
        } catch (e: Exception) {
            Log.e(TAG, "Error in notification work", e)
            if (runAttemptCount < 3) {
                Result.retry()
            } else {
                Result.failure()
            }
        }
    }
}

class NotificationSyncWorker(
    context: Context,
    params: WorkerParameters
) : CoroutineWorker(context, params) {

    companion object {
        private const val TAG = "NotificationSyncWorker"
    }

    override suspend fun doWork(): Result {
        return try {
            Log.d(TAG, "Running periodic sync check")
            
            // Check if BLE service is running and connected
            val intent = Intent(applicationContext, BLEService::class.java).apply {
                action = "SYNC_CHECK"
            }
            applicationContext.startService(intent)
            
            Log.d(TAG, "Sync check completed")
            Result.success()
        } catch (e: Exception) {
            Log.e(TAG, "Error in sync work", e)
            Result.retry()
        }
    }
}
