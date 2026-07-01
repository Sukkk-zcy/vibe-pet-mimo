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

static void IRAM_ATTR btnLeftISR() {
    btnLeftPressed = true;
}

static void IRAM_ATTR btnRightISR() {
    btnRightPressed = true;
}

static void IRAM_ATTR btnOkISR() {
    btnOkPressed = true;
}

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

    if (btnLeftPressed) {
        btnLeftPressed = false;
        unsigned long now = millis();
        if (now - btnLeftTime > 300) {
            currentPage--;
            if (currentPage < 0) currentPage = 1;
            displayShowPage(currentPage);
        }
        btnLeftTime = now;
    }

    if (btnRightPressed) {
        btnRightPressed = false;
        unsigned long now = millis();
        if (now - btnRightTime > 300) {
            currentPage++;
            if (currentPage > 1) currentPage = 0;
            displayShowPage(currentPage);
        }
        btnRightTime = now;
    }

    if (btnOkPressed) {
        btnOkPressed = false;
        unsigned long now = millis();
        if (now - btnOkTime > 300) {
            currentPage = 0;
            displayShowPage(currentPage);
        }
        btnOkTime = now;
    }
}

static void checkLongPress() {
    static unsigned long leftPressStart = 0;
    static unsigned long rightPressStart = 0;
    static bool leftLongTriggered = false;
    static bool rightLongTriggered = false;

    bool leftDown = digitalRead(BUTTON_LEFT_PIN) == LOW;
    bool rightDown = digitalRead(BUTTON_RIGHT_PIN) == LOW;
    unsigned long now = millis();

    if (leftDown && rightDown) {
        if (!leftLongTriggered && !rightLongTriggered) {
            if (leftPressStart == 0) {
                leftPressStart = now;
            } else if (now - leftPressStart > 2000) {
                leftLongTriggered = true;
                rightLongTriggered = true;
                configMode = true;
                networkStartConfigPortal();
                displayShowConfigPortal("192.168.4.1");
            }
        }
    } else {
        leftPressStart = 0;
        leftLongTriggered = false;
        rightLongTriggered = false;
    }
}

void networkTask(void* parameter) {
    for (;;) {
        if (!configMode) {
            networkPoll();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void displayTask(void* parameter) {
    for (;;) {
        if (configMode) {
            networkHandleClient();
        } else {
            displayRender();
        }
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
        Serial.println("WiFi connected, starting normal operation");
        xTaskCreatePinnedToCore(networkTask, "NetworkTask", 4096, NULL, 1, NULL, 0);
        xTaskCreatePinnedToCore(displayTask, "DisplayTask", 4096, NULL, 1, NULL, 1);
    } else {
        Serial.println("WiFi not connected, entering config mode");
        configMode = true;
        networkStartConfigPortal();
        displayShowConfigPortal("192.168.4.1");
        xTaskCreatePinnedToCore(displayTask, "DisplayTask", 4096, NULL, 1, NULL, 1);
    }
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
