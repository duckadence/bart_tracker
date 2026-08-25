#include <NimBLEDevice.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define BART_API_KEY "YOUR_BART_API_KEY"

#define NUM_STATIONS 3
#define CYCLE_TIME   10000
#define UPDATE_TIME  60000

NimBLECharacteristic *characteristic;
Preferences prefs;

String stations[NUM_STATIONS];

unsigned long lastUpdate = 0;
unsigned long lastCycle = 0;
int currentStation = 0;

// --------------------------------------------------
// BLE CONFIGURATION
// --------------------------------------------------

class ConfigCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *c,
               NimBLEConnInfo &connInfo) override {

    std::string raw = c->getValue();
    if (raw.empty()) return;

    String cmd = raw.c_str();

    Serial.print("BLE: ");
    Serial.println(cmd);

    processCommand(cmd);
  }

  void processCommand(String cmd) {

    if (cmd.startsWith("WIFI_SSID=")) {
      prefs.putString("ssid", cmd.substring(10));
      Serial.println("Wi-Fi SSID saved.");
    }

    else if (cmd.startsWith("WIFI_PASS=")) {
      prefs.putString("password", cmd.substring(10));
      Serial.println("Wi-Fi password saved.");
    }

    else if (cmd.startsWith("STATION_")) {
      int slot = cmd.charAt(8) - '1';

      if (slot >= 0 && slot < NUM_STATIONS) {
        stations[slot] = cmd.substring(10);

        prefs.putString(
          ("station" + String(slot)).c_str(),
          stations[slot]
        );

        Serial.printf(
          "Station %d saved: %s\n",
          slot + 1,
          stations[slot].c_str()
        );
      }
    }

    else if (cmd == "WIFI_CONNECT") {
      connectWiFi();
    }

    else if (cmd == "UPDATE") {
      updateStation(currentStation);
    }

    else if (cmd == "STATUS") {
      printConfig();
    }
  }
};

// --------------------------------------------------
// WIFI
// --------------------------------------------------

void connectWiFi() {

  String ssid = prefs.getString("ssid", "");
  String password = prefs.getString("password", "");

  if (ssid.length() == 0) {
    Serial.println("No Wi-Fi configured.");
    return;
  }

  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < 15000) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println("Wi-Fi connection failed.");
  }
}

// --------------------------------------------------
// BART API
// --------------------------------------------------

void updateStation(int index) {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No Wi-Fi.");
    return;
  }

  if (stations[index].length() == 0) {
    Serial.printf("Station %d not configured.\n", index + 1);
    return;
  }

  String url =
    "https://api.bart.gov/api/etd.aspx"
    "?cmd=etd"
    "&orig=" + stations[index] +
    "&key=" + BART_API_KEY +
    "&json=y";

  Serial.println();
  Serial.println("================================");
  Serial.print("Station: ");
  Serial.println(stations[index]);
  Serial.println("================================");

  HTTPClient http;

  http.begin(url);

  int response = http.GET();

  if (response != HTTP_CODE_OK) {
    Serial.print("HTTP error: ");
    Serial.println(response);
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;

  DeserializationError error =
    deserializeJson(doc, payload);

  if (error) {
    Serial.print("JSON error: ");
    Serial.println(error.c_str());
    return;
  }

  JsonArray estimates =
    doc["root"]["station"][0]["etd"];

  if (estimates.isNull()) {
    Serial.println("No arrival data.");
    return;
  }

  for (JsonObject train : estimates) {

    const char *destination =
      train["destination"];

    const char *abbreviation =
      train["abbreviation"];

    Serial.println();
    Serial.print(destination);

    if (abbreviation) {
      Serial.print(" (");
      Serial.print(abbreviation);
      Serial.print(")");
    }

    Serial.println();

    JsonArray estimatesList =
      train["estimate"];

    for (JsonObject estimate : estimatesList) {

      const char *minutes =
        estimate["minutes"];

      const char *direction =
        estimate["direction"];

      Serial.print("  ");

      if (direction) {
        Serial.print(direction);
        Serial.print("  ");
      }

      Serial.print(minutes);
      Serial.println(" min");
    }
  }
}

// --------------------------------------------------
// CONFIGURATION
// --------------------------------------------------

void loadConfig() {

  for (int i = 0; i < NUM_STATIONS; i++) {

    stations[i] =
      prefs.getString(
        ("station" + String(i)).c_str(),
        ""
      );
  }
}

void printConfig() {

  Serial.println();
  Serial.println("========== CONFIG ==========");

  Serial.print("Wi-Fi: ");

  if (prefs.getString("ssid", "").length())
    Serial.println(prefs.getString("ssid", ""));
  else
    Serial.println("(not configured)");

  for (int i = 0; i < NUM_STATIONS; i++) {

    Serial.printf(
      "Station %d: %s\n",
      i + 1,
      stations[i].length()
        ? stations[i].c_str()
        : "(not configured)"
    );
  }

  Serial.println("============================");
}

// --------------------------------------------------
// SETUP
// --------------------------------------------------

void setup() {

  Serial.begin(115200);
  delay(500);

  prefs.begin("config", false);

  loadConfig();

  // BLE
  NimBLEDevice::init("TRANSIT-TRACKER");

  NimBLEServer *server =
    NimBLEDevice::createServer();

  NimBLEService *service =
    server->createService(SERVICE_UUID);

  characteristic =
    service->createCharacteristic(
      CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::WRITE
    );

  characteristic->setCallbacks(
    new ConfigCallbacks()
  );

  service->start();

  NimBLEAdvertising *advertising =
    NimBLEDevice::getAdvertising();

  advertising->addServiceUUID(SERVICE_UUID);

  NimBLEDevice::startAdvertising();

  Serial.println();
  Serial.println("==============================");
  Serial.println("     TRANSIT TRACKER");
  Serial.println("==============================");

  connectWiFi();

  printConfig();
}

// --------------------------------------------------
// LOOP
// --------------------------------------------------

void loop() {

  // Reconnect Wi-Fi if necessary
  if (WiFi.status() != WL_CONNECTED) {

    static unsigned long lastReconnect = 0;

    if (millis() - lastReconnect > 10000) {
      lastReconnect = millis();
      connectWiFi();
    }
  }

  // Update BART data
  if (millis() - lastUpdate > UPDATE_TIME) {

    lastUpdate = millis();

    updateStation(currentStation);
  }

  // Cycle stations
  if (millis() - lastCycle > CYCLE_TIME) {

    lastCycle = millis();

    currentStation++;

    if (currentStation >= NUM_STATIONS)
      currentStation = 0;

    updateStation(currentStation);
  }

  delay(10);
}