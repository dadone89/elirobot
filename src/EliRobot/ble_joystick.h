// ble_joystick.h
#ifndef BLE_JOYSTICK_H
#define BLE_JOYSTICK_H

#include <NimBLEDevice.h>
#include "config.h"

static NimBLEAddress pServerAddress("ff:25:12:02:12:80", 0);  // INDIRIZZO MAC DEL TUO JOYSTICK
NimBLEClient* pClient = nullptr;
bool bleConnected = false;

// Shared
volatile int bleCurrentCommand = BTN_NONE;
volatile unsigned long lastBlePacketTime = 0;

// Callback
void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {

  if (length > 0) {

    uint8_t cmd = pData[0];

    // IMPORTANT: Replace with controller codes
    switch (cmd) {
      case 0x02:
        bleCurrentCommand = BTN_U;
        break;
      case 0x01:
        bleCurrentCommand = BTN_D;
        break;
      case 0x20:
        bleCurrentCommand = BTN_L;
        break;
      case 0x10:
        bleCurrentCommand = BTN_R;
        break;
      // Release
      case 0x00:
        bleCurrentCommand = BTN_NONE;
        break;
      default:
        // Opzional: ignore other keys
        // bleCurrentCommand = BTN_NONE;
        break;
    }
  }
}

class ClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pClient) {
    Serial.println("-> BLE Joystick Connesso.");
    bleConnected = true;
  }
  void onDisconnect(NimBLEClient* pClient) {
    Serial.println("-> BLE Joystick Disconnesso.");
    bleConnected = false;
    bleCurrentCommand = BTN_NONE;
  }
};

bool connectToJoystick() {
  pClient = NimBLEDevice::getClientByPeerAddress(pServerAddress);

  if (!pClient) {
    pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(new ClientCallbacks(), true);
  }

  if (!pClient->isConnected()) {
    if (!pClient->connect(pServerAddress)) {
      // Not print error at every loop
      return false;
    }
  }

  Serial.println("BLE: Ricerca Servizi...");
  auto services = pClient->getServices(true);
  for (auto pSvc : services) {
    auto characteristics = pSvc->getCharacteristics(true);
    for (auto pChar : characteristics) {
      if (pChar->canNotify()) {
        if (pChar->subscribe(true, notifyCB)) {
          Serial.printf("BLE: Iscritto a: %s\n", pChar->getUUID().toString().c_str());
        }
      }
    }
  }
  return true;
}

void setupBLE() {
  NimBLEDevice::init("ESP32_EliRobot");
  NimBLEDevice::setSecurityAuth(true, true, true);
  Serial.println("BLE Init completato.");
}

// Reconnection handle
void loopBLE() {
  static unsigned long lastBleAttempt = 0;
  // Retry every 5 seconds
  if (!bleConnected && (millis() - lastBleAttempt > 5000)) {
    Serial.println("BLE: Tentativo connessione...");
    lastBleAttempt = millis();
    connectToJoystick();
  }
}

#endif