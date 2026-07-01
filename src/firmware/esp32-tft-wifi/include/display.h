#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

void displayInit();
void displayBrightness(int brightness);
void displayRender();
void displayShowConnecting();
void displayShowConfigPortal(const char* ip);
void displayShowError(const char* msg);
void displayFillScreen(uint16_t color);
void displayShowPage(int page);
void displayShowIP(const char* ip);

#endif
