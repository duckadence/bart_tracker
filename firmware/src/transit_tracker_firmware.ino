/*  Minimal BART‑only Transit Tracker
 *  - Single station (first one stored via BLE)
 *  - Wi‑Fi optional (if ssid/password saved)
 *  - Prints a simple, human‑readable prediction list to Serial
 */

#include <NimBLEDevice.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

Preferences prefs;
NimBLECharacteristic *characteristic = nullptr;

bool   ssidSet   = false;
bool   passwordSet = false;
bool   stationSet = false;
String storedSsid;
String storedPassword;
String storedStation;   // the BART abbreviation, e.g. "MLPT"

unsigned long lastFetch = 0;
const unsigned long FETCH_INTERVAL = 30000;   // 30 seconds between fetches

// ------------------------------------------------------------------
// BLE Callbacks – only the commands we need
// ------------------------------------------------------------------
class ConfigCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic) override {
    std::string value = pCharacteristic->getValue();
    if (value.empty()) return;

    String cmd = String(value.c_str());
    Serial.print(F("BLE: "));
    Serial.println(cmd);

    if (cmd.startsWith("WIFI_SSID=")) {
      storedSsid = cmd.substring(10);
      ssidSet = true;
      prefs.putString("ssid", storedSsid);
      Serial.println(F("Wi‑Fi SSID saved"));
    }
    else if (cmd.startsWith("WIFI_PASS=")) {
      storedPassword = cmd.substring(10);
      passwordSet = true;
      prefs.putString("password", storedPassword);
      Serial.println(F("Wi‑Fi password saved"));
    }
    else if (cmd.startsWith("STATION=")) {
      storedStation = cmd.substring(8);
      stationSet = true;
      prefs.putString("station", storedStation);
      Serial.print(F("Station saved: "));
      Serial.println(storedStation);
    }
    else if (cmd == "WIFI_CONNECT") {
      connectWiFi();
    }
    else if (cmd == "GET_STATION") {
      // Echo back the single stored station (useful for the web app)
      if (stationSet) {
        String reply = "1:" + storedStation;
        sendNotify(reply);
      } else {
        sendNotify("none");
      }
    }
    else if (cmd == "CLEAR") {
      ssidSet = passwordSet = stationSet = false;
      storedSsid = storedPassword = storedStation = "";
      prefs.remove("ssid");
      prefs.remove("password");
      prefs.remove("station");
      Serial.println(F("All settings cleared"));
    }
  }
};

// ------------------------------------------------------------------
// Helper: send a string as a BLE notification (max 20 bytes per packet)
// ------------------------------------------------------------------
void sendNotify(const String &msg) {
  const uint8_t *data = (const uint8_t *)msg.c_str();
  size_t len = msg.length();
  size_t offset = 0;
  while (offset < len) {
    size_t chunk = min(20u, len - offset);
    characteristic->setValue(data + offset, chunk);
    characteristic->notify();
    offset += chunk;
  }
}

