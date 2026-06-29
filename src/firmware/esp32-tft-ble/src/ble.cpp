#include "ble.h"
#include "config.h"
#include "state.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <ArduinoJson.h>

static BLEServer* pServer = nullptr;
static BLECharacteristic* pStateChar = nullptr;
static bool bleDeviceConnected = false;
static bool bleInitialized = false;

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        bleDeviceConnected = true;
        isConnected = true;
        Serial.println("BLE connected");
    }

    void onDisconnect(BLEServer* pServer) {
        bleDeviceConnected = false;
        isConnected = false;
        Serial.println("BLE disconnected");
        pServer->startAdvertising();
        Serial.println("BLE advertising again...");
    }
};

class StateCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            String json = String(value.c_str());
            Serial.print("BLE received (");
            Serial.print(value.length());
            Serial.print(" bytes): ");
            Serial.println(json);
            if (stateParse(json.c_str())) {
                Serial.print("State OK: ");
                Serial.println(pet.state);
            } else {
                Serial.println("Parse FAILED");
            }
        }
    }
};

static ServerCallbacks serverCallbacks;
static StateCallbacks stateCallbacks;

void bleInit() {
    if (bleInitialized) return;

    Serial.println("BLE: Init device...");
    BLEDevice::init(BLE_DEVICE_NAME);
    BLEDevice::setMTU(517);
    delay(100);

    Serial.println("BLE: Create server...");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);

    Serial.println("BLE: Create service...");
    BLEService* pService = pServer->createService(BLE_SERVICE_UUID);

    Serial.println("BLE: Create characteristic...");
    pStateChar = pService->createCharacteristic(
        BLE_STATE_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_WRITE_NR
    );
    pStateChar->setCallbacks(&stateCallbacks);

    Serial.println("BLE: Start service...");
    pService->start();

    Serial.println("BLE: Start advertising...");
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    bleInitialized = true;
    Serial.println("BLE: Initialized and advertising!");
    Serial.print("BLE: Device name: ");
    Serial.println(BLE_DEVICE_NAME);
}
