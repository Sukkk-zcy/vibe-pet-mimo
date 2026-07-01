#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "state.h"
#include "network.h"
#include "display.h"

static volatile bool btnLeftPressed = false;
static volatile bool btnRightPressed = false;
static volatile bool btnOkPressed = false;
static unsigned long btnLeftTime = 0;
static unsigned long btnRightTime = 0;
static unsigned long btnOkTime = 0;
static int currentPage = 0;
static bool configMode = false;

static void IRAM_ATTR btnLeftISR() { btnLeftPressed = true; }
static void IRAM_ATTR btnRightISR() { btnRightPressed = true; }
static void IRAM_ATTR btnOkISR() { btnOkPressed = true; }

static void initButtons() {
    pinMode(BUTTON_LEFT_PIN, INPUT_PULLUP);
    pinMode(BUTTON_RIGHT_PIN, INPUT_PULLUP);
    pinMode(BUTTON_OK_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_LEFT_PIN), btnLeftISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(BUTTON_RIGHT_PIN), btnRightISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(BUTTON_OK_PIN), btnOkISR, FALLING);
}

static void handleButtons() {
    if (configMode) return;
    unsigned long n = millis();
    if (btnLeftPressed) { btnLeftPressed = false; if (n - btnLeftTime > 300) { currentPage--; if (currentPage < 0) currentPage = 1; displayShowPage(currentPage); } btnLeftTime = n; }
    if (btnRightPressed) { btnRightPressed = false; if (n - btnRightTime > 300) { currentPage++; if (currentPage > 1) currentPage = 0; displayShowPage(currentPage); } btnRightTime = n; }
    if (btnOkPressed) { btnOkPressed = false; if (n - btnOkTime > 300) { currentPage = 0; displayShowPage(currentPage); } btnOkTime = n; }
}

static void checkLongPress() {
    static unsigned long lp = 0;
    static bool lt = false;
    bool ld = digitalRead(BUTTON_LEFT_PIN) == LOW;
    bool rd = digitalRead(BUTTON_RIGHT_PIN) == LOW;
    if (ld && rd) {
        if (!lt) { if (lp == 0) lp = millis(); else if (millis() - lp > 2000) { lt = true; configMode = true; networkStartConfigPortal(); displayShowConfigPortal("192.168.4.1"); } }
    } else { lp = 0; lt = false; }
}

// ─── 主任务：处理 HTTP + 显示 + 按键 ────────────────────
void appTask(void* parameter) {
    for (;;) {
        networkHandle();
        if (!configMode) displayRender();
        handleButtons();
        checkLongPress();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    Serial.println("VibePet-WiFi starting...");

    stateInit();
    initButtons();
    displayInit();
    displayBrightness(currentBrightness);

    displayShowConnecting();
    networkInit();

    if (WiFi.status() == WL_CONNECTED) {
        String ip = WiFi.localIP().toString();
        Serial.printf("Ready! POST to http://%s/api/state\n", ip.c_str());

        // 启动前显示 IP 5 秒
        displayShowIP(ip.c_str());
        delay(5000);
        xTaskCreatePinnedToCore(appTask, "AppTask", 4096, NULL, 1, NULL, 1);
    } else {
        configMode = true;
        networkStartConfigPortal();
        displayShowConfigPortal("192.168.4.1");
        xTaskCreatePinnedToCore(appTask, "AppTask", 4096, NULL, 1, NULL, 1);
    }
}

void loop() {
    static unsigned long lastHb = 0;
    unsigned long now = millis();
    if (now - lastHb > 60000) {
        lastHb = now;
        Serial.printf("[HB] WiFi:%s Agent:%s State:%s Output:%s\n",
            WiFi.status() == WL_CONNECTED ? "OK" : "NO",
            pet.agent.c_str(), pet.state.c_str(), pet.output.substring(0, 30).c_str());
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
}
