/*  Refactored BART‑only Transit Tracker
 *  - Uses modular managers for BLE, Wi‑Fi, and BART API
 *  - Prints predictions to Serial
 */

#include <Arduino.h>
#include "ble_manager.h"
#include "wifi_manager.h"
#include "bart_client.h"

// Global instances
BLEManager bleManager;
BartClient bartClient; // uses default demo key

unsigned long lastFetch = 0;
const unsigned long FETCH_INTERVAL = 30000; // 30 seconds
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 10000; // 10 seconds

void setup() {
    Serial.begin(115200);
    // Wait for serial to be ready (optional)
    while (!Serial) { delay(10); }

    Serial.println(F("\n=== Transit Tracker Starting ==="));
    bleManager.begin();
    // Initial Wi‑Fi connection attempt if credentials exist
    if (bleManager.isStationSet()) {
        // Try to connect using stored credentials
        WiFiManager::connect(bleManager.getStoredSsid().c_str(), bleManager.getStoredPassword().c_str());
    }
}

void loop() {
    unsigned long now = millis();

    // Periodic Wi‑Fi reconnect (if we have credentials saved)
    if (bleManager.isStationSet() && WiFi.status() != WL_CONNECTED &&
        now - lastWiFiCheck > WIFI_CHECK_INTERVAL) {
        lastWiFiCheck = now;
        Serial.println(F("Wi‑Fi disconnected, attempting reconnect..."));
        WiFiManager::connect(bleManager.getStoredSsid().c_str(), bleManager.getStoredPassword().c_str());
    }

    // Fetch BART predictions at the defined interval
    if (bleManager.isStationSet() && WiFi.status() == WL_CONNECTED &&
        now - lastFetch >= FETCH_INTERVAL) {
        lastFetch = now;
        String stationAbbr = bleManager.getStoredStation().c_str();
        BartStationData data;
        if (bartClient.fetchPredictions(stationAbbr.c_str(), data)) {
            Serial.print(F("Station: "));
            Serial.println(data.stationName);
            for (const auto& est : data.estimates) {
                Serial.print(est.destination);
                Serial.print(F(" | "));
                Serial.print(est.direction);
                Serial.print(F(" | "));
                Serial.print(est.minutes);
                Serial.println(F(" min"));
            }
            Serial.println(); // blank line after each fetch
        } else {
            Serial.println(F("Failed to fetch BART predictions"));
        }
    }

    delay(10);
}