package net.yehudae.esp32s3notificationsreceiver

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.IBinder
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import net.yehudae.esp32s3notificationsreceiver.ui.theme.ESP32S3NotificationsReceiverTheme
import java.text.SimpleDateFormat
import java.util.*

class MainActivity : ComponentActivity() {
    private var bluetoothAdapter: BluetoothAdapter? = null
    private var bleService: BLEService? = null
    private var isBound = false
    private var currentScreen by mutableStateOf("main")
    
    private val serviceConnection = object : ServiceConnection {
        override fun onServiceConnected(className: ComponentName, service: IBinder) {
            try {
                val binder = service as BLEService.LocalBinder
                bleService = binder.getService()
                isBound = true
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }

        override fun onServiceDisconnected(arg0: ComponentName) {
            isBound = false
        }
    }

    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        // Handle permission results
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        
        try {
            val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
            bluetoothAdapter = bluetoothManager.adapter
            
            checkPermissions()
            
            setContent {
                ESP32S3NotificationsReceiverTheme {
                    when (currentScreen) {
                        "main" -> MainScreen(
                            onConnectESP32 = { connectToESP32() },
                            onOpenNotificationSettings = { openNotificationSettings() },
                            onNavigateToDevices = { currentScreen = "devices" },
                            onNavigateToSettings = { currentScreen = "settings" },
                            bleService = bleService
                        )
                        "devices" -> DeviceDiscoveryScreen(
                            onBackClick = { currentScreen = "main" },
                            onDeviceClick = { device -> 
                                bleService?.connectToDevice(device)
                                currentScreen = "main"
                            },
                            bleService = bleService
                        )
                        "settings" -> SettingsScreen(
                            onBackClick = { currentScreen = "main" },
                            bleService = bleService
                        )
                    }
                }
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    override fun onStart() {
        super.onStart()
        try {
            Intent(this, BLEService::class.java).also { intent ->
                startService(intent)
                bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE)
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    override fun onStop() {
        super.onStop()
        try {
            if (isBound) {
                unbindService(serviceConnection)
                isBound = false
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun checkPermissions() {
        try {
            val permissions = mutableListOf<String>()
            
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                permissions.addAll(listOf(
                    Manifest.permission.BLUETOOTH_SCAN,
                    Manifest.permission.BLUETOOTH_CONNECT
                ))
            } else {
                permissions.addAll(listOf(
                    Manifest.permission.BLUETOOTH,
                    Manifest.permission.BLUETOOTH_ADMIN
                ))
            }
            
            permissions.addAll(listOf(
                Manifest.permission.ACCESS_FINE_LOCATION,
                Manifest.permission.ACCESS_COARSE_LOCATION
            ))
            
            val missingPermissions = permissions.filter {
                ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
            }
            
            if (missingPermissions.isNotEmpty()) {
                requestPermissionLauncher.launch(missingPermissions.toTypedArray())
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun connectToESP32() {
        try {
            bleService?.startScanning()
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun openNotificationSettings() {
        try {
            val intent = Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)
            startActivity(intent)
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen(
    onConnectESP32: () -> Unit,
    onOpenNotificationSettings: () -> Unit,
    onNavigateToDevices: () -> Unit,
    onNavigateToSettings: () -> Unit,
    bleService: BLEService?
) {
    var connectionStatus by remember { mutableStateOf("Disconnected") }
    var notifications by remember { mutableStateOf(listOf<NotificationData>()) }
    var connectedDeviceInfo by remember { mutableStateOf<ConnectedDeviceInfo?>(null) }
    
    LaunchedEffect(bleService) {
        try {
            bleService?.connectionStatus?.collect { status ->
                connectionStatus = status
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }
    
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
                    "ESP32S3 Notifier",
                    color = Color.White,
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold
                )
            },
            actions = {
                IconButton(onClick = onNavigateToSettings) {
                    Icon(
                        Icons.Default.Analytics,
                        contentDescription = "Statistics & Settings",
                        tint = Color.White
                    )
                }
                IconButton(onClick = onNavigateToDevices) {
                    Icon(
                        Icons.Default.Devices,
                        contentDescription = "Devices",
                        tint = Color.White
                    )
                }
            },
            colors = TopAppBarDefaults.centerAlignedTopAppBarColors(
                containerColor = Color.Transparent
            )
        )

        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(16.dp)
        ) {
            // Connected Device Info Card (NEW)
            if (connectedDeviceInfo != null) {
                ConnectedDeviceCard(
                    deviceInfo = connectedDeviceInfo!!,
                    onDisconnect = { bleService?.disconnectDevice() }
                )
                Spacer(modifier = Modifier.height(16.dp))
            }

            // Connection Status Card
            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(bottom = 16.dp),
                shape = RoundedCornerShape(16.dp),
                colors = CardDefaults.cardColors(
                    containerColor = Color.White.copy(alpha = 0.95f)
                ),
                elevation = CardDefaults.cardElevation(defaultElevation = 8.dp)
            ) {
                Column(
                    modifier = Modifier.padding(20.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Icon(
                        imageVector = when (connectionStatus) {
                            "Connected", "Ready" -> Icons.Default.BluetoothConnected
                            "Connecting..." -> Icons.Default.Bluetooth
                            "Scanning..." -> Icons.Default.BluetoothSearching
                            else -> Icons.Default.Bluetooth
                        },
                        contentDescription = "Bluetooth",
                        modifier = Modifier.size(48.dp),
                        tint = when (connectionStatus) {
                            "Connected", "Ready" -> Color(0xFF4CAF50)
                            "Connecting...", "Scanning..." -> Color(0xFFFF9800)
                            else -> Color(0xFF757575)
                        }
                    )
                    
                    Spacer(modifier = Modifier.height(8.dp))
                    
                    Text(
                        text = "ESP32S3 Status",
                        fontSize = 16.sp,
                        fontWeight = FontWeight.Medium,
                        color = Color(0xFF333333)
                    )
                    
                    Text(
                        text = connectionStatus,
                        fontSize = 18.sp,
                        fontWeight = FontWeight.Bold,
                        color = when (connectionStatus) {
                            "Connected", "Ready" -> Color(0xFF4CAF50)
                            "Connecting...", "Scanning..." -> Color(0xFFFF9800)
                            else -> Color(0xFF757575)
                        }
                    )
                    
                    // Background Worker Status (NEW)
                    Spacer(modifier = Modifier.height(8.dp))
                    Row(
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(
                            Icons.Default.WorkOutline,
                            contentDescription = null,
                            modifier = Modifier.size(16.dp),
                            tint = Color(0xFF4CAF50)
                        )
                        Spacer(Modifier.width(4.dp))
                        Text(
                            text = "Background worker active",
                            fontSize = 12.sp,
                            color = Color(0xFF4CAF50),
                            fontWeight = FontWeight.Medium
                        )
                    }
                    
                    Spacer(modifier = Modifier.height(16.dp))
                    
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(12.dp)
                    ) {
                        Button(
                            onClick = onConnectESP32,
                            modifier = Modifier.weight(1f),
                            colors = ButtonDefaults.buttonColors(
                                containerColor = Color(0xFF667eea)
                            ),
                            shape = RoundedCornerShape(12.dp)
                        ) {
                            Icon(
                                Icons.Default.Refresh,
                                contentDescription = null,
                                modifier = Modifier.size(16.dp)
                            )
                            Spacer(Modifier.width(8.dp))
                            Text("Scan")
                        }
                    }

                    Spacer(modifier = Modifier.height(16.dp))

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(12.dp)
                    ) {
                        OutlinedButton(
                            onClick = onNavigateToDevices,
                            modifier = Modifier.weight(1f),
                            shape = RoundedCornerShape(12.dp)
                        ) {
                            Icon(
                                Icons.Default.Devices,
                                contentDescription = null,
                                modifier = Modifier.size(16.dp)
                            )
                            Spacer(Modifier.width(8.dp))
                            Text("Devices")
                        }
                        
                        OutlinedButton(
                            onClick = onOpenNotificationSettings,
                            modifier = Modifier.weight(1f),
                            shape = RoundedCornerShape(12.dp)
                        ) {
                            Icon(
                                Icons.Default.Settings,
                                contentDescription = null,
                                modifier = Modifier.size(16.dp)
                            )
                            Spacer(Modifier.width(8.dp))
                            Text("Settings")
                        }
                    }
                }
            }

            // Notifications Section
            Card(
                modifier = Modifier.fillMaxSize(),
                shape = RoundedCornerShape(topStart = 16.dp, topEnd = 16.dp),
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
                            Icons.Default.Notifications,
                            contentDescription = null,
                            tint = Color(0xFF667eea),
                            modifier = Modifier.size(24.dp)
                        )
                        Spacer(Modifier.width(8.dp))
                        Text(
                            "Recent Notifications",
                            fontSize = 18.sp,
                            fontWeight = FontWeight.Bold,
                            color = Color(0xFF333333)
                        )
                        Spacer(Modifier.weight(1f))
                        Text(
                            "${notifications.size}",
                            fontSize = 14.sp,
                            color = Color(0xFF667eea),
                            fontWeight = FontWeight.Bold
                        )
                    }

                    if (notifications.isEmpty()) {
                        Column(
                            modifier = Modifier.fillMaxSize(),
                            horizontalAlignment = Alignment.CenterHorizontally,
                            verticalArrangement = Arrangement.Center
                        ) {
                            Icon(
                                Icons.Default.NotificationsOff,
                                contentDescription = null,
                                modifier = Modifier.size(64.dp),
                                tint = Color(0xFFBBBBBB)
                            )
                            Spacer(Modifier.height(16.dp))
                            Text(
                                "No notifications yet",
                                fontSize = 16.sp,
                                color = Color(0xFF888888),
                                textAlign = TextAlign.Center
                            )
                            Text(
                                "Make sure notification access is enabled",
                                fontSize = 14.sp,
                                color = Color(0xFFBBBBBB),
                                textAlign = TextAlign.Center
                            )
                        }
                    } else {
                        LazyColumn(
                            verticalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            items(notifications) { notification ->
                                NotificationItem(notification = notification)
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun ConnectedDeviceCard(
    deviceInfo: ConnectedDeviceInfo,
    onDisconnect: () -> Unit
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(16.dp),
        colors = CardDefaults.cardColors(
            containerColor = Color(0xFFE8F5E8).copy(alpha = 0.95f)
        ),
        elevation = CardDefaults.cardElevation(defaultElevation = 8.dp)
    ) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.fillMaxWidth()
            ) {
                Icon(
                    Icons.Default.BluetoothConnected,
                    contentDescription = null,
                    modifier = Modifier.size(24.dp),
                    tint = Color(0xFF4CAF50)
                )
                Spacer(Modifier.width(12.dp))
                Column(
                    modifier = Modifier.weight(1f)
                ) {
                    Text(
                        text = deviceInfo.name,
                        fontSize = 16.sp,
                        fontWeight = FontWeight.Bold,
                        color = Color(0xFF333333),
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                    Text(
                        text = deviceInfo.address,
                        fontSize = 14.sp,
                        color = Color(0xFF666666)
                    )
                }
                IconButton(onClick = onDisconnect) {
                    Icon(
                        Icons.Default.Close,
                        contentDescription = "Disconnect",
                        tint = Color(0xFF666666)
                    )
                }
            }
            
            Spacer(modifier = Modifier.height(12.dp))
            
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Column {
                    Text(
                        text = "Connected Since",
                        fontSize = 12.sp,
                        color = Color(0xFF666666)
                    )
                    Text(
                        text = SimpleDateFormat("HH:mm:ss", Locale.getDefault())
                            .format(Date(deviceInfo.connectionTime)),
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Medium,
                        color = Color(0xFF333333)
                    )
                }
                Column {
                    Text(
                        text = "Notifications Sent",
                        fontSize = 12.sp,
                        color = Color(0xFF666666)
                    )
                    Text(
                        text = "${deviceInfo.notificationsSent}",
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Medium,
                        color = Color(0xFF4CAF50)
                    )
                }
                Column {
                    Text(
                        text = "Status",
                        fontSize = 12.sp,
                        color = Color(0xFF666666)
                    )
                    Text(
                        text = deviceInfo.status,
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Medium,
                        color = when (deviceInfo.status) {
                            "Ready" -> Color(0xFF4CAF50)
                            "Connected" -> Color(0xFF4CAF50)
                            else -> Color(0xFFFF9800)
                        }
                    )
                }
            }
        }
    }
}

@Composable
fun NotificationItem(notification: NotificationData) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(12.dp),
        colors = CardDefaults.cardColors(
            containerColor = if (notification.isPriority) 
                Color(0xFFFFF3E0) else Color(0xFFF8F9FA)
        ),
        elevation = CardDefaults.cardElevation(defaultElevation = 2.dp)
    ) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(
                    Icons.Default.Circle,
                    contentDescription = null,
                    modifier = Modifier.size(8.dp),
                    tint = if (notification.isPriority) Color(0xFFFF9800) else Color(0xFF4CAF50)
                )
                Spacer(Modifier.width(8.dp))
                Text(
                    text = notification.appName,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Medium,
                    color = Color(0xFF667eea)
                )
                Spacer(Modifier.weight(1f))
                
                // Priority indicator
                if (notification.isPriority) {
                    Icon(
                        Icons.Default.Star,
                        contentDescription = "Priority",
                        modifier = Modifier.size(16.dp),
                        tint = Color(0xFFFF9800)
                    )
                    Spacer(Modifier.width(8.dp))
                }
                
                Text(
                    text = notification.timestamp,
                    fontSize = 12.sp,
                    color = Color(0xFF888888)
                )
            }
            
            Spacer(Modifier.height(8.dp))
            
            if (notification.title.isNotEmpty()) {
                Text(
                    text = notification.title,
                    fontSize = 15.sp,
                    fontWeight = FontWeight.SemiBold,
                    color = Color(0xFF333333)
                )
            }
            
            if (notification.text.isNotEmpty()) {
                Text(
                    text = notification.text,
                    fontSize = 14.sp,
                    color = Color(0xFF666666),
                    modifier = Modifier.padding(top = 4.dp),
                    maxLines = 3,
                    overflow = TextOverflow.Ellipsis
                )
            }
        }
    }
}
