#include "wifi_manager.h"
#include <Arduino.h>

bool WiFiManager::connect(const char* ssid, const char* password) {
    if (!ssid || !*ssid || !password) {
        Serial.println(F("Wi‑Fi: No credentials provided"));
        return false;
    }

    Serial.print(F("Wi‑Fi: Connecting to "));
    Serial.println(ssid);
    WiFi.begin(ssid, password);

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
        return true;
    } else {
        Serial.println(F("Wi‑Fi: Connection failed"));
        return false;
    }
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void WiFiManager::disconnect() {
    WiFi.disconnect(true);
    Serial.println(F("Wi‑Fi: Disconnected"));
}