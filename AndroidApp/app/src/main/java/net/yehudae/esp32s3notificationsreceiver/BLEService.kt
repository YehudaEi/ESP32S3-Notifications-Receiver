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
    
    // ESP32S3 Service and Characteristic UUIDs (customize these to match your ESP32S3)
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

    private val notificationQueue = mutableListOf<NotificationData>()
    private var lastConnectionTime: Long = 0
    private var totalNotificationsSent: Int = 0

    companion object {
        private const val TAG = "BLEService"
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
            
            // Start periodic sync for background reliability
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
                    notificationData?.let { sendNotificationToESP32(it) }
                }
                "SYNC_CHECK" -> {
                    checkConnectionAndSync()
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error in onStartCommand", e)
        }
        return START_STICKY
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
            
            // Auto-stop scanning after 30 seconds
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
                    // Update RSSI if device already exists
                    val index = currentDevices.indexOf(existingDevice)
                    currentDevices[index] = bluetoothDevice
                    _discoveredDevices.value = currentDevices
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
            
            // Disconnect from current device if connected
            bluetoothGatt?.disconnect()
            
            _connectionStatus.value = "Connecting..."
            bluetoothGatt = device.connectGatt(this, false, gattCallback)
            connectedDevice = device
            
            // Update connected device info
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

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            try {
                val device = result.device
                
                if (!hasBluetoothPermissions()) {
                    return
                }
                
                // Check if this is your ESP32S3 (you might want to filter by name or MAC address)
                val deviceName = device.name
                if (deviceName?.contains("ESP32", ignoreCase = true) == true ||
                    deviceName?.contains("ESP32S3", ignoreCase = true) == true) {
                    bluetoothLeScanner?.stopScan(this)
                    connectToDevice(device)
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error in scan result", e)
            }
        }

        override fun onScanFailed(errorCode: Int) {
            Log.e(TAG, "Scan failed with error: $errorCode")
            _connectionStatus.value = "Scan failed: $errorCode"
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            try {
                when (newState) {
                    BluetoothProfile.STATE_CONNECTED -> {
                        _connectionStatus.value = "Connected"
                        lastConnectionTime = System.currentTimeMillis()
                        
                        // Update connected device info
                        connectedDevice?.let { device ->
                            _connectedDeviceInfo.value = _connectedDeviceInfo.value?.copy(
                                status = "Connected",
                                connectionTime = lastConnectionTime
                            )
                        }
                        
                        if (hasBluetoothPermissions()) {
                            gatt.discoverServices()
                        }
                        
                        // Process queued notifications
                        processNotificationQueue()
                    }
                    BluetoothProfile.STATE_DISCONNECTED -> {
                        _connectionStatus.value = "Disconnected"
                        _connectedDeviceInfo.value = _connectedDeviceInfo.value?.copy(
                            status = "Disconnected"
                        )
                        notificationCharacteristic = null
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

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            try {
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    val service = gatt.getService(SERVICE_UUID)
                    notificationCharacteristic = service?.getCharacteristic(CHARACTERISTIC_UUID)
                    if (notificationCharacteristic != null) {
                        _connectionStatus.value = "Ready"
                        _connectedDeviceInfo.value = _connectedDeviceInfo.value?.copy(
                            status = "Ready"
                        )
                        
                        // Process any queued notifications
                        processNotificationQueue()
                    } else {
                        _connectionStatus.value = "Service not found"
                        _connectedDeviceInfo.value = _connectedDeviceInfo.value?.copy(
                            status = "Service not found"
                        )
                    }
                } else {
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

    private fun sendNotificationToESP32(notificationData: NotificationData) {
        try {
            if (!hasBluetoothPermissions()) {
                return
            }
            
            if (notificationCharacteristic != null && _connectionStatus.value == "Ready") {
                val message = "${notificationData.appName}|${notificationData.title}|${notificationData.text}"
                notificationCharacteristic?.value = message.toByteArray()
                bluetoothGatt?.writeCharacteristic(notificationCharacteristic)
                
                // Update local notifications list
                val currentList = _notifications.value.toMutableList()
                currentList.add(0, notificationData)
                if (currentList.size > 50) { // Keep only last 50 notifications
                    currentList.removeAt(currentList.size - 1)
                }
                _notifications.value = currentList
                
                Log.d(TAG, "Notification sent: ${notificationData.appName} - ${notificationData.title}")
            } else {
                // Queue notification if not connected (Garmin-like behavior)
                notificationQueue.add(notificationData)
                Log.d(TAG, "Notification queued: ${notificationData.appName} - ${notificationData.title}")
                
                // Try to reconnect if we have a known device
                connectedDevice?.let { device ->
                    if (_connectionStatus.value == "Disconnected") {
                        Log.d(TAG, "Attempting reconnection for queued notification")
                        connectToDevice(device)
                    }
                }
                
                // Also enqueue background work for reliability
                NotificationWorker.enqueueWork(
                    context = this,
                    notificationData = notificationData,
                    delay = 5000 // 5 second delay
                )
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error sending notification", e)
        }
    }

    private fun processNotificationQueue() {
        if (notificationCharacteristic != null && notificationQueue.isNotEmpty()) {
            Log.d(TAG, "Processing ${notificationQueue.size} queued notifications")
            val queuedNotifications = notificationQueue.toList()
            notificationQueue.clear()
            
            queuedNotifications.forEach { notification ->
                sendNotificationToESP32(notification)
                Thread.sleep(100) // Small delay between notifications
            }
        }
    }

    private fun checkConnectionAndSync() {
        when (_connectionStatus.value) {
            "Disconnected" -> {
                // Try to reconnect to last known device
                connectedDevice?.let { device ->
                    Log.d(TAG, "Sync check: attempting reconnection")
                    connectToDevice(device)
                }
            }
            "Ready" -> {
                // Connection is good, process any queued notifications
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
