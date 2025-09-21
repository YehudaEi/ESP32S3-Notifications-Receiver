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
    
    // ESP32S3 Service and Characteristic UUIDs (customize these to match your ESP32S3)
    private val SERVICE_UUID = UUID.fromString("12345678-1234-1234-1234-123456789abc")
    private val CHARACTERISTIC_UUID = UUID.fromString("87654321-4321-4321-4321-cba987654321")
    
    private val _connectionStatus = MutableStateFlow("Disconnected")
    val connectionStatus: StateFlow<String> = _connectionStatus
    
    private val _notifications = MutableStateFlow<List<NotificationData>>(emptyList())
    val notifications: StateFlow<List<NotificationData>> = _notifications

    inner class LocalBinder : Binder() {
        fun getService(): BLEService = this@BLEService
    }

    override fun onBind(intent: Intent): IBinder {
        return binder
    }

    override fun onCreate() {
        super.onCreate()
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter
        bluetoothLeScanner = bluetoothAdapter?.bluetoothLeScanner
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            "SEND_NOTIFICATION" -> {
                val notificationData = intent.getParcelableExtra<NotificationData>("notification_data")
                notificationData?.let { sendNotificationToESP32(it) }
            }
        }
        return START_STICKY
    }

    fun startScanning() {
        if (ActivityCompat.checkSelfPermission(
                this,
                android.Manifest.permission.BLUETOOTH_SCAN
            ) != PackageManager.PERMISSION_GRANTED
        ) {
            _connectionStatus.value = "Permission required"
            return
        }
        
        _connectionStatus.value = "Scanning..."
        bluetoothLeScanner?.startScan(scanCallback)
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val device = result.device
            if (ActivityCompat.checkSelfPermission(
                    this@BLEService,
                    android.Manifest.permission.BLUETOOTH_CONNECT
                ) != PackageManager.PERMISSION_GRANTED
            ) {
                return
            }
            
            // Check if this is your ESP32S3 (you might want to filter by name or MAC address)
            if (device.name?.contains("ESP32", ignoreCase = true) == true ||
                device.name?.contains("ESP32S3", ignoreCase = true) == true) {
                bluetoothLeScanner?.stopScan(this)
                connectToDevice(device)
            }
        }

        override fun onScanFailed(errorCode: Int) {
            _connectionStatus.value = "Scan failed: $errorCode"
        }
    }

    private fun connectToDevice(device: BluetoothDevice) {
        if (ActivityCompat.checkSelfPermission(
                this,
                android.Manifest.permission.BLUETOOTH_CONNECT
            ) != PackageManager.PERMISSION_GRANTED
        ) {
            return
        }
        
        _connectionStatus.value = "Connecting..."
        bluetoothGatt = device.connectGatt(this, false, gattCallback)
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    _connectionStatus.value = "Connected"
                    if (ActivityCompat.checkSelfPermission(
                            this@BLEService,
                            android.Manifest.permission.BLUETOOTH_CONNECT
                        ) != PackageManager.PERMISSION_GRANTED
                    ) {
                        return
                    }
                    gatt.discoverServices()
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    _connectionStatus.value = "Disconnected"
                }
                BluetoothProfile.STATE_CONNECTING -> {
                    _connectionStatus.value = "Connecting..."
                }
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                val service = gatt.getService(SERVICE_UUID)
                notificationCharacteristic = service?.getCharacteristic(CHARACTERISTIC_UUID)
                if (notificationCharacteristic != null) {
                    _connectionStatus.value = "Ready"
                } else {
                    _connectionStatus.value = "Service not found"
                }
            } else {
                _connectionStatus.value = "Service discovery failed"
            }
        }

        override fun onCharacteristicWrite(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int
        ) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                // Notification sent successfully
            }
        }
    }

    private fun sendNotificationToESP32(notificationData: NotificationData) {
        if (ActivityCompat.checkSelfPermission(
                this,
                android.Manifest.permission.BLUETOOTH_CONNECT
            ) != PackageManager.PERMISSION_GRANTED
        ) {
            return
        }
        
        notificationCharacteristic?.let { characteristic ->
            val message = "${notificationData.appName}|${notificationData.title}|${notificationData.text}"
            characteristic.value = message.toByteArray()
            bluetoothGatt?.writeCharacteristic(characteristic)
            
            // Update local notifications list
            val currentList = _notifications.value.toMutableList()
            currentList.add(0, notificationData)
            if (currentList.size > 50) { // Keep only last 50 notifications
                currentList.removeAt(currentList.size - 1)
            }
            _notifications.value = currentList
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        bluetoothGatt?.close()
    }
}
