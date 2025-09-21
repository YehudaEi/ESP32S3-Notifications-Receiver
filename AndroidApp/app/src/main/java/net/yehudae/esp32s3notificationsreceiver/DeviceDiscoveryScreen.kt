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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun DeviceDiscoveryScreen(
    onBackClick: () -> Unit,
    onDeviceClick: (BluetoothDevice) -> Unit,
    bleService: BLEService?
) {
    var discoveredDevices by remember { mutableStateOf(listOf<BluetoothDevice>()) }
    var isScanning by remember { mutableStateOf(false) }
    var connectionStatus by remember { mutableStateOf("Disconnected") }

    LaunchedEffect(bleService) {
        bleService?.connectionStatus?.collect { status ->
            connectionStatus = status
        }
    }

    LaunchedEffect(bleService) {
        bleService?.discoveredDevices?.collect { devices ->
            discoveredDevices = devices
        }
    }

    LaunchedEffect(bleService) {
        bleService?.scanningState?.collect { scanning ->
            isScanning = scanning
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
                    "Device Discovery",
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
                        if (isScanning) {
                            bleService?.stopScanning()
                        } else {
                            bleService?.startDeviceDiscovery()
                        }
                    }
                ) {
                    Icon(
                        if (isScanning) Icons.Default.Stop else Icons.Default.Search,
                        contentDescription = if (isScanning) "Stop Scan" else "Start Scan",
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
            // Scanning Status Card
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
                    if (isScanning) {
                        CircularProgressIndicator(
                            modifier = Modifier.size(32.dp),
                            color = Color(0xFF667eea)
                        )
                        Spacer(modifier = Modifier.height(8.dp))
                        Text(
                            text = "Scanning for devices...",
                            fontSize = 16.sp,
                            fontWeight = FontWeight.Medium,
                            color = Color(0xFF333333)
                        )
                    } else {
                        Icon(
                            Icons.Default.BluetoothSearching,
                            contentDescription = null,
                            modifier = Modifier.size(32.dp),
                            tint = Color(0xFF667eea)
                        )
                        Spacer(modifier = Modifier.height(8.dp))
                        Text(
                            text = "Tap search to discover devices",
                            fontSize = 16.sp,
                            fontWeight = FontWeight.Medium,
                            color = Color(0xFF333333)
                        )
                    }
                    
                    Spacer(modifier = Modifier.height(8.dp))
                    
                    Text(
                        text = "Found ${discoveredDevices.size} devices",
                        fontSize = 14.sp,
                        color = Color(0xFF666666)
                    )
                }
            }

            // Devices List
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
                            Icons.Default.Devices,
                            contentDescription = null,
                            tint = Color(0xFF667eea),
                            modifier = Modifier.size(24.dp)
                        )
                        Spacer(Modifier.width(8.dp))
                        Text(
                            "Available Devices",
                            fontSize = 18.sp,
                            fontWeight = FontWeight.Bold,
                            color = Color(0xFF333333)
                        )
                    }

                    if (discoveredDevices.isEmpty()) {
                        Column(
                            modifier = Modifier.fillMaxSize(),
                            horizontalAlignment = Alignment.CenterHorizontally,
                            verticalArrangement = Arrangement.Center
                        ) {
                            Icon(
                                Icons.Default.BluetoothDisabled,
                                contentDescription = null,
                                modifier = Modifier.size(64.dp),
                                tint = Color(0xFFBBBBBB)
                            )
                            Spacer(Modifier.height(16.dp))
                            Text(
                                "No devices found",
                                fontSize = 16.sp,
                                color = Color(0xFF888888),
                                textAlign = TextAlign.Center
                            )
                            Text(
                                "Make sure your ESP32S3 is in pairing mode",
                                fontSize = 14.sp,
                                color = Color(0xFFBBBBBB),
                                textAlign = TextAlign.Center
                            )
                        }
                    } else {
                        LazyColumn(
                            verticalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            items(discoveredDevices) { device ->
                                DeviceItem(
                                    device = device,
                                    onClick = { onDeviceClick(device) },
                                    isConnected = connectionStatus == "Connected" || connectionStatus == "Ready"
                                )
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun DeviceItem(
    device: BluetoothDevice,
    onClick: () -> Unit,
    isConnected: Boolean
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(12.dp),
        colors = CardDefaults.cardColors(
            containerColor = if (isConnected) Color(0xFFE8F5E8) else Color(0xFFF8F9FA)
        ),
        elevation = CardDefaults.cardElevation(defaultElevation = 2.dp),
        onClick = onClick
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Icon(
                Icons.Default.Bluetooth,
                contentDescription = null,
                modifier = Modifier.size(24.dp),
                tint = if (isConnected) Color(0xFF4CAF50) else Color(0xFF667eea)
            )
            
            Spacer(Modifier.width(12.dp))
            
            Column(
                modifier = Modifier.weight(1f)
            ) {
                Text(
                    text = device.name ?: "Unknown Device",
                    fontSize = 16.sp,
                    fontWeight = FontWeight.SemiBold,
                    color = Color(0xFF333333),
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                
                Text(
                    text = device.address,
                    fontSize = 14.sp,
                    color = Color(0xFF666666),
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                
                if (device.rssi != null) {
                    Text(
                        text = "Signal: ${device.rssi} dBm",
                        fontSize = 12.sp,
                        color = Color(0xFF888888)
                    )
                }
            }
            
            if (isConnected) {
                Icon(
                    Icons.Default.CheckCircle,
                    contentDescription = "Connected",
                    tint = Color(0xFF4CAF50),
                    modifier = Modifier.size(20.dp)
                )
            } else {
                Icon(
                    Icons.Default.ChevronRight,
                    contentDescription = "Connect",
                    tint = Color(0xFF999999),
                    modifier = Modifier.size(20.dp)
                )
            }
        }
    }
}
