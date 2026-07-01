#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>

void networkInit();
void networkHandle();            // 每帧调用：处理 HTTP 请求 + 配置门户
void networkStartConfigPortal(); // 进入配置模式
String networkGetSSID();

#endif
