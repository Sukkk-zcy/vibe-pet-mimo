#include <Arduino.h>
#include "config.h"
#include "state.h"
#include "display.h"
#include "ble.h"

static volatile bool btnLeftPressed = false;
static volatile bool btnRightPressed = false;
static volatile bool btnOkPressed = false;
static unsigned long btnLeftTime = 0;
static unsigned long btnRightTime = 0;
static unsigned long btnOkTime = 0;
static int currentPage = 0;

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
    if (btnLeftPressed) {
        btnLeftPressed = false;
        unsigned long now = millis();
        if (now - btnLeftTime > 150) {
            currentPage--;
            if (currentPage < 0) currentPage = 1;
            displayShowPage(currentPage);
        }
        btnLeftTime = now;
    }

    if (btnRightPressed) {
        btnRightPressed = false;
        unsigned long now = millis();
        if (now - btnRightTime > 150) {
            currentPage++;
            if (currentPage > 1) currentPage = 0;
            displayShowPage(currentPage);
        }
        btnRightTime = now;
    }

    if (btnOkPressed) {
        btnOkPressed = false;
        unsigned long now = millis();
        if (now - btnOkTime > 150) {
            currentPage = 0;
            displayShowPage(currentPage);
        }
        btnOkTime = now;
    }
}

static void handleSerialCommand() {
    if (!Serial.available()) return;
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();

    if (cmd.startsWith("test ")) {
        String state = cmd.substring(5);
        state.trim();
        String json = "{\"s\":\"" + state + "\",\"a\":\"MiMoCode\",\"m\":\"test\",\"ts\":" + String(millis()) + "}";
        Serial.print("Test state: ");
        Serial.println(state);
        stateParse(json.c_str());
    } else if (cmd == "help") {
        Serial.println("Commands:");
        Serial.println("  test <state>  - Test state (idle/thinking/working/error/sleeping/attention)");
        Serial.println("  help          - Show this help");
    }
}

void displayTask(void* parameter) {
    for (;;) {
        handleButtons();
        displayRender();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    Serial.println("VibePet BLE starting...");
    Serial.println("Type 'test working' or 'help' for commands");

    stateInit();
    initButtons();
    displayInit();
    displayBrightness(currentBrightness);

    displayShowConnecting();

    Serial.println("Initializing BLE...");
    bleInit();
    Serial.println("BLE ready, waiting for connection...");

    xTaskCreatePinnedToCore(displayTask, "DisplayTask", 4096, NULL, 1, NULL, 1);
}

void loop() {
    handleSerialCommand();
    vTaskDelay(pdMS_TO_TICKS(100));
}
