package net.yehudae.esp32s3notificationsreceiver

import android.os.Parcelable
import kotlinx.parcelize.Parcelize

@Parcelize
data class NotificationData(
    val appName: String,
    val title: String,
    val text: String,
    val timestamp: String
) : Parcelable
