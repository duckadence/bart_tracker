#include "ble_manager.h"
#include "wifi_manager.h"
#include <Arduino.h>

// Forward declare callback class
class BLECharacteristicCallbacks : public NimBLECharacteristicCallbacks {
public:
    BLECharacteristicCallbacks(BLEManager* parent) : parent_(parent) {}
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        std::string value = pCharacteristic->getValue();
        if (!value.empty() && parent_) {
            parent_->handleBLECommands(value);
        }
    }
private:
    BLEManager* parent_;
};

BLEManager::BLEManager() {
    // Constructor
}

void BLEManager::begin() {
    prefs.begin("transit", false);
    loadPrefs();

    NimBLEDevice::init("TransitTracker");
    Serial.println(F("BLE: Device initialized"));

    NimBLEServer* server = NimBLEDevice::createServer();
    Serial.println(F("BLE: Server created"));

    NimBLEService* service = server->createService(SERVICE_UUID);
    Serial.println(F("BLE: Service created"));

    characteristic = service->createCharacteristic(
        CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
    );
    Serial.println(F("BLE: Characteristic created"));

    // Set up callbacks
    characteristic->setCallbacks(new BLECharacteristicCallbacks(this));
    Serial.println(F("BLE: Callbacks set"));

    service->start();
    Serial.println(F("BLE: Service started"));

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->setMinInterval(32);   // 20 ms
    advertising->setMaxInterval(160);  // 100 ms
    Serial.println(F("BLE: Starting advertising..."));
    NimBLEDevice::startAdvertising();
    Serial.println(F("BLE: Advertising started – device should be discoverable"));

    // Try to connect Wi‑Fi if we have credentials
    if (ssidSet && passwordSet) {
        // WiFiManager will be called from elsewhere; we just note credentials.
    }
}

void BLEManager::handleBLECommands(const std::string& value) {
    if (value.empty()) return;

    String cmd = String(value.c_str());
    Serial.print(F("BLE: "));
    Serial.println(cmd);

    if (cmd.startsWith("WIFI_SSID=")) {
        storedSsid = cmd.substring(10).c_str();
        ssidSet = true;
        saveString("ssid", storedSsid);
        Serial.println(F("Wi‑Fi SSID saved"));
    }
    else if (cmd.startsWith("WIFI_PASS=")) {
        storedPassword = cmd.substring(10).c_str();
        passwordSet = true;
        saveString("password", storedPassword);
        Serial.println(F("Wi‑Fi password saved"));
    }
    else if (cmd.startsWith("STATION=")) {
        storedStation = cmd.substring(8).c_str();
        stationSet = true;
        saveString("station", storedStation);
        Serial.print(F("Station saved: "));
        Serial.println(storedStation.c_str());
    }
    else if (cmd == "WIFI_CONNECT") {
        // Delegate to WiFiManager
        WiFiManager::connect(storedSsid.c_str(), storedPassword.c_str());
    }
    else if (cmd == "GET_STATION") {
        // Echo back the single stored station (useful for the web app)
        if (stationSet) {
            String reply = "1:" + String(storedStation.c_str());
            sendNotification(reply);
        } else {
            sendNotification("none");
        }
    }
    else if (cmd == "CLEAR") {
        ssidSet = passwordSet = stationSet = false;
        storedSsid = "";
        storedPassword = "";
        storedStation = "";
        prefs.remove("ssid");
        prefs.remove("password");
        prefs.remove("station");
        Serial.println(F("All settings cleared"));
    }
}

std::string BLEManager::getStoredStation() const {
    return storedStation;
}

bool BLEManager::isStationSet() const {
    return stationSet;
}

std::string BLEManager::getStoredSsid() const {
    return storedSsid;
}

std::string BLEManager::getStoredPassword() const {
    return storedPassword;
}

void BLEManager::clearAll() {
    ssidSet = passwordSet = stationSet = false;
    storedSsid.clear();
    storedPassword.clear();
    storedStation.clear();
    prefs.remove("ssid");
    prefs.remove("password");
    prefs.remove("station");
    Serial.println(F("All settings cleared"));
}

void BLEManager::sendNotification(const std::string& msg) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(msg.c_str());
    size_t len = msg.length();
    size_t offset = 0;
    while (offset < len) {
        size_t chunk = std::min<size_t>(20, len - offset);
        characteristic->setValue(data + offset, chunk);
        characteristic->notify();
        offset += chunk;
    }
}

void BLEManager::loadPrefs() {
    storedSsid = loadString("ssid");
    ssidSet = !storedSsid.empty();
    storedPassword = loadString("password");
    passwordSet = !storedPassword.empty();
    storedStation = loadString("station");
    stationSet = !storedStation.empty();
}

void BLEManager::saveString(const char* key, const std::string& value) {
    prefs.putString(key, value.c_str());
}

std::string BLEManager::loadString(const char* key, const std::string& def) {
    return prefs.getString(key, def.c_str());
}