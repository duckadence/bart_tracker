#include "bart_client.h"
#include <Arduino.h>

BartClient::BartClient(const char* apiKey) : apiKey_(apiKey) {}

String BartClient::buildURL(const char* stationAbbr) const {
    String url = baseURL_;
    url += "?cmd=etd&orig=";
    url += stationAbbr;
    url += "&key=";
    url += apiKey_;
    return url;
}

bool BartClient::fetchPredictions(const char* stationAbbr, BartStationData& outData) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("Wi‑Fi not connected"));
        return false;
    }

    String url = buildURL(stationAbbr);
    Serial.print(F("Fetching URL: "));
    Serial.println(url);

    HTTPClient http;
    http.setTimeout(8000);
    http.begin(url);

    // BART uses HTTPS – skip cert verification (simple & works on most networks)
    if (http.getStreamPtr()) {
        static_cast<WiFiClientSecure*>(http.getStreamPtr())->setInsecure();
    }

    int httpCode = http.GET();
    if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
        WiFiClient* stream = http.getStreamPtr();
        String response;
        while (stream->connected() && stream->available()) {
            response += (char)stream->read();
        }
        http.end();

        if (parseXML(response, outData)) {
            return true;
        } else {
            Serial.println(F("Failed to parse BART XML"));
            return false;
        }
    } else {
        Serial.print(F("[HTTP] GET failed, error: "));
        Serial.println(http.errorToString(httpCode).c_str());
        http.end();
        return false;
    }
}

bool BartClient::parseXML(const String& xml, BartStationData& outData) {
    // Very lightweight XML parsing - find <station> name and <etd> blocks
    int stationNameStart = xml.indexOf("<station>");
    if (stationNameStart < 0) {
        // Try <abbr> as fallback (original code)
        stationNameStart = xml.indexOf("<abbr>");
        if (stationNameStart < 0) return false;
        int stationNameEnd = xml.indexOf("</abbr>", stationNameStart);
        if (stationNameEnd <= stationNameStart) return false;
        outData.stationName = xml.substring(stationNameStart + 6, stationNameEnd);
    } else {
        int stationNameEnd = xml.indexOf("</station>", stationNameStart);
        if (stationNameEnd <= stationNameStart) return false;
        outData.stationName = xml.substring(stationNameStart + 9, stationNameEnd);
    }

    // Find each <etd> block
    int etdPos = xml.indexOf("<etd>");
    while (etdPos >= 0) {
        // Destination
        int destStart = xml.indexOf("<destination>", etdPos);
        int destEnd   = xml.indexOf("</destination>", destStart);
        String destination = "-";
        if (destStart >= 0 && destEnd > destStart) {
            destination = xml.substring(destStart + 12, destEnd);
        }

        int etdEnd = xml.indexOf("</etd>", etdPos);
        int estPos = xml.indexOf("<estimate>", etdPos);
        while (estPos >= 0 && estPos < etdEnd) {
            int minsStart = xml.indexOf("<minutes>", estPos);
            int minsEnd   = xml.indexOf("</minutes>", minsStart);
            int dirStart  = xml.indexOf("<direction>", estPos);
            int dirEnd    = xml.indexOf("</direction>", dirStart);

            String minutes = "-";
            String direction = "-";
            if (minsStart >= 0 && minsEnd > minsStart) {
                minutes = xml.substring(minsStart + 8, minsEnd);
            }
            if (dirStart >= 0 && dirEnd > dirStart) {
                direction = xml.substring(dirStart + 10, dirEnd);
            }

            BartEstimate est;
            est.destination = destination;
            est.direction = direction;
            est.minutes = minutes;
            outData.estimates.push_back(est);

            estPos = xml.indexOf("<estimate>", estPos + 1);
        }

        etdPos = xml.indexOf("<etd>", etdPos + 1);
    }

    return !outData.estimates.empty();
}