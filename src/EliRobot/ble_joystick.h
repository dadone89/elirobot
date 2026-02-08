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
// In ble_joystick.h -> notifyCB

void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {

  // DEBUG (check key, config only)
  /*
  Serial.print("BLE HEX: ");
  for (size_t i = 0; i < length; i++) {
    Serial.printf("%02X ", pData[i]);
  }
  Serial.println();
  */

  if (length > 1) {  // Check
    uint8_t byte0 = pData[0];
    uint8_t byte1 = pData[1];

    switch (byte1) {
      // Joypad
      case 0x90: bleCurrentCommand = BTN_U; break;
      case 0x10: bleCurrentCommand = BTN_D; break;
      case 0x60: bleCurrentCommand = BTN_L; break;
      case 0x40: bleCurrentCommand = BTN_R; break;

      // Lx eye
      case 0x52:  // A key
        bleCurrentCommand = BTN_EYE_SX_OPEN;
        break;
      case 0x51:  // C key
        bleCurrentCommand = BTN_EYE_SX_CLOSE;
        break;

      // Rx eye and release cmd, complicated case
      case 0x50:
        // Check byte 0 for discriminate keys
        if (byte0 == 0x01) {
          bleCurrentCommand = BTN_EYE_DX_OPEN;
        } else if (byte0 == 0x02) {
          bleCurrentCommand = BTN_EYE_DX_CLOSE;
        } else {
          // if byte 0 == 0 is a real release
          bleCurrentCommand = BTN_NONE;
        }
        break;

      default:
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