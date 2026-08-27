#include <NimBLEDevice.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define MAX_STATIONS 10

// Rate limiting: 60 calls/hour = 1 call/minute
// With 10 stations, that's 6 minutes per full cycle
const unsigned long STATION_INTERVAL = 60000; // 1 minute between station changes
const unsigned long WIFI_RETRY_INTERVAL = 30000;

// Track API calls to respect rate limits
unsigned long lastApiCallTime = 0;
int apiCallCount = 0;
const int MAX_API_CALLS_PER_HOUR = 60;
const unsigned long API_RESET_PERIOD = 3600000; // 1 hour in milliseconds

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

  void onWrite(NimBLECharacteristic *pCharacteristic) override {

    std::string value = pCharacteristic->getValue();

    if (value.empty())
      return;

    String command = String(value.c_str());

    Serial.print(F("BLE: "));
    Serial.println(command);

    // ----------------------------------------------
    // Wi-Fi SSID
    // ----------------------------------------------
    if (command.startsWith("WIFI_SSID=")) {
      prefs.putString("ssid", command.substring(10));
      Serial.println(F("SSID saved."));
    }

    // ----------------------------------------------
    // Wi-Fi password
    // ----------------------------------------------
    else if (command.startsWith("WIFI_PASS=")) {
      prefs.putString("password", command.substring(10));
      Serial.println(F("Password saved."));
    }

    // ----------------------------------------------
    // Provider Config (bart, sf-muni, vta, caltrain, custom)
    // ----------------------------------------------
    else if (command.startsWith("PROVIDER=")) {
      prefs.putString("provider", command.substring(9));
      Serial.println(F("Provider saved."));
    }

    // ----------------------------------------------
    // API Key
    // ----------------------------------------------
    else if (command.startsWith("API_KEY=")) {
      prefs.putString("apiKey", command.substring(8));
      Serial.println(F("API Key saved."));
    }

    // ----------------------------------------------
    // Custom API URL Base (Optional)
    // ----------------------------------------------
    else if (command.startsWith("API_URL=")) {
      prefs.putString("apiUrl", command.substring(8));
      Serial.println(F("API URL saved."));
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

        Serial.print(F("Station "));
        Serial.print(stationNumber);
        Serial.print(F(" saved: "));
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

      Serial.println(F("All stations cleared."));
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
  
  if (ssid.isEmpty() || password.isEmpty()) {
    Serial.println(F("Wi-Fi: missing credentials"));
    return;
  }

  Serial.print(F("Wi-Fi: connecting to "));
  Serial.println(ssid);

  WiFi.disconnect(true);
  delay(50); // Reduced delay

  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(250); // Reduced delay and timeout
    Serial.print(F("."));
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("Wi-Fi connected."));
    Serial.print(F("IP: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("Wi-Fi connection failed."));
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

// Check if we can make an API call (rate limiting for 511.org)
bool canMakeApiCall() {
  unsigned long now = millis();
  
  // Reset counter if an hour has passed
  if (now - lastApiCallTime >= API_RESET_PERIOD) {
    apiCallCount = 0;
    lastApiCallTime = now;
  }
  
  return apiCallCount < MAX_API_CALLS_PER_HOUR;
}

void recordApiCall() {
  apiCallCount++;
  lastApiCallTime = millis();
}

void getTransitData(const String &station) {
  if (station.isEmpty() || WiFi.status() != WL_CONNECTED)
    return;

  // Check rate limit for 511.org APIs
  String provider = prefs.getString("provider", F("bart"));
  provider.toLowerCase();
  bool is511Provider = (provider == "511" || provider == "muni" || provider == "vta" || provider == "caltrain");
  
  if (is511Provider && !canMakeApiCall()) {
    Serial.println(F("Rate limit reached, skipping API call"));
    return;
  }

  String apiKey = prefs.getString("apiKey", F("MW9S-E7SL-26DU-VV8V")); // Fallback default BART key
  String customUrl = prefs.getString("apiUrl", "");
  
  // Early return if no station configured
  if (station.isEmpty()) return;

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
    Serial.println(F("Error: Unknown URL"));
    return;
  }

  HTTPClient http;
  if (!http.begin(url)) return;

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.print(F("HTTP Error: "));
    Serial.println(code);
    http.end();
    return;
  }

  String response = http.getString();
  http.end();

  // Strip potential UTF-8 Byte Order Mark (BOM) occasionally sent by 511.org
  if (response.length() > 3 && response[0] == '\xEF' && response[1] == '\xBB' && response[2] == '\xBF') {
    response = response.substring(3);
  }

  // Use smaller JSON document - most transit APIs return relatively small responses
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response.c_str());

  if (error) {
    Serial.print(F("JSON Parse Failed"));
    http.end();
    return;
  }

  // Route to provider-specific JSON parser
  if (provider == "bart") {
    parseBARTJson(doc);
  } else if (provider == "511" || provider == "muni" || provider == "vta" || provider == "caltrain") {
    parse511Json(doc);
  } else {
    Serial.println(F("Response received successfully."));
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
      const char *color = train["color"] | "?";
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
        Serial.print(F(" | "));
        Serial.print(direction);
        Serial.print(F(" | "));
        Serial.println(minutes);
      }
    }
  }
}

void parse511Json(JsonDocument &doc) {
  JsonArray visits = doc["ServiceDelivery"]["StopMonitoringDelivery"]["MonitoredStopVisit"].as<JsonArray>();
  if (visits.isNull()) {
    Serial.println(F("No active departures found."));
    return;
  }

  for (JsonObject visit : visits) {
    JsonObject journey = visit["MonitoredVehicleJourney"];
    const char *lineRef = journey["LineRef"] | "?";
    const char *destinationName = journey["DestinationName"] | "Unknown";
    const char *expectedArrival = journey["MonitoredCall"]["ExpectedArrivalTime"] | "N/A";

    // Standardized Output: LINE | DESTINATION | ARRIVAL_TIME
    Serial.print(lineRef);
    Serial.print(F(" | "));
    Serial.print(destinationName);
    Serial.print(F(" | "));
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
    Serial.println(F("No stations configured."));
    return;
  }

  currentStation = next;
  currentStationCode = getStation(currentStation + 1);

  Serial.println();
  Serial.println(F("=============================="));
  Serial.print(F("Station: "));
  Serial.println(currentStationCode);
  Serial.println(F("=============================="));

  getTransitData(currentStationCode);
}

// ==================================================
// PRINT CONFIGURATION
// ==================================================

void printConfiguration() {
  Serial.println();
  Serial.println(F("=============================="));
  Serial.println(F("Transit Tracker"));
  Serial.println(F("=============================="));

  String ssid = prefs.getString("ssid", F("(none)"));
  String provider = prefs.getString("provider", F("bart"));

  Serial.print(F("Wi-Fi: "));
  Serial.println(ssid);

  Serial.print(F("Provider: "));
  Serial.println(provider);

  Serial.println(F("Stations:"));

  int count = 0;
  for (int i = 1; i <= MAX_STATIONS; i++) {
    String station = getStation(i);
    if (!station.isEmpty()) {
      Serial.print(F("  "));
      Serial.print(i);
      Serial.print(F(": "));
      Serial.println(station);
      count++;
    }
  }

  if (count == 0) {
    Serial.println(F("  (none)"));
  }

  Serial.println(F("=============================="));
}

// ==================================================
// SETUP
// ==================================================

void setup() {
  Serial.begin(115200);

  prefs.begin("transit", false);

  NimBLEDevice::init("TransitTracker"); // Shorter name saves flash

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

  delay(5); // Reduced delay in main loop
}