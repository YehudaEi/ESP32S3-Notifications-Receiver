package net.yehudae.esp32s3notificationsreceiver

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import java.text.SimpleDateFormat
import java.util.*

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(
    onBackClick: () -> Unit,
    bleService: BLEService?
) {
    val context = LocalContext.current
    val settings = remember { NotificationSettings(context) }
    
    var notifications by remember { mutableStateOf(listOf<NotificationData>()) }
    var connectedDeviceInfo by remember { mutableStateOf<ConnectedDeviceInfo?>(null) }
    var syncStatus by remember { mutableStateOf<SyncStatus?>(null) }
    var connectionStatus by remember { mutableStateOf("Disconnected") }
    
    LaunchedEffect(bleService) {
        try {
            bleService?.notifications?.collect { notificationList ->
                notifications = notificationList
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
    
    LaunchedEffect(bleService) {
        try {
            bleService?.connectedDeviceInfo?.collect { deviceInfo ->
                connectedDeviceInfo = deviceInfo
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    LaunchedEffect(bleService) {
        try {
            bleService?.syncStatus?.collect { status ->
                syncStatus = status
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    LaunchedEffect(bleService) {
        try {
            bleService?.connectionStatus?.collect { status ->
                connectionStatus = status
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    colors = listOf(
                        Color(0xFF667eea),
                        Color(0xFF764ba2)
                    )
                )
            )
    ) {
        // Top App Bar
        CenterAlignedTopAppBar(
            title = {
                Text(
                    "Settings & Statistics",
                    color = Color.White,
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold
                )
            },
            navigationIcon = {
                IconButton(onClick = onBackClick) {
                    Icon(
                        Icons.AutoMirrored.Filled.ArrowBack,
                        contentDescription = "Back",
                        tint = Color.White
                    )
                }
            },
            actions = {
                IconButton(
                    onClick = {
                        // Clear all notifications
                        bleService?.clearAllNotifications()
                    }
                ) {
                    Icon(
                        Icons.Default.ClearAll,
                        contentDescription = "Clear All",
                        tint = Color.White
                    )
                }
            },
            colors = TopAppBarDefaults.centerAlignedTopAppBarColors(
                containerColor = Color.Transparent
            )
        )

        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            // NEW: Sync Management Card
            item {
                SyncManagementCard(
                    syncStatus = syncStatus,
                    connectionStatus = connectionStatus,
                    onSyncExisting = { bleService?.readExistingNotifications() }
                )
            }

            // Statistics Card
            item {
                StatisticsCard(
                    notifications = notifications,
                    connectedDeviceInfo = connectedDeviceInfo
                )
            }
            
            // App Management Card
            item {
                AppManagementCard(
                    notifications = notifications,
                    settings = settings
                )
            }
            
            // Notification Settings Card
            item {
                NotificationSettingsCard(settings = settings)
            }
            
            // Recent Notifications by Category
            item {
                NotificationCategoriesCard(notifications = notifications)
            }
        }
    }
}

// NEW: Sync Management Card
@Composable
fun SyncManagementCard(
    syncStatus: SyncStatus?,
    connectionStatus: String,
    onSyncExisting: () -> Unit
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(16.dp),
        colors = CardDefaults.cardColors(
            containerColor = Color.White.copy(alpha = 0.95f)
        ),
        elevation = CardDefaults.cardElevation(defaultElevation = 8.dp)
    ) {
        Column(
            modifier = Modifier.padding(20.dp)
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.padding(bottom = 16.dp)
            ) {
                Icon(
                    Icons.Default.Sync,
                    contentDescription = null,
                    tint = Color(0xFF667eea),
                    modifier = Modifier.size(24.dp)
                )
                Spacer(Modifier.width(8.dp))
                Text(
                    "Sync Management",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    color = Color(0xFF333333)
                )
            }

            // Current sync status
            if (syncStatus != null) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    if (syncStatus.isInProgress) {
                        CircularProgressIndicator(
                            modifier = Modifier.size(20.dp),
                            color = Color(0xFFFF9800),
                            strokeWidth = 2.dp
                        )
                        Spacer(Modifier.width(12.dp))
                        Text(
                            text = "Syncing existing notifications...",
                            fontSize = 14.sp,
                            color = Color(0xFF666666)
                        )
                    } else {
                        Icon(
                            Icons.Default.CheckCircle,
                            contentDescription = null,
                            modifier = Modifier.size(20.dp),
                            tint = Color(0xFF4CAF50)
                        )
                        Spacer(Modifier.width(12.dp))
                        Column {
                            Text(
                                text = "Last sync: ${syncStatus.sentCount}/${syncStatus.processedCount} notifications sent",
                                fontSize = 14.sp,
                                color = Color(0xFF666666)
                            )
                            val duration = (syncStatus.endTime ?: System.currentTimeMillis()) - syncStatus.startTime
                            Text(
                                text = "Completed in ${duration / 1000}s",
                                fontSize = 12.sp,
                                color = Color(0xFF888888)
                            )
                        }
                    }
                }
                
                Spacer(modifier = Modifier.height(16.dp))
            }

            // Sync button and info
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(12.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Button(
                    onClick = onSyncExisting,
                    enabled = connectionStatus == "Ready" && syncStatus?.isInProgress != true,
                    colors = ButtonDefaults.buttonColors(
                        containerColor = Color(0xFF4CAF50)
                    ),
                    shape = RoundedCornerShape(12.dp)
                ) {
                    if (syncStatus?.isInProgress == true) {
                        CircularProgressIndicator(
                            modifier = Modifier.size(16.dp),
                            color = Color.White,
                            strokeWidth = 2.dp
                        )
                    } else {
                        Icon(
                            Icons.Default.Sync,
                            contentDescription = null,
                            modifier = Modifier.size(16.dp)
                        )
                    }
                    Spacer(Modifier.width(8.dp))
                    Text("Sync Existing")
                }
                
                Text(
                    text = if (connectionStatus == "Ready") 
                        "Reads all current Android notifications" 
                    else 
                        "Connect to ESP32S3 first",
                    fontSize = 12.sp,
                    color = Color(0xFF666666),
                    modifier = Modifier.weight(1f)
                )
            }

            Spacer(modifier = Modifier.height(16.dp))

            // Sync info
            Column {
                Text(
                    text = "How it works:",
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Medium,
                    color = Color(0xFF333333)
                )
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                    text = "• Reads all notifications currently in your Android notification panel\n• Filters them using your app settings and preferences\n• Sends them to your connected ESP32S3 device\n• Perfect for initial setup or missed notifications",
                    fontSize = 12.sp,
                    color = Color(0xFF666666),
                    lineHeight = 16.sp
                )
            }
        }
    }
}

@Composable
fun StatisticsCard(
    notifications: List<NotificationData>,
    connectedDeviceInfo: ConnectedDeviceInfo?
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(16.dp),
        colors = CardDefaults.cardColors(
            containerColor = Color.White.copy(alpha = 0.95f)
        ),
        elevation = CardDefaults.cardElevation(defaultElevation = 8.dp)
    ) {
        Column(
            modifier = Modifier.padding(20.dp)
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.padding(bottom = 16.dp)
            ) {
                Icon(
                    Icons.Default.Analytics,
                    contentDescription = null,
                    tint = Color(0xFF667eea),
                    modifier = Modifier.size(24.dp)
                )
                Spacer(Modifier.width(8.dp))
                Text(
                    "Statistics",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    color = Color(0xFF333333)
                )
            }
            
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceEvenly
            ) {
                StatisticItem(
                    title = "Total Notifications",
                    value = "${notifications.size}",
                    icon = Icons.Default.Notifications,
                    color = Color(0xFF4CAF50)
                )
                
                StatisticItem(
                    title = "Sent to Device",
                    value = "${connectedDeviceInfo?.notificationsSent ?: 0}",
                    icon = Icons.Default.Send,
                    color = Color(0xFF2196F3)
                )
                
                StatisticItem(
                    title = "Unique Apps",
                    value = "${notifications.distinctBy { it.appName }.size}",
                    icon = Icons.Default.Apps,
                    color = Color(0xFFFF9800)
                )
            }
            
            Spacer(modifier = Modifier.height(16.dp))
            
            // Today's notifications
            val today = SimpleDateFormat("dd/MM", Locale.getDefault()).format(Date())
            val todayNotifications = notifications.count { 
                it.timestamp.startsWith(today) || it.timestamp.contains(SimpleDateFormat("HH:mm", Locale.getDefault()).format(Date()).substring(0, 2))
            }
            
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text(
                    text = "Today's Notifications:",
                    fontSize = 14.sp,
                    color = Color(0xFF666666)
                )
                Text(
                    text = "$todayNotifications",
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Bold,
                    color = Color(0xFF333333)
                )
            }
            
            if (connectedDeviceInfo != null) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    Text(
                        text = "Connection Duration:",
                        fontSize = 14.sp,
                        color = Color(0xFF666666)
                    )
                    val duration = (System.currentTimeMillis() - connectedDeviceInfo.connectionTime) / 1000 / 60
                    Text(
                        text = "${duration}m",
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Bold,
                        color = Color(0xFF333333)
                    )
                }
            }
        }
    }
}

