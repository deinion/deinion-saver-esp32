#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

// BLE service UUID voor WiFi-provisioning
#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
// Characteristic: SSID insturen (write)
#define BLE_SSID_UUID           "beb5483e-36e1-4688-b7f5-ea07361b26a8"
// Characteristic: Wachtwoord insturen (write, encrypted)
#define BLE_PASS_UUID           "beb5483f-36e1-4688-b7f5-ea07361b26a8"
// Characteristic: Status lezen (notify)
#define BLE_STATUS_UUID         "beb54840-36e1-4688-b7f5-ea07361b26a8"

// BLE TX-vermogen: standaard 9 dBm → EIRP = 12 dBm met 3 dBi antenne (ruim binnen EU-norm)
// Geen aanpassing nodig.

// Callback zodat main.cpp weet wanneer nieuwe credentials binnenkomen
using ProvisioningCallback = std::function<void(const String& ssid, const String& password)>;

class BLEProvisioning : public NimBLEServerCallbacks {
public:
    BLEProvisioning();

    // Start BLE advertising als "Deinion-XXXX" (laatste 4 tekens MAC-adres)
    void start(ProvisioningCallback onCredentialsReceived);

    // Stop BLE en geef RF vrij voor WiFi
    void stop();

    bool isRunning() const { return _running; }

    // Stuur statusupdate naar verbonden telefoon
    void sendStatus(const String& status);

    // NimBLE server callbacks
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override;
    void onDisconnect(NimBLEServer* pServer) override;

private:
    bool                 _running;
    NimBLEServer*        _pServer;
    NimBLECharacteristic* _pStatusChar;
    ProvisioningCallback _callback;
    String               _pendingSsid;
    String               _pendingPassword;

    friend class CredentialWriteCallback;
};
