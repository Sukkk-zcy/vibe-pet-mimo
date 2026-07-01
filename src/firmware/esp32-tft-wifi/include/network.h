#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>

void networkInit();
void networkHandle();
void networkStartConfigPortal();
String networkGetSSID();

#endif
