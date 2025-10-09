package net.yehudae.esp32s3notificationsreceiver

import android.app.Service
import android.bluetooth.*
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Binder
import android.os.IBinder
import android.util.Log
import androidx.core.app.ActivityCompat
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import java.util.*

class BLEService : Service() {
    private val binder = LocalBinder()
    private var bluetoothAdapter: BluetoothAdapter? = null
    private var bluetoothLeScanner: BluetoothLeScanner? = null
    private var bluetoothGatt: BluetoothGatt? = null
    private var notificationCharacteristic: BluetoothGattCharacteristic? = null
    private var connectedDevice: android.bluetooth.BluetoothDevice? = null
    private var currentMtu = 23 // Default BLE MTU
    
    // ESP32 Service and Characteristic UUIDs
    private val SERVICE_UUID = UUID.fromString("12345678-1234-1234-1234-123456789abc")
    private val CHARACTERISTIC_UUID = UUID.fromString("87654321-4321-4321-4321-cba987654321")
    
    private val _connectionStatus = MutableStateFlow("Disconnected")
    val connectionStatus: StateFlow<String> = _connectionStatus
    
    private val _notifications = MutableStateFlow<List<NotificationData>>(emptyList())
    val notifications: StateFlow<List<NotificationData>> = _notifications

    private val _discoveredDevices = MutableStateFlow<List<BluetoothDevice>>(emptyList())
    val discoveredDevices: StateFlow<List<BluetoothDevice>> = _discoveredDevices

    private val _scanningState = MutableStateFlow(false)
    val scanningState: StateFlow<Boolean> = _scanningState

    private val _connectedDeviceInfo = MutableStateFlow<ConnectedDeviceInfo?>(null)
    val connectedDeviceInfo: StateFlow<ConnectedDeviceInfo?> = _connectedDeviceInfo

    private val _syncStatus = MutableStateFlow<SyncStatus?>(null)
    val syncStatus: StateFlow<SyncStatus?> = _syncStatus

    private val notificationQueue = mutableListOf<NotificationData>()
    private var lastConnectionTime: Long = 0
    private var totalNotificationsSent: Int = 0

    // Protocol constants
    private val CMD_ADD_NOTIFICATION: Byte = 0x01
    private val CMD_REMOVE_NOTIFICATION: Byte = 0x02
    private val CMD_CLEAR_ALL: Byte = 0x03
    private val CMD_ACTION: Byte = 0x04

    companion object {
        private const val TAG = "BLEService"
        private const val MAX_PACKET_SIZE = 240 // Safe packet size for most devices
    }

    inner class LocalBinder : Binder() {
        fun getService(): BLEService = this@BLEService
    }

    override fun onBind(intent: Intent): IBinder {
        return binder
    }

