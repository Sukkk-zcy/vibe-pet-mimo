# Firmware Targets

Each supported hardware target has a direct PlatformIO project in this folder.
The per-board display projects reuse `esp-display-code-pet/src/main.cpp` and keep
board-specific pins, libraries, device names, and transport settings in their own
`platformio.ini`.

| Directory | Hardware | Transport |
| --- | --- | --- |
| `wio-terminal-code-pet` | Wio Terminal | BLE |
| `esp-ai-mini-ext-status` | ESP-AI Mini Ext | BLE |
| `esp-ai-common-3-tft-code-pet` | ESP-AI Common 3.0.0 TFT | BLE |
| `esp-ai-diy-esp32s3-oled-code-pet` | ESP-AI DIY ESP32S3 OLED | BLE |
| `m5stack-core2-code-pet` | M5Stack Core2 | BLE |
| `m5stack-cores3-code-pet` | M5Stack CoreS3 | BLE |
| `m5stickc-plus2-code-pet` | M5StickC Plus2 | BLE |
| `m5stack-cardputer-code-pet` | M5Stack Cardputer | BLE |
| `m5stack-atoms3-code-pet` | M5Stack AtomS3 | BLE |
| `lilygo-t-display-code-pet` | LILYGO T-Display ESP32 | BLE |
| `lilygo-t-display-s3-code-pet` | LILYGO T-Display S3 | BLE |
| `heltec-wifi-kit-32-code-pet` | Heltec WiFi Kit 32 | BLE |
| `heltec-wifi-kit-8-code-pet` | Heltec WiFi Kit 8 | Wi-Fi polling |
| `wemos-d1-mini-oled-code-pet` | WEMOS D1 mini + OLED Shield | Wi-Fi polling |

Example:

```bash
pio run -d src/firmware/m5stack-core2-code-pet -t upload
```