// ------------------------------------------------------------------
// Wi‑Fi connection (uses saved credentials if present)
// ------------------------------------------------------------------
void connectWiFi() {
  if (!ssidSet || !passwordSet) {
    Serial.println(F("Wi‑Fi: No credentials saved"));
    return;
  }

  Serial.print(F("Wi‑Fi: Connecting to "));
  Serial.println(storedSsid);
  WiFi.begin(storedSsid.c_str(), storedPassword.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("Wi‑Fi: Connected"));
    Serial.print(F("IP: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("Wi‑Fi: Connection failed"));
  }
}

// ------------------------------------------------------------------
// Fetch BART predictions for the stored station and print them
// ------------------------------------------------------------------
void fetchAndPrintBART() {
  if (!stationSet) {
    Serial.println(F("No station configured"));
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("Wi‑Fi not connected"));
    return;
  }

  // Build the BART API URL (XML output)
  String url = "https://api.bart.gov/api/etd.aspx?cmd=etd&orig=";
  url += storedStation;
  url += "&key=MW9S-E7SL-26DU-VV8V";   // public demo key

  HTTPClient http;
  http.setTimeout(8000);
  http.begin(url);

  // BART uses HTTPS – skip cert verification (simple & works on most networks)
  if (http.getStreamPtr()) {
    static_cast<WiFiClientSecure*>(http.getStreamPtr())->setInsecure();
  }

  int httpCode = http.GET();
  if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
    WiFiClient *stream = http.getStreamPtr();
    String response;
    while (stream->connected() && stream->available()) {
      response += (char)stream->read();
    }

    // ---- Very lightweight XML parsing ----
    // Find <abbr> (station name)
    int abbrStart = response.indexOf("<abbr>");
    String stationName = "???";
    if (abbrStart >= 0) {
      int abbrEnd = response.indexOf("</abbr>", abbrStart);
      if (abbrEnd > abbrStart) {
        stationName = response.substring(abbrStart + 5, abbrEnd);
      }
    }
    Serial.print(F("Station: "));
    Serial.println(stationName);

    // Find each <etd> block
    int etdPos = response.indexOf("<etd>");
    while (etdPos >= 0) {
      // Destination
      int destStart = response.indexOf("<destination>", etdPos);
      int destEnd   = response.indexOf("</destination>", destStart);
      String destination = "-";
      if (destStart >= 0 && destEnd > destStart) {
        destination = response.substring(destStart + 12, destEnd);
      }

      // Loop through each <estimate> inside this <etd>
      int estPos = response.indexOf("<estimate>", etdPos);
      int etdEnd = response.indexOf("</etd>", etdPos);
      while (estPos >= 0 && estPos < etdEnd) {
        int minsStart = response.indexOf("<minutes>", estPos);
        int minsEnd   = response.indexOf("</minutes>", minsStart);
        int dirStart  = response.indexOf("<direction>", estPos);
        int dirEnd    = response.indexOf("</direction>", dirStart);

        String minutes = "-";
        String direction = "-";
        if (minsStart >= 0 && minsEnd > minsStart) {
          minutes = response.substring(minsStart + 8, minsEnd);
        }
        if (dirStart >= 0 && dirEnd > dirStart) {
          direction = response.substring(dirStart + 10, dirEnd);
        }

        Serial.print(destination);
        Serial.print(F(" | "));
        Serial.print(direction);
        Serial.print(F(" | "));
        Serial.print(minutes);
        Serial.println(F(" min"));

        estPos = response.indexOf("<estimate>", estPos + 1);
      }

      etdPos = response.indexOf("<etd>", etdPos + 1);
    }
    Serial.println();   // blank line after each fetch
  } else {
    Serial.print(F("[HTTP] GET failed, error: "));
    Serial.println(http.errorToString(httpCode).c_str());
  }
  http.end();
}

// ------------------------------------------------------------------
// Setup
// ------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  prefs.begin("transit", false);

  // Load saved values (if any)
  storedSsid   = prefs.getString("ssid", "");
  ssidSet      = !storedSsid.isEmpty();
  storedPassword = prefs.getString("password", "");
  passwordSet  = !storedPassword.isEmpty();
  storedStation = prefs.getString("station", "");
  stationSet   = !storedStation.isEmpty();

  // BLE init
  NimBLEDevice::init("TransitTracker");
  Serial.println(F("BLE: Device initialized"));

  NimBLEServer *server = NimBLEDevice::createServer();
  Serial.println(F("BLE: Server created"));

  NimBLEService *service = server->createService(SERVICE_UUID);
  Serial.println(F("BLE: Service created"));

  characteristic = service->createCharacteristic(
    CHARACTERISTIC_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
  );
  Serial.println(F("BLE: Characteristic created"));

  characteristic->setCallbacks(new ConfigCallbacks());
  Serial.println(F("BLE: Callbacks set"));

  service->start();
  Serial.println(F("BLE: Service started"));

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->setMinInterval(32);   // 20 ms
  advertising->setMaxInterval(160);  // 100 ms
  Serial.println(F("BLE: Starting advertising..."));
  NimBLEDevice::startAdvertising();
  Serial.println(F("BLE: Advertising started – device should be discoverable"));

  // Try to connect Wi‑Fi if we have credentials
  if (ssidSet && passwordSet) connectWiFi();
}

// ------------------------------------------------------------------
// Main loop
// ------------------------------------------------------------------
void loop() {
  unsigned long now = millis();

  // Periodic Wi‑Fi reconnect (if we have credentials)
  if (ssidSet && passwordSet && WiFi.status() != WL_CONNECTED &&
      now - lastFetch > 10000) {   // retry every 10 s if disconnected
    lastFetch = now;
    connectWiFi();
  }

  // Fetch BART predictions at the defined interval
  if (stationSet && WiFi.status() == WL_CONNECTED &&
      now - lastFetch >= FETCH_INTERVAL) {
    lastFetch = now;
    fetchAndPrintBART();
  }

  delay(10);
}