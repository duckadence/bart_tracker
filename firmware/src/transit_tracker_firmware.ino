#include <NimBLEDevice.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define MAX_STATIONS 10

// Timing defaults
unsigned long STATION_SWITCH_INTERVAL = 60000; // 1 minute
unsigned long API_FETCH_INTERVAL = 300000;    // 5 minutes
const unsigned long WIFI_RETRY_INTERVAL = 30000;

Preferences prefs;
NimBLECharacteristic *characteristic = nullptr;

unsigned long lastStationChange = 0;
unsigned long lastApiFetch = 0;
unsigned long lastWiFiAttempt = 0;

int currentStation = -1;
String currentStationCode;
String cachedTransitData = "";

// Function prototypes
void connectWiFi();
void getTransitData(const String &station);
void parseBARTJson(JsonDocument &doc);
void parse511Json(JsonDocument &doc);
String getStation(int index);
int findNextStation();
void selectNextStation();

class ConfigCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic) override {
    std::string value = pCharacteristic->getValue();
    if (value.empty()) return;

    String command = String(value.c_str());
    Serial.print(F("BLE: "));
    Serial.println(command);

    if (command.startsWith("WIFI_SSID=")) prefs.putString("ssid", command.substring(10));
    else if (command.startsWith("WIFI_PASS=")) prefs.putString("password", command.substring(10));
    else if (command.startsWith("PROVIDER=")) prefs.putString("provider", command.substring(9));
    else if (command.startsWith("API_KEY=")) prefs.putString("apiKey", command.substring(8));
    else if (command.startsWith("API_URL=")) prefs.putString("apiUrl", command.substring(8));
    else if (command.startsWith("SWITCH_INT=")) {
        STATION_SWITCH_INTERVAL = command.substring(11).toInt();
        prefs.putULong("switchInt", STATION_SWITCH_INTERVAL);
    }
    else if (command.startsWith("API_INT=")) {
        API_FETCH_INTERVAL = command.substring(8).toInt();
        prefs.putULong("apiInt", API_FETCH_INTERVAL);
    }
    else if (command.startsWith("STATION_")) {
      int equalsPos = command.indexOf('=');
      if (equalsPos < 0) return;
      int stationNumber = command.substring(8, equalsPos).toInt();
      if (stationNumber >= 1 && stationNumber <= MAX_STATIONS) {
        String key = "station" + String(stationNumber);
        prefs.putString(key.c_str(), command.substring(equalsPos + 1));
      }
    }
    else if (command == "CLEAR_STATIONS") {
      for (int i = 1; i <= MAX_STATIONS; i++) {
        char key[10]; snprintf(key, sizeof(key), "station%d", i);
        prefs.remove(key);
      }
      currentStation = -1;
    }
    else if (command == "GET_STATIONS") {
        // Send stations back via notify (simplified for now by printing)
        Serial.println(F("CONFIGURED_STATIONS:"));
        for(int i=1; i<=MAX_STATIONS; i++) {
            String s = getStation(i);
            if(!s.isEmpty()) { Serial.print(i); Serial.print(":"); Serial.println(s); }
        }
    }
    else if (command == "WIFI_CONNECT") connectWiFi();
  }
};

void connectWiFi() {
  String ssid = prefs.getString("ssid", "");
  String password = prefs.getString("password", "");
  if (ssid.isEmpty() || password.isEmpty()) return;
  WiFi.begin(ssid.c_str(), password.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) delay(250);
}

String getStation(int index) {
  char key[10]; snprintf(key, sizeof(key), "station%d", index);
  return prefs.getString(key, "");
}

int findNextStation() {
  for (int i = 0; i < MAX_STATIONS; i++) {
    int index = (currentStation + 1 + i) % MAX_STATIONS;
    if (!getStation(index + 1).isEmpty()) return index;
  }
  return -1;
}

void getTransitData(const String &station) {
  // Logic to only fetch every API_FETCH_INTERVAL
  if (WiFi.status() != WL_CONNECTED) return;
  
  // Implementation of actual HTTP request would go here, 
  // currently simplified to focus on structure
  Serial.print(F("Fetching data for: "));
  Serial.println(station);
  cachedTransitData = "Data for " + station; // Placeholder
}

void selectNextStation() {
  int next = findNextStation();
  if (next < 0) return;
  currentStation = next;
  currentStationCode = getStation(currentStation + 1);
  
  // Only re-fetch if interval passed
  if (millis() - lastApiFetch >= API_FETCH_INTERVAL) {
      getTransitData(currentStationCode);
      lastApiFetch = millis();
  }
}

void setup() {
  Serial.begin(115200);
  prefs.begin("transit", false);
  STATION_SWITCH_INTERVAL = prefs.getULong("switchInt", 60000);
  API_FETCH_INTERVAL = prefs.getULong("apiInt", 300000);

  NimBLEDevice::init("TransitTracker");
  NimBLEServer *server = NimBLEDevice::createServer();
  NimBLEService *service = server->createService(SERVICE_UUID);
  characteristic = service->createCharacteristic(CHARACTERISTIC_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  characteristic->setCallbacks(new ConfigCallbacks());
  service->start();
  NimBLEDevice::startAdvertising();

  if (!prefs.getString("ssid", "").isEmpty()) connectWiFi();
}

void loop() {
  unsigned long now = millis();
  if (WiFi.status() == WL_CONNECTED && now - lastStationChange >= STATION_SWITCH_INTERVAL) {
    lastStationChange = now;
    selectNextStation();
  }
  delay(10);
}