@Composable
fun StatisticItem(
    title: String,
    value: String,
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    color: Color
) {
    Column(
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Icon(
            icon,
            contentDescription = null,
            modifier = Modifier.size(32.dp),
            tint = color
        )
        Spacer(Modifier.height(8.dp))
        Text(
            text = value,
            fontSize = 20.sp,
            fontWeight = FontWeight.Bold,
            color = color
        )
        Text(
            text = title,
            fontSize = 12.sp,
            color = Color(0xFF666666),
            textAlign = TextAlign.Center
        )
    }
}

@Composable
fun AppManagementCard(
    notifications: List<NotificationData>,
    settings: NotificationSettings
) {
    var showAppList by remember { mutableStateOf(false) }
    
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(16.dp),
        colors = CardDefaults.cardColors(
            containerColor = Color.White.copy(alpha = 0.95f)
        ),
        elevation = CardDefaults.cardElevation(defaultElevation = 8.dp)
    ) {
        Column(
            modifier = Modifier.padding(20.dp)
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.padding(bottom = 16.dp)
            ) {
                Icon(
                    Icons.Default.Apps,
                    contentDescription = null,
                    tint = Color(0xFF667eea),
                    modifier = Modifier.size(24.dp)
                )
                Spacer(Modifier.width(8.dp))
                Text(
                    "App Management",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    color = Color(0xFF333333)
                )
                Spacer(Modifier.weight(1f))
                TextButton(
                    onClick = { showAppList = !showAppList }
                ) {
                    Text(if (showAppList) "Hide" else "Manage")
                }
            }
            
            if (showAppList) {
                val appNotifications = notifications.groupBy { it.appName }
                    .map { (appName, notifications) ->
                        AppNotificationInfo(
                            appName = appName,
                            count = notifications.size,
                            isEnabled = !settings.getBlockedApps().contains(appName),
                            isPriority = settings.getPriorityApps().contains(appName)
                        )
                    }
                    .sortedByDescending { it.count }
                
                appNotifications.forEach { appInfo ->
                    AppManagementItem(
                        appInfo = appInfo,
                        onToggleEnabled = { settings.toggleAppEnabled(appInfo.appName) },
                        onTogglePriority = { settings.toggleAppPriority(appInfo.appName) }
                    )
                    Spacer(Modifier.height(8.dp))
                }
            } else {
                Text(
                    text = "Tap 'Manage' to control which apps can send notifications to your ESP32S3",
                    fontSize = 14.sp,
                    color = Color(0xFF666666)
                )
            }
        }
    }
}

