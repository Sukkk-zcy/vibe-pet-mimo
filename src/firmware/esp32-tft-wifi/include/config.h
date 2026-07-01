#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#define DEVICE_NAME "VibePet-WiFi"

#define BUTTON_LEFT_PIN  25
#define BUTTON_RIGHT_PIN 27
#define BUTTON_OK_PIN    26
#define BUZZER_PIN       13
#define BATTERY_ADC_PIN  34

#define ADC_ATTEN        ADC_2_5db
#define ADC_WIDTH        ADC_WIDTH_BIT_12
#define ADC_SAMPLES      256
#define VOLTAGE_DIVIDER  4.0f
#define BATTERY_MIN_V    2.9f
#define BATTERY_MAX_V    4.2f

#define WIFI_AP_SSID     "VibePet-Setup"
#define WIFI_AP_PASS     "12345678"
#define WIFI_AP_CHANNEL  1
#define WIFI_AP_HIDDEN   false
#define WIFI_AP_MAX_CONN 1

#define DEFAULT_BRIDGE_HOST "192.168.1.2"
#define DEFAULT_BRIDGE_PORT 17384
#define POLL_INTERVAL_MS    500
#define RECONNECT_INTERVAL_MS 30000

#define NTP_SERVER      "ntp1.aliyun.com"
#define GMT_OFFSET_SEC  (8 * 3600)
#define DAYLIGHT_OFFSET 0

#define SERIAL_BAUD 115200

#define TFT_BACKLIGHT_PIN 17
#define TFT_BACKLIGHT_ON HIGH

#define PREF_NAMESPACE    "vibepet"
#define PREF_KEY_SSID     "ssid"
#define PREF_KEY_PASS     "pass"
#define PREF_KEY_HOST     "host"
#define PREF_KEY_PORT     "port"
#define PREF_KEY_BRIGHT   "bright"

#define BRIGHTNESS_DEFAULT 128
#define BRIGHTNESS_MIN     10
#define BRIGHTNESS_MAX     255

#define STATE_IDLE         "idle"
#define STATE_THINKING     "thinking"
#define STATE_WORKING      "working"
#define STATE_TYPING       "typing"
#define STATE_BUILDING     "building"
#define STATE_JUGGLING     "juggling"
#define STATE_ATTENTION    "attention"
#define STATE_NOTIFICATION "notification"
#define STATE_ERROR        "error"
#define STATE_SWEEPING     "sweeping"
#define STATE_SLEEPING     "sleeping"

#endif
