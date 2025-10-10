# ESP32S3 Notifications Receiver (YNotificator)

<div align="center">

![Project Status](https://img.shields.io/badge/status-active-success.svg)
![Zephyr Version](https://img.shields.io/badge/Zephyr-4.2.1-blue.svg)
![LVGL Version](https://img.shields.io/badge/LVGL-9.x-purple.svg)
![License](https://img.shields.io/badge/license-AGPL%203.0-blue.svg)

**A Zephyr RTOS-based notification receiver that displays Android notifications on an ESP32-S3 with round LCD display via Bluetooth Low Energy.**

[Features](#features) • [Hardware](#hardware-requirements) • [Setup](#getting-started) • [Build](#building-the-project) • [Usage](#usage) • [Contributing](#contributing)

</div>

---

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Software Requirements](#software-requirements)
- [Getting Started](#getting-started)
- [Building the Project](#building-the-project)
- [Flashing the Firmware](#flashing-the-firmware)
- [Installing the Android App](#installing-the-android-app)
- [Usage](#usage)
- [Project Structure](#project-structure)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)

---

## 🎯 Overview

**YNotificator** is a complete notification receiver system consisting of:

1. **ESP32-S3 Firmware** - Real-time notification display on a round LCD
2. **Android Application** - Captures and forwards Android notifications via BLE

The system provides a smartwatch-like experience for viewing Android notifications on a dedicated ESP32-S3 device with a beautiful circular display.

### Architecture

```
┌─────────────────┐         BLE          ┌──────────────────┐
│                 │◄────────────────────►│                  │
│  Android Phone  │   Notifications      │   ESP32-S3 +     │
│                 │                      │   Round LCD      │
└─────────────────┘                      └──────────────────┘
```

---

## ✨ Features

### ESP32-S3 Firmware
- ✅ **Real-time BLE Communication** - Low latency notification reception
- ✅ **LVGL GUI** - Beautiful circular UI optimized for round displays
- ✅ **Multi-language Support** - Hebrew, Arabic (RTL), English, and more
- ✅ **Gesture Control** - Swipe navigation between notifications
- ✅ **Secure Pairing** - Encrypted BLE connection with passkey display
- ✅ **Watchdog Timer** - System stability and auto-recovery
- ✅ **Power Management** - PWM-controlled backlight
- ✅ **Touch Interface** - Capacitive touch with CST816S controller

### Android Application
- ✅ **Notification Listener Service** - Captures all Android notifications
- ✅ **Background Sync** - WorkManager ensures reliable delivery
- ✅ **App Filtering** - Choose which apps can send notifications
- ✅ **Priority Notifications** - Mark important apps for instant delivery
- ✅ **Quiet Hours** - Schedule do-not-disturb periods
- ✅ **Statistics Dashboard** - Track notification metrics
- ✅ **Device Discovery** - Easy BLE pairing and connection
- ✅ **Existing Notifications Sync** - Read and send current Android notifications

---

## 🔧 Hardware Requirements

### Required Hardware
- **Waveshare ESP32-S3-Touch-LCD-1.28** development board
  - [Product Page](https://www.waveshare.com/esp32-s3-touch-lcd-1.28.htm)
  - [Zephyr Board Documentation](https://docs.zephyrproject.org/latest/boards/waveshare/esp32s3_touch_lcd_1_28/doc/index.html)

### Board Specifications

#### Microcontroller
- **SoC**: ESP32-S3-WROOM-1 Module
- **CPU**: Dual-core Xtensa® 32-bit LX7 microprocessor
- **Clock Speed**: Up to 240 MHz
- **RAM**: 2 MB SRAM
- **Flash**: 16 MB
- **AI Acceleration**: Additional vector instructions support

#### Connectivity
- **Wi-Fi**: 802.11 b/g/n (2.4 GHz)
- **Bluetooth**: Bluetooth LE 5.0 with long-range support (up to 2 Mbps data rate)
- **USB**: USB-C connector for programming and power

#### Display & Input
- **LCD**: 1.28" Round IPS Display
  - Resolution: 240 × 240 pixels
  - Controller: GC9A01 (Galaxycore)
  - Interface: SPI
- **Touch**: Capacitive touchscreen
  - Controller: CST816S (Hynitron)
  - Interface: I2C

#### Sensors
- **IMU**: QMI8658C 6-axis accelerometer and gyroscope
  - 3-axis accelerometer
  - 3-axis gyroscope
  - Interface: I2C/SPI

#### Power
- **Battery Charger**: Onboard battery charging circuit
- **Power Supply**: 5V via USB-C
- **Power Modes**: 5 power management modes

#### Expansion
- **GPIOs**: 6 programmable GPIO pins
- **Interfaces**: I2C, SPI, UART available on headers

#### Security Features
- Secure boot
- Flash encryption
- Cryptographic hardware acceleration (AES-128/256, Hash, RSA, RNG, HMAC)
- 4-Kbit OTP memory

---

## 💻 Software Requirements

### For ESP32-S3 Firmware Development

- **Zephyr RTOS 4.1.0+**
- **West Build Tool**
- **Python 3.8+**
- **ESP-IDF tools** (for flashing)
- **Git**

### For Android App Development

- **Android Studio 2024.1+**
- **JDK 21**
- **Android SDK API 24+ (Android 7.0+)**
- **Gradle 8.13+**

---

## 🚀 Getting Started

### 1. Clone the Repository

```bash
git clone https://github.com/YehudaEi/ESP32S3-Notifications-Receiver.git
cd ESP32S3-Notifications-Receiver
```

### 2. Initialize Zephyr Workspace

```bash
# Initialize west workspace
west init -l .

# Update dependencies
west update

# Export Zephyr CMake package
west zephyr-export

# Install Python dependencies
pip3 install -r ~/zephyrproject/zephyr/scripts/requirements.txt
```

### 3. Fetch Espressif Binary Blobs

The Espressif HAL requires WiFi and Bluetooth binary blobs. Fetch them with:

```bash
# Fetch HAL binary blobs
west blobs fetch hal_espressif
```

**Note**: Run this command after every `west update`.

### 4. Install ESP-IDF Tools (for flashing)

```bash
# Install esptool for flashing
pip3 install esptool
```

---

## 🔨 Building the Project

### Build ESP32-S3 Firmware

```bash
# Clean build (recommended for first build)
west build -b esp32s3_touch_lcd_1_28/esp32s3/procpu -p always

# Incremental build (after making changes)
west build
```

The build output will be in `build/zephyr/`:
- `zephyr.elf` - ELF executable
- `zephyr.bin` - Binary firmware image

### Build Android Application

```bash
cd AndroidApp

# Debug build
./gradlew assembleDebug

# Release build (unsigned)
./gradlew assembleRelease
```

APK files will be in `AndroidApp/app/build/outputs/apk/`

---

## 📲 Flashing the Firmware

### Flash to ESP32-S3

```bash
# Flash firmware using west (recommended)
west flash

# Or specify the serial port manually
west flash --esp-device /dev/ttyUSB0

# Monitor serial output
west espressif monitor
```

### Manual Flashing with esptool

```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  --before default_reset --after hard_reset write_flash \
  -z --flash_mode dio --flash_freq 80m --flash_size detect \
  0x0 build/zephyr/zephyr.bin
```

**Note**: Replace `/dev/ttyUSB0` with your actual serial port:
- **Linux/macOS**: `/dev/ttyUSB0` or `/dev/ttyACM0`
- **Windows**: `COM3`, `COM4`, etc.

### First Time Flashing

If the board doesn't enter flash mode automatically:
1. Hold the **BOOT** button
2. Press the **RESET** button briefly
3. Release the **BOOT** button
4. Run the flash command

---

## 📱 Installing the Android App

### Method 1: Android Studio

1. Open `AndroidApp` folder in Android Studio
2. Connect your Android device via USB
3. Enable **USB Debugging** in Developer Options
4. Click **Run** button or press `Shift+F10`

### Method 2: Install APK Directly

```bash
# Install debug APK
adb install AndroidApp/app/build/outputs/apk/debug/app-debug.apk

# Or copy APK to device and install manually
```

### Required Permissions

The app will request:
- ✅ **Notification Access** - To read notifications
- ✅ **Bluetooth** - For BLE communication
- ✅ **Location** - Required for BLE scanning (Android requirement)

---

## 📖 Usage

### First Time Setup

#### 1. Enable Notification Access

1. Open **Android Settings**
2. Go to **Apps & Notifications** → **Special App Access**
3. Select **Notification Access**
4. Enable **ESP32S3 Notifications Receiver**

#### 2. Pair ESP32-S3 Device

1. Power on ESP32-S3 device
2. Open the Android app
3. Tap **Scan** or **Devices** button
4. Select **YNotificator** from the list
5. Enter the 6-digit pairing code shown on ESP32-S3 display
6. Tap **Accept** on both devices

#### 3. Sync Existing Notifications

1. Once connected, tap the **Sync** button
2. The app will read all current Android notifications
3. These will be sent to the ESP32-S3 device
4. Perfect for catching up on missed notifications!

### Using the ESP32-S3 Device

#### Gesture Controls

- **Swipe Left** → Next notification
- **Swipe Right** → Previous notification
- **Swipe Up** → Delete notification (with undo option)
- **Double Tap** → Mark as read
- **Tap during delete** → Undo deletion

#### Status Indicators

- **Green Circle** → Connected and ready
- **Blue Circle** → Connecting...
- **Yellow Circle** → Weak signal
- **Red Circle** → Disconnected

#### Display Information

The round LCD shows:
- **Top Bar**: Current time and connection status
- **App Icon**: Color-coded by application
- **App Name**: Source application
- **Sender**: Message sender or notification title
- **Content**: Notification message text
- **Timestamp**: When notification was received
- **Counter**: Current notification number / Total count

### Using the Android App

#### Main Screen
- Shows connection status
- Display recent notifications
- Quick sync button for existing notifications
- Access to devices and settings

#### Device Discovery
- Scan for nearby ESP32-S3 devices
- Shows signal strength (RSSI)
- One-tap connection

#### Settings & Statistics
- View notification statistics
- Manage which apps can send notifications
- Set priority apps (faster delivery)
- Configure quiet hours
- View sync status and history

#### Sync Management
The app features sophisticated sync capabilities:
- **Real-time Sync**: New notifications sent immediately
- **Existing Notifications**: Read all current Android notifications on demand
- **Background Sync**: WorkManager ensures delivery even when app is closed
- **Retry Logic**: Failed notifications automatically retry

---

## 📁 Project Structure

```
ESP32S3-Notifications-Receiver/
├── src/                        # ESP32-S3 firmware source
│   ├── main.c                  # Main application entry
│   ├── bluetooth/              # BLE communication
│   │   ├── bluetooth.c         # BLE service implementation
│   │   └── pairing_screen.c    # Pairing UI
│   ├── display/                # LCD display driver
│   ├── graphics/               # LVGL initialization
│   ├── notifications/          # Notification UI
│   ├── watchdog/               # Watchdog timer
│   └── fonts/                  # Custom fonts (including Hebrew)
├── boards/                     # Board-specific overlays
│   └── esp32.overlay           # ESP32-S3 device tree overlay
├── AndroidApp/                 # Android application
│   └── app/src/main/java/.../
│       ├── MainActivity.kt     # Main activity
│       ├── BLEService.kt       # BLE communication service
│       ├── NotificationListener.kt  # Notification capture
│       ├── NotificationWorker.kt    # Background sync
│       ├── DeviceDiscoveryScreen.kt
│       └── SettingsScreen.kt
├── CMakeLists.txt              # Zephyr build configuration
├── prj.conf                    # Zephyr project configuration
├── west.yml                    # West manifest
└── README.md                   # This file
```

---

## 🐛 Troubleshooting

### ESP32-S3 Issues

#### Build Fails
```bash
# Clean and rebuild
west build -t pristine
west build -b esp32s3_touch_lcd_1_28/esp32s3/procpu -p always
```

#### Binary Blobs Missing
```bash
# If you see errors about missing WiFi/BT blobs
west blobs fetch hal_espressif
```

#### Flash Fails
- Check USB cable connection (use data cable, not charging-only)
- Try different baud rate: `--baud 115200`
- Hold **BOOT** button while connecting USB
- Check serial port permissions: `sudo usermod -a -G dialout $USER` (logout/login required)

#### Display Not Working
- Verify display connections in device tree
- Check backlight PWM configuration
- Ensure `CONFIG_DISPLAY=y` in `prj.conf`
- Try adjusting brightness with `change_brightness()` function

#### Touch Not Responding
- Verify CST816S touchscreen is properly connected
- Check I2C configuration in device tree
- Enable touch input logging for debugging

#### BLE Not Advertising
- Check Bluetooth enabled: `CONFIG_BT=y`
- Verify device name: `CONFIG_BT_DEVICE_NAME="YNotificator"`
- Ensure binary blobs are fetched: `west blobs fetch hal_espressif`
- Check antenna connection on module

### Android App Issues

#### Notifications Not Appearing
1. Verify notification access is enabled
2. Check app is not battery optimized (Settings → Battery → Battery Optimization)
3. Ensure BLE permissions granted
4. Try reconnecting device

#### Connection Drops
- Keep Android device closer to ESP32-S3 (within 10 meters)
- Check for Bluetooth interference (other BLE devices, WiFi, microwave)
- Ensure ESP32-S3 firmware is running (check serial monitor)
- Try forgetting and re-pairing device

#### Sync Not Working
- Check notification listener service is running
- Verify app has all required permissions
- Clear app data and reconfigure
- Check Android version compatibility (API 24+ / Android 7.0+)

#### Pairing Issues
- Ensure both devices show the same 6-digit code
- Check that secure pairing is enabled in `prj.conf`
- Try clearing Bluetooth cache on Android
- Verify encryption is working (check logs)

---

## 🤝 Contributing

Contributions are welcome! Please follow these steps:

1. **Fork** the repository
2. **Create** a feature branch (`git checkout -b feature/amazing-feature`)
3. **Commit** your changes (`git commit -m 'Add amazing feature'`)
4. **Push** to the branch (`git push origin feature/amazing-feature`)
5. **Open** a Pull Request

### Development Guidelines

- Follow existing code style
- Add comments for complex logic
- Test on actual hardware before submitting
- Update documentation for new features
- Run CI checks locally before pushing

### Code Style

- **C Code**: Follow Zephyr coding style guidelines
- **Kotlin Code**: Follow official Kotlin style guide
- **Documentation**: Use clear, concise language

---

## 📄 License

This project is licensed under the **GNU Affero General Public License v3.0 (AGPL-3.0)**.

This means:
- ✅ You can use this software for any purpose
- ✅ You can modify the software
- ✅ You can distribute the software
- ✅ You can use it for commercial purposes
- ⚠️ You must disclose source code when distributing
- ⚠️ Network use is considered distribution (AGPL requirement)
- ⚠️ Modified versions must be released under the same license

See the [LICENSE](LICENSE) file for full details.

---

## 👤 Author

**Yehuda**  
📧 Email: Yehuda@YehudaE.net  
🌐 GitHub: [@YehudaEI](https://github.com/YehudaEI)  
🔗 Repository: [ESP32S3-Notifications-Receiver](https://github.com/YehudaEi/ESP32S3-Notifications-Receiver)

---

## 🙏 Acknowledgments

- **Zephyr Project** - Real-time operating system
- **LVGL** - Light and Versatile Graphics Library
- **Espressif** - ESP32-S3 SoC and development tools
- **Waveshare** - ESP32-S3-Touch-LCD-1.28 development board
- **Android Open Source Project** - Android SDK and tools

---

## 📊 Project Status

- ✅ Core functionality complete
- ✅ BLE communication working
- ✅ Android app feature-complete
- ✅ Multi-language support (English, Hebrew, Arabic)
- ✅ Background sync implemented
- ✅ Secure pairing with passkey
- ✅ Touch gesture controls
- 🔄 Battery optimization in progress
- 🔄 Additional notification actions planned
- 🔄 Accelerometer/gyroscope integration planned

---

## 📸 Screenshots

_(Add your screenshots here once available)_

---

## 🔗 Useful Links

- [Zephyr Documentation](https://docs.zephyrproject.org/)
- [Board Documentation](https://docs.zephyrproject.org/latest/boards/waveshare/esp32s3_touch_lcd_1_28/doc/index.html)
- [LVGL Documentation](https://docs.lvgl.io/)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- [Waveshare Product Page](https://www.waveshare.com/esp32-s3-touch-lcd-1.28.htm)

---

<div align="center">

**Made with ❤️ using Zephyr RTOS, LVGL, and Kotlin**

If you found this project helpful, please consider giving it a ⭐!

[Report Bug](https://github.com/YehudaEi/ESP32S3-Notifications-Receiver/issues) · [Request Feature](https://github.com/YehudaEi/ESP32S3-Notifications-Receiver/issues)

</div>