@Composable
fun AppManagementItem(
    appInfo: AppNotificationInfo,
    onToggleEnabled: () -> Unit,
    onTogglePriority: () -> Unit
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(
            modifier = Modifier.weight(1f)
        ) {
            Text(
                text = appInfo.appName,
                fontSize = 16.sp,
                fontWeight = FontWeight.Medium,
                color = Color(0xFF333333)
            )
            Text(
                text = "${appInfo.count} notifications",
                fontSize = 12.sp,
                color = Color(0xFF666666)
            )
        }
        
        if (appInfo.isPriority) {
            Icon(
                Icons.Default.Star,
                contentDescription = "Priority",
                modifier = Modifier.size(16.dp),
                tint = Color(0xFFFFD700)
            )
            Spacer(Modifier.width(8.dp))
        }
        
        Switch(
            checked = appInfo.isEnabled,
            onCheckedChange = { onToggleEnabled() },
            colors = SwitchDefaults.colors(
                checkedThumbColor = Color(0xFF4CAF50),
                checkedTrackColor = Color(0xFF4CAF50).copy(alpha = 0.5f)
            )
        )
    }
}

@Composable
fun NotificationSettingsCard(settings: NotificationSettings) {
    var quietHoursEnabled by remember { mutableStateOf(settings.isQuietHoursEnabled()) }
    var maxNotifications by remember { mutableStateOf(settings.getMaxNotifications()) }
    
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(16.dp),
        colors = CardDefaults.cardColors(
            containerColor = Color.White.copy(alpha = 0.95f)
        ),
        elevation = CardDefaults.cardElevation(defaultElevation = 8.dp)
    ) {
        Column(
            modifier = Modifier.padding(20.dp)
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.padding(bottom = 16.dp)
            ) {
                Icon(
                    Icons.Default.Settings,
                    contentDescription = null,
                    tint = Color(0xFF667eea),
                    modifier = Modifier.size(24.dp)
                )
                Spacer(Modifier.width(8.dp))
                Text(
                    "Notification Settings",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    color = Color(0xFF333333)
                )
            }
            
            // Quiet Hours
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column(
                    modifier = Modifier.weight(1f)
                ) {
                    Text(
                        text = "Quiet Hours",
                        fontSize = 16.sp,
                        fontWeight = FontWeight.Medium,
                        color = Color(0xFF333333)
                    )
                    Text(
                        text = "Don't send notifications during ${settings.getQuietHoursStart()}:00-${settings.getQuietHoursEnd()}:00",
                        fontSize = 12.sp,
                        color = Color(0xFF666666)
                    )
                }
                Switch(
                    checked = quietHoursEnabled,
                    onCheckedChange = { 
                        quietHoursEnabled = it
                        settings.setQuietHoursEnabled(it)
                    }
                )
            }
            
            Spacer(Modifier.height(16.dp))
            
            // Max Notifications
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column(
                    modifier = Modifier.weight(1f)
                ) {
                    Text(
                        text = "Max Stored Notifications",
                        fontSize = 16.sp,
                        fontWeight = FontWeight.Medium,
                        color = Color(0xFF333333)
                    )
                    Text(
                        text = "Currently: $maxNotifications",
                        fontSize = 12.sp,
                        color = Color(0xFF666666)
                    )
                }
            }
        }
    }
}

