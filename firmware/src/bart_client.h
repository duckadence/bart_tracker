#ifndef BART_CLIENT_H
#define BART_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

struct BartEstimate {
    String destination;
    String direction;
    String minutes; // could be "Leaving", "Due", etc.
};

struct BartStationData {
    String stationName;
    std::vector<BartEstimate> estimates;
};

class BartClient {
public:
    BartClient(const char* apiKey = "MW9S-E7SL-26DU-VV8V");
    bool fetchPredictions(const char* stationAbbr, BartStationData& outData);

private:
    const char* apiKey_;
    const char* baseURL_ = "https://api.bart.gov/api/etd.aspx";

    String buildURL(const char* stationAbbr) const;
    bool parseXML(const String& xml, BartStationData& outData);
};

#endif //BART_CLIENT_H