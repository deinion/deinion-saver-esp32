#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

// Hoe lang we wachten op een WiFi-verbinding voor we opgeven (ms)
#define WIFI_CONNECT_TIMEOUT_MS  30000
// Hoe lang tussen herverbindingspogingen (ms)
#define WIFI_RETRY_INTERVAL_MS   60000
// Aantal pogingen voor we BLE starten
#define WIFI_MAX_RETRIES         3

// WiFi TX-vermogen: 17 dBm zodat EIRP ≤ 20 dBm met 3 dBi antenne (EU/NL)
#define WIFI_TX_POWER            WIFI_POWER_17dBm

enum class WiFiState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    FAILED            // Max pogingen bereikt → BLE provisioning starten
};

class WiFiManager {
public:
    WiFiManager();

    // Laad opgeslagen credentials en probeer te verbinden.
    // Geeft true terug als verbinding gelukt is.
    bool begin();

    // Periodiek aanroepen in loop() — handelt herverbinding af.
    void update();

    // Sla nieuwe WiFi-credentials op en verbind opnieuw.
    void setCredentials(const String& ssid, const String& password);

    // Verwijder opgeslagen credentials (factory reset)
    void clearCredentials();

    bool        isConnected()        const { return _state == WiFiState::CONNECTED; }
    bool        needsSetup()         const { return _state == WiFiState::FAILED; }
    bool        hasSavedCredentials() const { return !_ssid.isEmpty(); }
    WiFiState   getState()           const { return _state; }
    String      getSSID()            const { return _ssid; }
    IPAddress   getIP()              const { return WiFi.localIP(); }

private:
    bool        _connect();
    void        _loadCredentials();

    String      _ssid;
    String      _password;
    WiFiState   _state;
    uint8_t     _retryCount;
    unsigned long _lastRetryTime;
    Preferences _prefs;
};
