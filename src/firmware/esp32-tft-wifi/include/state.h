#ifndef STATE_H
#define STATE_H

#include <Arduino.h>

struct VibePetPacket {
    String state;
    String stateLabel;
    String agent;
    String event;
    String title;
    String output;
    String personaSlug;
    String personaName;
    String personaKind;
    int activeCount;
    unsigned long receivedAt;
};

extern VibePetPacket pet;
extern bool uiDirty;
extern bool isConnected;
extern int currentBrightness;

void stateInit();
void stateReset();
bool stateParse(const char* json);
const char* stateLabelForState(const String& state);
uint16_t stateColorRGB565(const String& state);

#endif
