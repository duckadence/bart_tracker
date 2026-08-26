#include <NimBLEDevice.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define MAX_STATIONS 10

const unsigned long STATION_INTERVAL = 10000;
const unsigned long WIFI_RETRY_INTERVAL = 30000;

Preferences prefs;

NimBLECharacteristic *characteristic = nullptr;

unsigned long lastStationChange = 0;
unsigned long lastWiFiAttempt = 0;

int currentStation = -1;
String currentStationCode;

// ==================================================
// FUNCTION PROTOTYPES
// ==================================================

void connectWiFi();
void getTransitData(const String &station);
void parseBARTJson(JsonDocument &doc);
void parse511Json(JsonDocument &doc);
String getStation(int index);
int findNextStation();
void selectNextStation();

// ==================================================
// BLE CALLBACK
// ==================================================

class ConfigCallbacks : public NimBLECharacteristicCallbacks {

  void onWrite(
    NimBLECharacteristic *pCharacteristic,
    NimBLEConnInfo &connInfo
  ) override {

    std::string value = pCharacteristic->getValue();

    if (value.empty())
      return;

    String command = String(value.c_str());

    Serial.print("BLE: ");
    Serial.println(command);

    // ----------------------------------------------
    // Wi-Fi SSID
    // ----------------------------------------------
    if (command.startsWith("WIFI_SSID=")) {
      prefs.putString("ssid", command.substring(10));
      Serial.println("SSID saved.");
    }

    // ----------------------------------------------
    // Wi-Fi password
    // ----------------------------------------------
    else if (command.startsWith("WIFI_PASS=")) {
      prefs.putString("password", command.substring(10));
      Serial.println("Password saved.");
    }

    // ----------------------------------------------
    // Provider Config (bart, sf-muni, vta, caltrain, custom)
    // ----------------------------------------------
    else if (command.startsWith("PROVIDER=")) {
      prefs.putString("provider", command.substring(9));
      Serial.println("Provider saved.");
    }

    // ----------------------------------------------
    // API Key
    // ----------------------------------------------
    else if (command.startsWith("API_KEY=")) {
      prefs.putString("apiKey", command.substring(8));
      Serial.println("API Key saved.");
    }

    // ----------------------------------------------
    // Custom API URL Base (Optional)
    // ----------------------------------------------
    else if (command.startsWith("API_URL=")) {
      prefs.putString("apiUrl", command.substring(8));
      Serial.println("API URL saved.");
    }

    // ----------------------------------------------
    // Stations
    // ----------------------------------------------
    else if (command.startsWith("STATION_")) {
      int equalsPos = command.indexOf('=');

      if (equalsPos < 0)
        return;

      int stationNumber = command.substring(8, equalsPos).toInt();

      if (stationNumber >= 1 && stationNumber <= MAX_STATIONS) {
        String station = command.substring(equalsPos + 1);
        String key = "station" + String(stationNumber);

        prefs.putString(key.c_str(), station);

        Serial.print("Station ");
        Serial.print(stationNumber);
        Serial.print(" saved: ");
        Serial.println(station);
      }
    }

    // ----------------------------------------------
    // Clear stations
    // ----------------------------------------------
    else if (command == "CLEAR_STATIONS") {
      for (int i = 1; i <= MAX_STATIONS; i++) {
        char key[10];
        snprintf(key, sizeof(key), "station%d", i);
        prefs.remove(key);
      }

      currentStation = -1;
      currentStationCode = "";

      Serial.println("All stations cleared.");
    }

    // ----------------------------------------------
    // Connect Wi-Fi
    // ----------------------------------------------
    else if (command == "WIFI_CONNECT") {
      connectWiFi();
    }
  }
};

// ==================================================
// WIFI
// ==================================================

void connectWiFi() {
  String ssid = prefs.getString("ssid", "");
  String password = prefs.getString("password", "");

  if (ssid.isEmpty()) {
    Serial.println("Wi-Fi: no network configured.");
    return;
  }

  Serial.print("Wi-Fi: connecting to ");
  Serial.println(ssid);

  WiFi.disconnect(true);
  delay(100);

  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi connection failed.");
  }
}