    override fun onCreate() {
        super.onCreate()
        try {
            val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
            bluetoothAdapter = bluetoothManager.adapter
            bluetoothLeScanner = bluetoothAdapter?.bluetoothLeScanner
            
            NotificationWorker.startPeriodicSync(this)
            
            Log.d(TAG, "BLE Service created with background worker support")
        } catch (e: Exception) {
            Log.e(TAG, "Error creating BLE service", e)
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        try {
            when (intent?.action) {
                "SEND_NOTIFICATION" -> {
                    val notificationData = intent.getParcelableExtra<NotificationData>("notification_data")
                    val isExisting = intent.getBooleanExtra("is_existing", false)
                    notificationData?.let { sendNotificationToESP32(it, isExisting) }
                }
                "SYNC_CHECK" -> {
                    checkConnectionAndSync()
                }
                "READ_EXISTING_NOTIFICATIONS" -> {
                    readExistingNotifications()
                }
                "SYNC_COMPLETED" -> {
                    val processedCount = intent.getIntExtra("processed_count", 0)
                    val sentCount = intent.getIntExtra("sent_count", 0)
                    handleSyncCompleted(processedCount, sentCount)
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error in onStartCommand", e)
        }
        return START_STICKY
    }

    fun readExistingNotifications() {
        try {
            Log.d(TAG, "Initiating existing notifications sync...")
            
            _syncStatus.value = SyncStatus(
                isInProgress = true,
                processedCount = 0,
                sentCount = 0,
                startTime = System.currentTimeMillis()
            )
            
            val intent = Intent(this, NotificationListener::class.java).apply {
                action = NotificationListener.ACTION_READ_EXISTING
            }
            startService(intent)
            
        } catch (e: Exception) {
            Log.e(TAG, "Error initiating existing notifications sync", e)
            _syncStatus.value = null
        }
    }

    private fun handleSyncCompleted(processedCount: Int, sentCount: Int) {
        try {
            val currentSync = _syncStatus.value
            if (currentSync != null) {
                _syncStatus.value = currentSync.copy(
                    isInProgress = false,
                    processedCount = processedCount,
                    sentCount = sentCount,
                    endTime = System.currentTimeMillis()
                )
                
                Log.d(TAG, "Sync completed: $sentCount/$processedCount notifications sent")
                
                android.os.Handler(mainLooper).postDelayed({
                    _syncStatus.value = null
                }, 5000)
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error handling sync completion", e)
        }
    }

    fun startScanning() {
        startDeviceDiscovery()
    }

    fun startDeviceDiscovery() {
        try {
            if (!hasBluetoothPermissions()) {
                _connectionStatus.value = "Permission required"
                return
            }
            
            _scanningState.value = true
            _discoveredDevices.value = emptyList()
            _connectionStatus.value = "Scanning..."
            bluetoothLeScanner?.startScan(deviceDiscoveryCallback)
            
            android.os.Handler(mainLooper).postDelayed({
                stopScanning()
            }, 30000)
        } catch (e: Exception) {
            Log.e(TAG, "Error starting device discovery", e)
            _connectionStatus.value = "Scan failed"
            _scanningState.value = false
        }
    }

    fun stopScanning() {
        try {
            if (hasBluetoothPermissions()) {
                bluetoothLeScanner?.stopScan(deviceDiscoveryCallback)
            }
            _scanningState.value = false
            if (_connectionStatus.value == "Scanning...") {
                _connectionStatus.value = "Disconnected"
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error stopping scan", e)
        }
    }

    fun disconnectDevice() {
        try {
            bluetoothGatt?.disconnect()
            connectedDevice = null
            _connectedDeviceInfo.value = null
            _connectionStatus.value = "Disconnected"
            Log.d(TAG, "Device disconnected manually")
        } catch (e: Exception) {
            Log.e(TAG, "Error disconnecting device", e)
        }
    }

    private val deviceDiscoveryCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            try {
                val device = result.device
                val rssi = result.rssi
                
                if (!hasBluetoothPermissions()) {
                    return
                }
                
                val bluetoothDevice = BluetoothDevice(
                    name = device.name,
                    address = device.address,
                    rssi = rssi,
                    device = device
                )
                
                val currentDevices = _discoveredDevices.value.toMutableList()
                val existingDevice = currentDevices.find { it.address == device.address }
                
                if (existingDevice == null) {
                    currentDevices.add(bluetoothDevice)
                    _discoveredDevices.value = currentDevices
                } else {
                    val index = currentDevices.indexOf(existingDevice)
                    currentDevices[index] = bluetoothDevice
                    _discoveredDevices.value = currentDevices
                }

                val deviceName = device.name
                if (deviceName?.contains("YNotificator", ignoreCase = true) == true ||
                    deviceName?.contains("ESP32", ignoreCase = true) == true) {
                    Log.d(TAG, "Found target device: $deviceName")
                }
                
            } catch (e: Exception) {
                Log.e(TAG, "Error in scan result", e)
            }
        }

        override fun onScanFailed(errorCode: Int) {
            Log.e(TAG, "Scan failed with error: $errorCode")
            _connectionStatus.value = "Scan failed: $errorCode"
            _scanningState.value = false
        }
    }

    fun connectToDevice(bluetoothDevice: BluetoothDevice) {
        bluetoothDevice.device?.let { device ->
            connectToDevice(device)
        }
    }

    private fun connectToDevice(device: android.bluetooth.BluetoothDevice) {
        try {
            if (!hasBluetoothPermissions()) {
                return
            }
            
            bluetoothGatt?.disconnect()
            
            _connectionStatus.value = "Connecting..."
            bluetoothGatt = device.connectGatt(this, false, gattCallback)
            connectedDevice = device
            
            _connectedDeviceInfo.value = ConnectedDeviceInfo(
                name = device.name ?: "Unknown Device",
                address = device.address,
                connectionTime = System.currentTimeMillis(),
                status = "Connecting...",
                notificationsSent = 0
            )
            
        } catch (e: Exception) {
            Log.e(TAG, "Error connecting to device", e)
            _connectionStatus.value = "Connection failed"
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            try {
                when (newState) {
                    BluetoothProfile.STATE_CONNECTED -> {
                        _connectionStatus.value = "Connected"
                        lastConnectionTime = System.currentTimeMillis()
                        
                        connectedDevice?.let { device ->
                            _connectedDeviceInfo.value = _connectedDeviceInfo.value?.copy(
                                status = "Connected",
                                connectionTime = lastConnectionTime
                            )
                        }
                        
                        if (hasBluetoothPermissions()) {
                            // Request larger MTU for better performance
                            gatt.requestMtu(517) // Max MTU size
                        }
                        
                        processNotificationQueue()
                    }
                    BluetoothProfile.STATE_DISCONNECTED -> {
                        _connectionStatus.value = "Disconnected"
                        _connectedDeviceInfo.value = _connectedDeviceInfo.value?.copy(
                            status = "Disconnected"
                        )
                        notificationCharacteristic = null
                        currentMtu = 23 // Reset to default
                    }
                    BluetoothProfile.STATE_CONNECTING -> {
                        _connectionStatus.value = "Connecting..."
                        _connectedDeviceInfo.value = _connectedDeviceInfo.value?.copy(
                            status = "Connecting..."
                        )
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error in connection state change", e)
            }
        }

        override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                currentMtu = mtu
                Log.d(TAG, "MTU changed to: $mtu")
                
                // Now discover services after MTU is set
                if (hasBluetoothPermissions()) {
                    gatt.discoverServices()
                }
            } else {
                Log.w(TAG, "MTU change failed, using default MTU")
                currentMtu = 23
                // Still try to discover services
                if (hasBluetoothPermissions()) {
                    gatt.discoverServices()
                }
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            try {
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    Log.d(TAG, "Services discovered, looking for notification service...")
                    
                    gatt.services.forEach { service ->
                        Log.d(TAG, "Found service: ${service.uuid}")
                        service.characteristics.forEach { char ->
                            Log.d(TAG, "  - Characteristic: ${char.uuid}")
                        }
                    }
                    
                    val service = gatt.getService(SERVICE_UUID)
                    if (service != null) {
                        Log.d(TAG, "Found notification service!")
                        notificationCharacteristic = service.getCharacteristic(CHARACTERISTIC_UUID)
                        if (notificationCharacteristic != null) {
                            _connectionStatus.value = "Ready"
                            _connectedDeviceInfo.value = _connectedDeviceInfo.value?.copy(
                                status = "Ready"
                            )
                            Log.d(TAG, "Notification characteristic found and ready! MTU: $currentMtu")
                            processNotificationQueue()
                        } else {
                            Log.e(TAG, "Notification characteristic not found!")
                            _connectionStatus.value = "Characteristic not found"
                            _connectedDeviceInfo.value = _connectedDeviceInfo.value?.copy(
                                status = "Characteristic not found"
                            )
                        }
                    } else {
                        Log.e(TAG, "Notification service not found!")
                        _connectionStatus.value = "Service not found"
                        _connectedDeviceInfo.value = _connectedDeviceInfo.value?.copy(
                            status = "Service not found"
                        )
                    }
                } else {
                    Log.e(TAG, "Service discovery failed with status: $status")
                    _connectionStatus.value = "Service discovery failed"
                    _connectedDeviceInfo.value = _connectedDeviceInfo.value?.copy(
                        status = "Service discovery failed"
                    )
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error in service discovery", e)
            }
        }

        override fun onCharacteristicWrite(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int
        ) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                totalNotificationsSent++
                _connectedDeviceInfo.value = _connectedDeviceInfo.value?.copy(
                    notificationsSent = totalNotificationsSent
                )
                Log.d(TAG, "Notification sent successfully. Total sent: $totalNotificationsSent")
            } else {
                Log.e(TAG, "Failed to send notification: $status")
            }
        }
    }

    private fun sendNotificationToESP32(notificationData: NotificationData, isExisting: Boolean = false) {
        try {
            if (!hasBluetoothPermissions()) {
                return
            }
            
            if (notificationCharacteristic != null && _connectionStatus.value == "Ready") {
                // Truncate long text to fit in MTU
                val maxDataSize = currentMtu - 3 - 5 // MTU minus ATT overhead minus our header
                val truncatedData = truncateNotificationData(notificationData, maxDataSize)
                
                val packet = createNotificationPacket(truncatedData)
                
                if (packet.size <= maxDataSize) {
                    notificationCharacteristic?.value = packet
                    bluetoothGatt?.writeCharacteristic(notificationCharacteristic)
                    
                    if (!isExisting) {
                        val currentList = _notifications.value.toMutableList()
                        currentList.add(0, notificationData)
                        if (currentList.size > 50) {
                            currentList.removeAt(currentList.size - 1)
                        }
                        _notifications.value = currentList
                    }
                    
                    Log.d(TAG, "${if (isExisting) "Existing" else "New"} notification sent: ${truncatedData.appName} - ${truncatedData.title} (${packet.size} bytes)")
                } else {
                    Log.w(TAG, "Packet too large (${packet.size} bytes), skipping notification")
                }
            } else {
                notificationQueue.add(notificationData)
                Log.d(TAG, "Notification queued: ${notificationData.appName} - ${notificationData.title}")
                
                connectedDevice?.let { device ->
                    if (_connectionStatus.value == "Disconnected") {
                        Log.d(TAG, "Attempting reconnection for queued notification")
                        connectToDevice(device)
                    }
                }
                
                if (!isExisting) {
                    NotificationWorker.enqueueWork(
                        context = this,
                        notificationData = notificationData,
                        delay = 5000
                    )
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error sending notification", e)
        }
    }

    private fun truncateNotificationData(data: NotificationData, maxSize: Int): NotificationData {
        val appNameBytes = data.appName.toByteArray(Charsets.UTF_8)
        val titleBytes = data.title.toByteArray(Charsets.UTF_8)
        val textBytes = data.text.toByteArray(Charsets.UTF_8)
        
        var appLen = minOf(appNameBytes.size, 20) // Max 20 chars for app name
        var titleLen = minOf(titleBytes.size, 40) // Max 40 chars for title
        var textLen = minOf(textBytes.size, maxSize - 5 - appLen - titleLen) // Remaining for text
        
        // Ensure we don't go negative
        if (textLen < 0) {
            titleLen = minOf(titleLen, maxSize - 5 - appLen - 10) // Leave at least 10 for text
            textLen = maxSize - 5 - appLen - titleLen
        }
        
        return NotificationData(
            appName = String(appNameBytes, 0, appLen, Charsets.UTF_8),
            title = String(titleBytes, 0, titleLen, Charsets.UTF_8),
            text = String(textBytes, 0, maxOf(0, textLen), Charsets.UTF_8),
            timestamp = data.timestamp,
            isPriority = data.isPriority,
            packageName = data.packageName
        )
    }

    private fun createNotificationPacket(notificationData: NotificationData): ByteArray {
        val appNameBytes = notificationData.appName.toByteArray(Charsets.UTF_8)
        val titleBytes = notificationData.title.toByteArray(Charsets.UTF_8)
        val textBytes = notificationData.text.toByteArray(Charsets.UTF_8)
        
        val appLen = appNameBytes.size
        val titleLen = titleBytes.size
        val textLen = textBytes.size
        
        val totalLength = 5 + appLen + titleLen + textLen
        val packet = ByteArray(totalLength)
        
        var offset = 0
        packet[offset++] = CMD_ADD_NOTIFICATION
        packet[offset++] = getNotificationType(notificationData.packageName).toByte()
        packet[offset++] = appLen.toByte()
        packet[offset++] = titleLen.toByte()
        packet[offset++] = textLen.toByte()
        
        System.arraycopy(appNameBytes, 0, packet, offset, appLen)
        offset += appLen
        System.arraycopy(titleBytes, 0, packet, offset, titleLen)
        offset += titleLen
        System.arraycopy(textBytes, 0, packet, offset, textLen)
        
        Log.d(TAG, "Created packet: CMD=${CMD_ADD_NOTIFICATION}, type=${getNotificationType(notificationData.packageName)}, lengths=[$appLen,$titleLen,$textLen], total=$totalLength bytes, MTU=$currentMtu")
        
        return packet
    }

    private fun getNotificationType(packageName: String): Int {
        return when {
            packageName.contains("phone", true) || packageName.contains("dialer", true) -> 0
            packageName.contains("mms", true) || packageName.contains("message", true) || 
            packageName.contains("sms", true) || packageName.contains("whatsapp", true) ||
            packageName.contains("telegram", true) -> 1
            packageName.contains("gmail", true) || packageName.contains("mail", true) ||
            packageName.contains("email", true) -> 2
            packageName.contains("facebook", true) || packageName.contains("instagram", true) ||
            packageName.contains("twitter", true) || packageName.contains("snapchat", true) ||
            packageName.contains("tiktok", true) -> 3
            packageName.contains("calendar", true) -> 4
            else -> 5
        }
    }

    private fun processNotificationQueue() {
        if (notificationCharacteristic != null && notificationQueue.isNotEmpty()) {
            Log.d(TAG, "Processing ${notificationQueue.size} queued notifications")
            val queuedNotifications = notificationQueue.toList()
            notificationQueue.clear()
            
            queuedNotifications.forEach { notification ->
                sendNotificationToESP32(notification)
                Thread.sleep(200) // Longer delay for reliability
            }
        }
    }

    private fun checkConnectionAndSync() {
        when (_connectionStatus.value) {
            "Disconnected" -> {
                connectedDevice?.let { device ->
                    Log.d(TAG, "Sync check: attempting reconnection")
                    connectToDevice(device)
                }
            }
            "Ready" -> {
                Log.d(TAG, "Sync check: processing queue")
                processNotificationQueue()
            }
        }
    }

    private fun hasBluetoothPermissions(): Boolean {
        return try {
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.S) {
                ActivityCompat.checkSelfPermission(this, android.Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED &&
                ActivityCompat.checkSelfPermission(this, android.Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED
            } else {
                ActivityCompat.checkSelfPermission(this, android.Manifest.permission.BLUETOOTH) == PackageManager.PERMISSION_GRANTED &&
                ActivityCompat.checkSelfPermission(this, android.Manifest.permission.BLUETOOTH_ADMIN) == PackageManager.PERMISSION_GRANTED
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error checking permissions", e)
            false
        }
    }

    fun clearAllNotifications() {
        try {
            _notifications.value = emptyList()
            notificationQueue.clear()
            totalNotificationsSent = 0
            _connectedDeviceInfo.value = _connectedDeviceInfo.value?.copy(
                notificationsSent = 0
            )
            
            if (notificationCharacteristic != null && _connectionStatus.value == "Ready") {
                val packet = ByteArray(5) { 0 }
                packet[0] = CMD_CLEAR_ALL
                notificationCharacteristic?.value = packet
                bluetoothGatt?.writeCharacteristic(notificationCharacteristic)
            }
            
            Log.d(TAG, "All notifications cleared")
        } catch (e: Exception) {
            Log.e(TAG, "Error clearing notifications", e)
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        try {
            stopScanning()
            bluetoothGatt?.close()
            Log.d(TAG, "BLE Service destroyed")
        } catch (e: Exception) {
            Log.e(TAG, "Error destroying service", e)
        }
    }
}

data class BluetoothDevice(
    val name: String?,
    val address: String,
    val rssi: Int? = null,
    val device: android.bluetooth.BluetoothDevice? = null
)

data class ConnectedDeviceInfo(
    val name: String,
    val address: String,
    val connectionTime: Long,
    val status: String,
    val notificationsSent: Int
)

data class SyncStatus(
    val isInProgress: Boolean,
    val processedCount: Int,
    val sentCount: Int,
    val startTime: Long,
    val endTime: Long? = null
)
