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
bool stationSet[MAX_STATIONS] = {false};
bool apiUrlSet = false;

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
    else if (command.startsWith("API_URL=")) {
            prefs.putString("apiUrl", command.substring(8));
            apiUrlSet = true;
          }
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
        String stationCode = command.substring(equalsPos + 1);
        prefs.putString(key.c_str(), stationCode);
        stationSet[stationNumber - 1] = true;
        
        Serial.print(F("Station "));
        Serial.print(stationNumber);
        Serial.print(F(" saved: "));
        Serial.println(stationCode);
        
        // Force API fetch for this station if WiFi is connected
        if (WiFi.status() == WL_CONNECTED) {
          getTransitData(stationCode);
        }
      }
    }
    else if (command == "CLEAR_STATIONS") {
      for (int i = 1; i <= MAX_STATIONS; i++) {
        char key[10]; snprintf(key, sizeof(key), "station%d", i);
        prefs.remove(key);
        stationSet[i-1] = false;
      }
      currentStation = -1;
    }
    else if (command == "GET_STATIONS") {
        // Send stations back via notify (simplified for now by printing)
        Serial.println(F("CONFIGURED_STATIONS:"));
        for(int i=1; i<=MAX_STATIONS; i++) {
            if (stationSet[i-1]) {
                String s = getStation(i);
                Serial.print(i); Serial.print(":"); Serial.println(s);
            }
        }
    }
    else if (command == "WIFI_CONNECT") connectWiFi();
  }
};

void connectWiFi() {
  String ssid = prefs.getString("ssid", "");
  String password = prefs.getString("password", "");
  if (ssid.isEmpty() || password.isEmpty()) {
    Serial.println(F("WiFi: No credentials saved"));
    return;
  }
  
  Serial.print(F("WiFi: Connecting to "));
  Serial.print(ssid);
  Serial.println(F("..."));
  
  WiFi.begin(ssid.c_str(), password.c_str());
  unsigned long start = millis();
  
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(250);
    Serial.print(F("."));
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("WiFi: Connected successfully"));
    Serial.print(F("WiFi: IP Address: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("WiFi: Connection failed"));
  }
}

String getStation(int index) {
  char key[10]; snprintf(key, sizeof(key), "station%d", index);
  return prefs.getString(key, "");
}

int findNextStation() {
  for (int i = 0; i < MAX_STATIONS; i++) {
    int index = (currentStation + 1 + i) % MAX_STATIONS;
    if (stationSet[index]) return index;
  }
  return -1;
}

void getTransitData(const String &station) {
  // Rate limiting: only fetch every API_FETCH_INTERVAL
  unsigned long now = millis();
  if (now - lastApiFetch < API_FETCH_INTERVAL) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!apiUrlSet) {
    Serial.println(F("No API URL set"));
    return;
  }
  String apiUrl = prefs.getString("apiUrl", "");
  String apiKey = prefs.getString("apiKey", "");

  // Build final URL: we assume the user has set a valid URL via BLE
  // that includes any necessary station and key parameters.
  // For simplicity, we do not modify the URL here.
  HTTPClient http;
  http.begin(apiUrl); // TODO: consider adding station and key if needed
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      cachedTransitData = payload;
      Serial.println(cachedTransitData);
      lastApiFetch = now;
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

void selectNextStation() {
  int next = findNextStation();
  if (next < 0) return;
  currentStation = next;
  currentStationCode = getStation(currentStation + 1);
  
  // Always fetch fresh data when switching to a new station
  if (WiFi.status() == WL_CONNECTED) {
    getTransitData(currentStationCode);
  }
}

void setup() {
  Serial.begin(115200);
  prefs.begin("transit", false);
  STATION_SWITCH_INTERVAL = prefs.getULong("switchInt", 60000);
  API_FETCH_INTERVAL = prefs.getULong("apiInt", 300000);

  // BLE Setup
  NimBLEDevice::init("TransitTracker");
  Serial.println("BLE: Device initialized");
  
  NimBLEServer *server = NimBLEDevice::createServer();
  Serial.println("BLE: Server created");
  
  NimBLEService *service = server->createService(SERVICE_UUID);
  Serial.println("BLE: Service created");
  
  characteristic = service->createCharacteristic(
    CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
  );
  Serial.println("BLE: Characteristic created");
  
  characteristic->setCallbacks(new ConfigCallbacks());
  Serial.println("BLE: Callbacks set");
  
  service->start();
  Serial.println("BLE: Service started");
  
  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->setMinInterval(32); // 20ms
  advertising->setMaxInterval(160); // 100ms
  Serial.println("BLE: Starting advertising...");
  NimBLEDevice::startAdvertising();
  Serial.println("BLE: Advertising started - device should be discoverable");

  String storedSsid = prefs.getString("ssid", "");
    if (!storedSsid.isEmpty()) connectWiFi();
    String storedApiUrl = prefs.getString("apiUrl", "");
    if (!storedApiUrl.isEmpty()) apiUrlSet = true;
}

void loop() {
  unsigned long now = millis();
  
  // Wi-Fi reconnect check
  if (WiFi.status() != WL_CONNECTED && now - lastWiFiAttempt >= WIFI_RETRY_INTERVAL) {
    lastWiFiAttempt = now;
    connectWiFi();
  }

  // Only cycle stations if we have at least one configured
  bool hasStations = false;
  for (int i = 0; i < MAX_STATIONS; i++) {
    if (stationSet[i]) {
      hasStations = true;
      break;
    }
  }
  
  if (WiFi.status() == WL_CONNECTED && hasStations && now - lastStationChange >= STATION_SWITCH_INTERVAL) {
    lastStationChange = now;
    selectNextStation();
  }
  
  // Periodic status update every 30 seconds
  static unsigned long lastStatusUpdate = 0;
  if (now - lastStatusUpdate >= 30000) {
    lastStatusUpdate = now;
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(F("Status: WiFi connected, waiting for station cycle"));
    } else {
      Serial.println(F("Status: WiFi disconnected, attempting to reconnect..."));
    }
  }
  
  delay(10);
}