// ==================================================
// GET STATION
// ==================================================

String getStation(int index) {
  if (index < 1 || index > MAX_STATIONS)
    return "";

  char key[10];
  snprintf(key, sizeof(key), "station%d", index);

  return prefs.getString(key, "");
}

// ==================================================
// FIND NEXT STATION
// ==================================================

int findNextStation() {
  int start = currentStation + 1;

  if (start >= MAX_STATIONS)
    start = 0;

  for (int i = 0; i < MAX_STATIONS; i++) {
    int index = (start + i) % MAX_STATIONS;

    if (!getStation(index + 1).isEmpty()) {
      return index;
    }
  }

  return -1;
}

// ==================================================
// GENERIC TRANSIT API FETCHING
// ==================================================

void getTransitData(const String &station) {
  if (station.isEmpty() || WiFi.status() != WL_CONNECTED)
    return;

  String provider = prefs.getString("provider", "bart");
  String apiKey = prefs.getString("apiKey", "MW9S-E7SL-26DU-VV8V"); // Fallback default BART key
  String customUrl = prefs.getString("apiUrl", "");

  provider.toLowerCase();
  String url = "";

  // 1. BART API Structure
  if (provider == "bart") {
    url = "https://api.bart.gov/api/etd.aspx?cmd=etd&orig=" + station + "&key=" + apiKey + "&json=y";
  } 
  // 2. Bay Area 511.org Unified API (SF Muni, VTA, Caltrain, AC Transit)
  else if (provider == "511" || provider == "muni" || provider == "vta" || provider == "caltrain") {
    String agencyCode = provider;
    if (provider == "muni") agencyCode = "SF";
    else if (provider == "vta") agencyCode = "SC";
    
    url = "https://api.511.org/transit/StopMonitoring?api_key=" + apiKey + "&agency=" + agencyCode + "&stopCode=" + station + "&format=json";
  }
  // 3. Custom REST Endpoint Configured via BLE
  else if (!customUrl.isEmpty()) {
    url = customUrl;
    url.replace("{station}", station);
    url.replace("{key}", apiKey);
  }

  if (url.isEmpty()) {
    Serial.println("Error: Unknown agency provider or empty URL.");
    return;
  }

  HTTPClient http;
  if (!http.begin(url)) return;

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.print("HTTP Error code: ");
    Serial.println(code);
    http.end();
    return;
  }

  String response = http.getString();
  http.end();

  // Strip potential UTF-8 Byte Order Mark (BOM) occasionally sent by 511.org
  if (response.startsWith("\xEF\xBB\xBF")) {
    response = response.substring(3);
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);

  if (error) {
    Serial.print("JSON Parse Failed: ");
    Serial.println(error.c_str());
    return;
  }

  // Route to provider-specific JSON parser
  if (provider == "bart") {
    parseBARTJson(doc);
  } else if (provider == "511" || provider == "muni" || provider == "vta" || provider == "caltrain") {
    parse511Json(doc);
  } else {
    Serial.println("Response received successfully.");
  }
}

// ==================================================
// PARSERS
// ==================================================

void parseBARTJson(JsonDocument &doc) {
  JsonArray stations = doc["root"]["station"].as<JsonArray>();
  if (stations.isNull()) return;

  for (JsonObject stationData : stations) {
    const char *stationName = stationData["name"] | "Unknown";

    Serial.println();
    Serial.println(stationName);

    JsonArray trains = stationData["etd"].as<JsonArray>();
    if (trains.isNull()) continue;

    for (JsonObject train : trains) {
      const char *color = train["color"];
      if (color == nullptr || strlen(color) == 0) {
        color = train["abbreviation"] | "?";
      }

      JsonArray estimates = train["estimate"].as<JsonArray>();
      if (estimates.isNull()) continue;

      for (JsonObject estimate : estimates) {
        if (strcmp(estimate["cancelflag"] | "0", "1") == 0) continue;

        const char *direction = estimate["direction"] | "?";
        const char *minutes = estimate["minutes"] | "?";

        // COLOR | DIRECTION | TIME
        Serial.print(color);
        Serial.print(" | ");
        Serial.print(direction);
        Serial.print(" | ");
        Serial.println(minutes);
      }
    }
  }
}

