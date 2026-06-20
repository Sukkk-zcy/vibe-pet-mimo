# Firmware Targets

Each supported hardware target has a direct PlatformIO project in this folder.
The per-board display projects reuse `esp-display-code-pet/src/main.cpp` and keep
board-specific pins, libraries, device names, and transport settings in their own
`platformio.ini`.

For desktop-app flashing, place the ready-to-flash image at `main.bin` inside the
matching hardware folder. ESP targets use the bundled JavaScript esptool path and
write `main.bin` at flash address `0x0`, so `main.bin` should be a merged image
when the board needs bootloader, partition, and app segments.

| Directory | Hardware | Transport |
| --- | --- | --- |
| `wio-terminal-code-pet` | Wio Terminal | BLE |
| `esp-ai-mini-ext-tft-code-pet` | ESP-AI Mini Ext TFT | BLE |
| `esp-ai-mini-ext-status` | ESP-AI Mini Ext Status LED | BLE |
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

Example contributor build/upload commands:

```bash
pio run -d src/firmware/esp-ai-mini-ext-tft-code-pet -t upload
pio run -d src/firmware/m5stack-core2-code-pet -t upload
```

ESP-AI Mini Ext TFT follows the official 2.4-inch profile: `ST7789_2_DRIVER`,
TFT setup `WIDTH=240`, `HEIGHT=320`, logical screen `320x240`, SPI pins
`MOSI=42`, `SCLK=39`, `CS=13`, `DC=7`, and low-active backlight `BL=3`.
