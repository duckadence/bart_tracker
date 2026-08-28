#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <NimBLEDevice.h>
#include <Preferences.h>
#include <string>

class BLEManager {
public:
    BLEManager();
    void begin();
    void handleBLECommands(const std::string& value);
    std::string getStoredStation() const;
    bool isStationSet() const;
    std::string getStoredSsid() const;
    std::string getStoredPassword() const;
    void clearAll();
    void sendNotification(const std::string& msg);

private:
    Preferences prefs;
    NimBLECharacteristic* characteristic = nullptr;
    bool ssidSet = false;
    bool passwordSet = false;
    bool stationSet = false;
    std::string storedSsid;
    std::string storedPassword;
    std::string storedStation;
    static constexpr const char* SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
    static constexpr const char* CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";

    void loadPrefs();
    void saveString(const char* key, const std::string& value);
    std::string loadString(const char* key, const std::string& def = "");
    void connectWiFi(); // declaration; definition in wifi_manager
};

#endif //BLE_MANAGER_H