void parse511Json(JsonDocument &doc) {
  JsonArray visits = doc["ServiceDelivery"]["StopMonitoringDelivery"]["MonitoredStopVisit"].as<JsonArray>();
  if (visits.isNull()) {
    Serial.println("No active departures found.");
    return;
  }

  for (JsonObject visit : visits) {
    JsonObject journey = visit["MonitoredVehicleJourney"];
    const char *lineRef = journey["LineRef"] | "?";
    const char *destinationName = journey["DestinationName"] | "Unknown";
    const char *expectedArrival = journey["MonitoredCall"]["ExpectedArrivalTime"] | "N/A";

    // Standardized Output: LINE | DESTINATION | ARRIVAL_TIME
    Serial.print(lineRef);
    Serial.print(" | ");
    Serial.print(destinationName);
    Serial.print(" | ");
    Serial.println(expectedArrival);
  }
}

// ==================================================
// SELECT NEXT STATION
// ==================================================

void selectNextStation() {
  int next = findNextStation();

  if (next < 0) {
    currentStation = -1;
    currentStationCode = "";
    Serial.println("No stations configured.");
    return;
  }

  currentStation = next;
  currentStationCode = getStation(currentStation + 1);

  Serial.println();
  Serial.println("==============================");
  Serial.print("Station: ");
  Serial.println(currentStationCode);
  Serial.println("==============================");

  getTransitData(currentStationCode);
}

// ==================================================
// PRINT CONFIGURATION
// ==================================================

void printConfiguration() {
  Serial.println();
  Serial.println("==============================");
  Serial.println("Transit Tracker");
  Serial.println("==============================");

  String ssid = prefs.getString("ssid", "(none)");
  String provider = prefs.getString("provider", "bart");

  Serial.print("Wi-Fi: ");
  Serial.println(ssid);

  Serial.print("Provider: ");
  Serial.println(provider);

  Serial.println("Stations:");

  int count = 0;
  for (int i = 1; i <= MAX_STATIONS; i++) {
    String station = getStation(i);
    if (!station.isEmpty()) {
      Serial.print("  ");
      Serial.print(i);
      Serial.print(": ");
      Serial.println(station);
      count++;
    }
  }

  if (count == 0) {
    Serial.println("  (none)");
  }

  Serial.println("==============================");
}

// ==================================================
// SETUP
// ==================================================

void setup() {
  Serial.begin(115200);

  prefs.begin("transit", false);

  NimBLEDevice::init("Transit Tracker");

  NimBLEServer *server = NimBLEDevice::createServer();
  NimBLEService *service = server->createService(SERVICE_UUID);

  characteristic = service->createCharacteristic(
    CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::WRITE
  );

  characteristic->setCallbacks(new ConfigCallbacks());
  service->start();

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  NimBLEDevice::startAdvertising();

  printConfiguration();

  if (!prefs.getString("ssid", "").isEmpty()) {
    connectWiFi();
  }

  if (WiFi.status() == WL_CONNECTED) {
    selectNextStation();
  }

  lastStationChange = millis();
}

// ==================================================
// LOOP
// ==================================================

void loop() {
  unsigned long now = millis();

  // Wi-Fi reconnect check
  if (WiFi.status() != WL_CONNECTED && now - lastWiFiAttempt >= WIFI_RETRY_INTERVAL) {
    lastWiFiAttempt = now;
    connectWiFi();
  }

  // Cycle stations
  if (WiFi.status() == WL_CONNECTED && now - lastStationChange >= STATION_INTERVAL) {
    lastStationChange = now;
    selectNextStation();
  }

  delay(10);
}