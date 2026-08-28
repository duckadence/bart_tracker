#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>

class WiFiManager {
public:
    static bool connect(const char* ssid, const char* password);
    static bool isConnected();
    static void disconnect();
};

#endif //WIFI_MANAGER_H