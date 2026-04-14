#include <Arduino.h>
#include "P1Parser.h"
#include "WiFiManager.h"
#include "BLEProvisioning.h"

// ─── Pin configuratie ──────────────────────────────────────────────────────────
// Aanpassen na definitieve PCB-layout
#define P1_RX_PIN    4
#define P1_RTS_PIN   5
#define P1_BAUDRATE  115200

// ─── Globale objecten ──────────────────────────────────────────────────────────
HardwareSerial  P1Serial(1);
P1Parser        parser;
WiFiManager     wifiManager;
BLEProvisioning bleProvisioning;

bool bleActive = false;

// ─── Telegram lezen ───────────────────────────────────────────────────────────

String readTelegram() {
    String telegram = "";
    bool inTelegram = false;
    unsigned long timeout = millis() + 15000;

    while (millis() < timeout) {
        if (!P1Serial.available()) { delay(1); continue; }

        char c = (char)P1Serial.read();
        if (c == '/') {
            inTelegram = true;
            telegram   = "/";
        } else if (inTelegram) {
            telegram += c;
            // Einde telegram: '!' gevolgd door 4 CRC-tekens en newline
            int excl = telegram.lastIndexOf('!');
            if (excl >= 0 && (int)telegram.length() >= excl + 6) {
                return telegram;
            }
        }
    }

    Serial.println("[P1] Timeout: geen compleet telegram ontvangen");
    return "";
}

// ─── BLE starten / stoppen ────────────────────────────────────────────────────

void startBLE() {
    if (bleActive) return;
    Serial.println("[Main] WiFi niet beschikbaar — BLE provisioning starten");

    // WiFi uitschakelen voor BLE start (gedeelde RF-keten)
    WiFi.mode(WIFI_OFF);
    delay(100);

    bleProvisioning.start([](const String& ssid, const String& password) {
        // Callback: nieuwe credentials ontvangen via BLE
        Serial.printf("[Main] Nieuwe credentials ontvangen voor '%s'\n", ssid.c_str());

        // BLE stoppen voor WiFi-verbinding (gedeelde RF-keten)
        bleProvisioning.stop();
        bleActive = false;
        delay(200);

        // Verbinden met nieuwe credentials
        wifiManager.setCredentials(ssid, password);

        if (wifiManager.isConnected()) {
            Serial.println("[Main] WiFi verbonden — normaal bedrijf hervat");
        } else {
            Serial.println("[Main] Verbinding mislukt — BLE opnieuw starten");
            startBLE();
        }
    });

    bleActive = true;
}

// ─── Setup ────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    Serial.println("\n╔══════════════════════════════╗");
    Serial.println("║   Deinion Saver ESP32-S3     ║");
    Serial.println("╚══════════════════════════════╝");

    // P1 seriële poort
    P1Serial.begin(P1_BAUDRATE, SERIAL_8N1, P1_RX_PIN, -1);
    pinMode(P1_RTS_PIN, OUTPUT);
    digitalWrite(P1_RTS_PIN, HIGH);

    // WiFi verbinden
    bool connected = wifiManager.begin();
    if (!connected) {
        startBLE();
    }
}

// ─── Loop ─────────────────────────────────────────────────────────────────────

void loop() {
    // WiFi-toestand bijhouden
    wifiManager.update();

    // Als WiFi weggevallen is: BLE starten
    if (wifiManager.needsSetup() && !bleActive) {
        startBLE();
    }

    // BLE actief: geen P1-verwerking, wacht op credentials
    if (bleActive) {
        delay(100);
        return;
    }

    // P1 telegram lezen en verwerken
    String telegram = readTelegram();
    if (telegram.isEmpty()) return;

    P1Data data;
    if (!parser.parse(telegram, data)) {
        Serial.println("[P1] Telegram kon niet worden geparsed");
        return;
    }

    // Debug output — wordt later vervangen door opslag + webserver
    Serial.println("─────────────────────────────────────────");
    Serial.printf("[P1] %s%s\n",
                  data.timestamp,
                  data.crc_valid ? "" : "  ⚠ CRC-fout");
    Serial.printf("     Import  T1/T2:  %.3f / %.3f kWh\n",
                  data.elec_import_t1, data.elec_import_t2);
    Serial.printf("     Export  T1/T2:  %.3f / %.3f kWh\n",
                  data.elec_export_t1, data.elec_export_t2);
    Serial.printf("     Verbruik/Ret.:  %.3f / %.3f kW\n",
                  data.current_power_usage, data.current_power_return);
    Serial.printf("     Voltage L1/2/3: %.1f / %.1f / %.1f V\n",
                  data.voltage_l1, data.voltage_l2, data.voltage_l3);
    Serial.printf("     Gas:            %.3f m³  (%s)\n",
                  data.gas_total, data.gas_timestamp);
}
