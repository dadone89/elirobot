/**
 * MAGICSEE R1 - NIMBLE FIX OPERATOR*
 */

#include <NimBLEDevice.h>

static NimBLEAddress pServerAddress("ff:25:12:02:12:80", 0);
NimBLEClient* pClient = nullptr;
bool connected = false;

void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    Serial.print("UUID: ");
    Serial.print(pRemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" | HEX: ");
    for (size_t i = 0; i < length; i++) {
        if(pData[i] < 16) Serial.print("0");
        Serial.print(pData[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) { Serial.println("-> Connesso."); }
    void onDisconnect(NimBLEClient* pClient) {
        Serial.println("-> Disconnesso.");
        connected = false;
    }
};

bool connectToServer() {
    pClient = NimBLEDevice::getClientByPeerAddress(pServerAddress);
    
    if(!pClient) {
        pClient = NimBLEDevice::createClient();
        pClient->setClientCallbacks(new ClientCallbacks(), true);
    }

    if (!pClient->isConnected()) {
        if (!pClient->connect(pServerAddress)) {
            Serial.println("Connessione FALLITA.");
            return false;
        }
    }

    Serial.println("Ricerca Servizi...");

    // NOTA: Rimosso '*' davanti a pClient->getServices
    // La versione 2.x restituisce un riferimento, quindi non serve dereferenziare
    auto services = pClient->getServices(true); 
    for (auto pSvc : services) {
        
        auto characteristics = pSvc->getCharacteristics(true);
        for (auto pChar : characteristics) {
            
            if(pChar->canNotify()) {
                if(pChar->subscribe(true, notifyCB)) {
                    Serial.printf("Iscritto a: %s\n", pChar->getUUID().toString().c_str());
                }
            }
        }
    }

    connected = true;
    return true;
}

void setup() {
    Serial.begin(115200);
    NimBLEDevice::init("ESP32_NimBLE");
    NimBLEDevice::setSecurityAuth(true, true, true);
}

void loop() {
    if(!connected) {
        if(!connectToServer()) delay(5000);
    }
    delay(1000);
}