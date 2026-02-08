#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

int scanTime = 10; 
BLEScan* pBLEScan;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      // Stampa tutto quello che trova
      Serial.print("MAC: ");
      Serial.print(advertisedDevice.getAddress().toString().c_str());
      Serial.print(" | RSSI: ");
      Serial.print(advertisedDevice.getRSSI());
      
      if (advertisedDevice.haveName()) {
        Serial.print(" | Nome: ");
        Serial.println(advertisedDevice.getName().c_str());
      } else {
        Serial.println(" | Nome: [Nessuno]");
      }
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("--- Scansione BLE (Modalità A) ---");

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); 
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); 
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
}

void loop() {
  Serial.println("Cerco...");
  // Sintassi corretta per ESP32 v3.x
  BLEScanResults *foundDevices = pBLEScan->start(scanTime, false);
  Serial.print("Trovati: ");
  Serial.println(foundDevices->getCount());
  pBLEScan->clearResults();   
  delay(3000);
}