@Composable
fun NotificationCategoriesCard(notifications: List<NotificationData>) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(16.dp),
        colors = CardDefaults.cardColors(
            containerColor = Color.White.copy(alpha = 0.95f)
        ),
        elevation = CardDefaults.cardElevation(defaultElevation = 8.dp)
    ) {
        Column(
            modifier = Modifier.padding(20.dp)
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.padding(bottom = 16.dp)
            ) {
                Icon(
                    Icons.Default.Category,
                    contentDescription = null,
                    tint = Color(0xFF667eea),
                    modifier = Modifier.size(24.dp)
                )
                Spacer(Modifier.width(8.dp))
                Text(
                    "Notification Categories",
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    color = Color(0xFF333333)
                )
            }
            
            val categories = mapOf(
                "Messages" to listOf("whatsapp", "telegram", "signal", "messages", "sms"),
                "Social" to listOf("instagram", "facebook", "twitter", "snapchat", "tiktok"),
                "Email" to listOf("gmail", "outlook", "mail", "email"),
                "Calls" to listOf("phone", "dialer", "call"),
                "News" to listOf("news", "reddit", "medium"),
                "Other" to emptyList()
            )
            
            categories.forEach { (category, keywords) ->
                val categoryNotifications = notifications.filter { notification ->
                    if (keywords.isEmpty()) {
                        // "Other" category - notifications that don't match any other category
                        categories.entries.none { (_, otherKeywords) ->
                            otherKeywords.isNotEmpty() && otherKeywords.any { 
                                notification.appName.contains(it, ignoreCase = true)
                            }
                        }
                    } else {
                        keywords.any { notification.appName.contains(it, ignoreCase = true) }
                    }
                }
                
                if (categoryNotifications.isNotEmpty()) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        Text(
                            text = category,
                            fontSize = 14.sp,
                            color = Color(0xFF666666)
                        )
                        Text(
                            text = "${categoryNotifications.size}",
                            fontSize = 14.sp,
                            fontWeight = FontWeight.Bold,
                            color = Color(0xFF333333)
                        )
                    }
                    Spacer(Modifier.height(4.dp))
                }
            }
        }
    }
}

data class AppNotificationInfo(
    val appName: String,
    val count: Int,
    val isEnabled: Boolean,
    val isPriority: Boolean
)
