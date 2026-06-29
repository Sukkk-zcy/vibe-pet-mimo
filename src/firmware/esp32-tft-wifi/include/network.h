#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>

void networkInit();
bool networkIsConnected();
void networkPoll();
void networkStartConfigPortal();
void networkHandleClient();
void networkLoadConfig();
void networkSaveConfig(const char* ssid, const char* pass, const char* host, uint16_t port);
String networkGetSSID();
String networkGetHost();
uint16_t networkGetPort();

#endif
