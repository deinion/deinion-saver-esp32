#include "WiFiManager.h"

WiFiManager::WiFiManager()
    : _state(WiFiState::DISCONNECTED)
    , _retryCount(0)
    , _lastRetryTime(0)
{}

bool WiFiManager::begin() {
    _retryCount = 0;  // Frisse start, ook na portal-timeout
    _loadCredentials();

    if (_ssid.isEmpty()) {
        Serial.println("[WiFi] Geen opgeslagen netwerk — captive portal nodig");
        _state = WiFiState::FAILED;
        return false;
    }

    return _connect();
}

void WiFiManager::update() {
    if (_state == WiFiState::CONNECTED) {
        // Controleer of verbinding nog actief is
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WiFi] Verbinding verloren");
            _state = WiFiState::DISCONNECTED;
            _retryCount = 0;
        }
        return;
    }

    if (_state == WiFiState::FAILED) return; // Wacht op BLE provisioning

    // Periodieke herverbindingspoging
    if (millis() - _lastRetryTime < WIFI_RETRY_INTERVAL_MS) return;

    if (_retryCount >= WIFI_MAX_RETRIES) {
        Serial.printf("[WiFi] %d pogingen mislukt — BLE provisioning starten\n",
                      WIFI_MAX_RETRIES);
        _state = WiFiState::FAILED;
        return;
    }

    _connect();
}

bool WiFiManager::_connect() {
    if (_ssid.isEmpty()) return false;

    _state = WiFiState::CONNECTING;
    _lastRetryTime = millis();
    _retryCount++;

    Serial.printf("[WiFi] Verbinden met '%s' (poging %d/%d)...\n",
                  _ssid.c_str(), _retryCount, WIFI_MAX_RETRIES);

    WiFi.mode(WIFI_STA);

    // Zendvermogen instellen: 17 dBm zodat EIRP ≤ 20 dBm met 3 dBi antenne (EU/NL-wetgeving)
    WiFi.setTxPower(WIFI_TX_POWER);

    WiFi.begin(_ssid.c_str(), _password.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println("[WiFi] Timeout");
            WiFi.disconnect(true);
            _state = WiFiState::DISCONNECTED;
            return false;
        }
        delay(500);
    }

    _state = WiFiState::CONNECTED;
    _retryCount = 0;
    Serial.printf("[WiFi] Verbonden — IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

void WiFiManager::setCredentials(const String& ssid, const String& password) {
    _ssid     = ssid;
    _password = password;

    // Opslaan in NVS (blijft bewaard na stroomonderbreking)
    _prefs.begin("wifi", false);
    _prefs.putString("ssid",     _ssid);
    _prefs.putString("password", _password);
    _prefs.end();

    Serial.printf("[WiFi] Nieuwe credentials opgeslagen voor '%s'\n", _ssid.c_str());

    // Reset en verbind opnieuw
    _retryCount = 0;
    _state = WiFiState::DISCONNECTED;
    WiFi.disconnect(true);
    delay(100);
    _connect();
}

void WiFiManager::clearCredentials() {
    _prefs.begin("wifi", false);
    _prefs.clear();
    _prefs.end();
    _ssid     = "";
    _password = "";
    _state    = WiFiState::FAILED;
    WiFi.disconnect(true);
    Serial.println("[WiFi] Credentials gewist");
}

void WiFiManager::_loadCredentials() {
    _prefs.begin("wifi", true); // read-only
    _ssid     = _prefs.getString("ssid",     "");
    _password = _prefs.getString("password", "");
    _prefs.end();

    if (!_ssid.isEmpty()) {
        Serial.printf("[WiFi] Opgeslagen netwerk geladen: '%s'\n", _ssid.c_str());
    }
}
