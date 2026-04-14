#include "BLEProvisioning.h"

// ─── Write-callback voor SSID en wachtwoord ────────────────────────────────────

class CredentialWriteCallback : public NimBLECharacteristicCallbacks {
public:
    CredentialWriteCallback(BLEProvisioning* prov, bool isPassword)
        : _prov(prov), _isPassword(isPassword) {}

    void onWrite(NimBLECharacteristic* pChar) override {
        String value = pChar->getValue().c_str();
        if (_isPassword) {
            _prov->_pendingPassword = value;
            Serial.println("[BLE] Wachtwoord ontvangen");
        } else {
            _prov->_pendingSsid = value;
            Serial.printf("[BLE] SSID ontvangen: '%s'\n", value.c_str());
        }

        // Zodra beide velden gevuld zijn, callback aanroepen
        if (!_prov->_pendingSsid.isEmpty() && !_prov->_pendingPassword.isEmpty()) {
            _prov->sendStatus("OK: verbinden...");
            if (_prov->_callback) {
                _prov->_callback(_prov->_pendingSsid, _prov->_pendingPassword);
            }
            _prov->_pendingSsid     = "";
            _prov->_pendingPassword = "";
        }
    }

private:
    BLEProvisioning* _prov;
    bool _isPassword;
};

// ─── BLEProvisioning implementatie ────────────────────────────────────────────

BLEProvisioning::BLEProvisioning()
    : _running(false), _pServer(nullptr), _pStatusChar(nullptr) {}

void BLEProvisioning::start(ProvisioningCallback onCredentialsReceived) {
    if (_running) return;
    _callback = onCredentialsReceived;

    // Apparaatnaam: "Deinion-XXXX" met laatste 4 tekens van MAC-adres
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char devName[20];
    snprintf(devName, sizeof(devName), "Deinion-%02X%02X", mac[4], mac[5]);

    Serial.printf("[BLE] Starten als '%s'...\n", devName);

    NimBLEDevice::init(devName);

    // TX-vermogen: standaard 9 dBm — EIRP = 12 dBm met 3 dBi, ruim binnen EU 20 dBm limiet
    // Geen aanpassing nodig, maar voor de volledigheid expliciet instellen:
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); // +9 dBm maximum

    _pServer = NimBLEDevice::createServer();
    _pServer->setCallbacks(this);

    NimBLEService* pService = _pServer->createService(BLE_SERVICE_UUID);

    // SSID characteristic (write)
    NimBLECharacteristic* pSsidChar = pService->createCharacteristic(
        BLE_SSID_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    pSsidChar->setCallbacks(new CredentialWriteCallback(this, false));

    // Wachtwoord characteristic (write) — geen lees-eigenschap voor veiligheid
    NimBLECharacteristic* pPassChar = pService->createCharacteristic(
        BLE_PASS_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    pPassChar->setCallbacks(new CredentialWriteCallback(this, true));

    // Status characteristic (notify) — ESP32 stuurt berichten naar telefoon
    _pStatusChar = pService->createCharacteristic(
        BLE_STATUS_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    _pStatusChar->setValue("Wachten op WiFi-gegevens...");

    pService->start();

    NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
    pAdv->addServiceUUID(BLE_SERVICE_UUID);
    pAdv->setScanResponse(true);
    pAdv->start();

    _running = true;
    Serial.println("[BLE] Adverteren gestart — verbind met de Deinion app");
}

void BLEProvisioning::stop() {
    if (!_running) return;
    NimBLEDevice::getAdvertising()->stop();
    NimBLEDevice::deinit(true); // Geeft RAM en RF vrij voor WiFi
    _running = false;
    Serial.println("[BLE] Gestopt");
}

void BLEProvisioning::sendStatus(const String& status) {
    if (_pStatusChar && _running) {
        _pStatusChar->setValue(status.c_str());
        _pStatusChar->notify();
    }
}

void BLEProvisioning::onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
    Serial.printf("[BLE] Telefoon verbonden: %s\n",
                  NimBLEAddress(desc->peer_ota_addr).toString().c_str());
    sendStatus("Verbonden — vul SSID en wachtwoord in");
}

void BLEProvisioning::onDisconnect(NimBLEServer* pServer) {
    Serial.println("[BLE] Telefoon verbroken — herstart adverteren");
    NimBLEDevice::getAdvertising()->start();
}
