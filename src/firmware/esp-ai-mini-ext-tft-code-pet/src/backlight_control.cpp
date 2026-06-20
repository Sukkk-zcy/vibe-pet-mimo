#include "backlight_control.h"

#if defined(ESP32) && defined(__has_include)
#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif
#endif

#ifndef ESP_ARDUINO_VERSION_VAL
#define ESP_ARDUINO_VERSION_VAL(major, minor, patch) (((major) << 16) | ((minor) << 8) | (patch))
#endif
#ifndef ESP_ARDUINO_VERSION
#define ESP_ARDUINO_VERSION ESP_ARDUINO_VERSION_VAL(2, 0, 0)
#endif

#define SCREEN_BL_PIN 3
#define SCREEN_TIMEOUT_MS (10UL * 60UL * 1000UL)
#define LEDC_FREQ 5000
#define LEDC_RES 8
#define LEDC_MAX ((1 << LEDC_RES) - 1)
#define BACKLIGHT_DEFAULT_BRIGHTNESS 255

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
#define LEDC_CH SCREEN_BL_PIN
#else
#define LEDC_CH 0
#endif

static unsigned long lastActivityTime = 0;
static bool backlightOn = false;
static uint8_t currentBrightness = 0;

static void writeBacklightDuty(uint8_t brightness) {
  uint32_t duty = LEDC_MAX - ((uint32_t)brightness * LEDC_MAX / 255);
  ledcWrite(LEDC_CH, duty);
}

void initBacklight() {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcAttach(SCREEN_BL_PIN, LEDC_FREQ, LEDC_RES);
#else
  ledcSetup(LEDC_CH, LEDC_FREQ, LEDC_RES);
  ledcAttachPin(SCREEN_BL_PIN, LEDC_CH);
#endif
  writeBacklightDuty(0);
  backlightOn = false;
  currentBrightness = 0;
}

void setBacklightBrightness(uint8_t brightness) {
  currentBrightness = brightness;
  writeBacklightDuty(brightness);
  backlightOn = brightness > 0;
  if (backlightOn) {
    lastActivityTime = millis();
  }
}

void setBacklightOn() {
  setBacklightBrightness(BACKLIGHT_DEFAULT_BRIGHTNESS);
}

void setBacklightOff() {
  writeBacklightDuty(0);
  backlightOn = false;
  currentBrightness = 0;
}

void resetBacklightTimer() {
  lastActivityTime = millis();
  if (!backlightOn) {
    setBacklightOn();
  }
}

void updateBacklight() {
  if (backlightOn && (millis() - lastActivityTime >= SCREEN_TIMEOUT_MS)) {
    setBacklightOff();
  }
}
