#ifndef BACKLIGHT_CONTROL_H
#define BACKLIGHT_CONTROL_H

#include <Arduino.h>

void initBacklight();
void setBacklightBrightness(uint8_t brightness);
void setBacklightOn();
void setBacklightOff();
void resetBacklightTimer();
void updateBacklight();

#endif
