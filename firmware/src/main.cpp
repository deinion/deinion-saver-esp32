#include <Arduino.h>
#include "P1Parser.h"
#include "WiFiManager.h"
#include "CaptivePortal.h"
#include "PowerFailureLog.h"

// ─── Pin configuratie ──────────────────────────────────────────────────────────
#define P1_RX_PIN    4
#define P1_RTS_PIN   5
#define P1_BAUDRATE  115200

// ─── Globale objecten ──────────────────────────────────────────────────────────
HardwareSerial  P1Serial(1);
P1Parser        parser;
WiFiManager     wifiManager;
CaptivePortal   portal;
PowerFailureLog failureLog;

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
            int excl = telegram.lastIndexOf('!');
            if (excl >= 0 && (int)telegram.length() >= excl + 6) {
                return telegram;
            }
        }
    }

    Serial.println("[P1] Timeout: geen compleet telegram ontvangen");
    return "";
}

// ─── Captive portal starten ───────────────────────────────────────────────────

void startPortal() {
    Serial.println("[Main] WiFi niet beschikbaar — captive portal starten");

    portal.start([](const String& ssid, const String& password) {
        portal.stop();
        delay(200);

        wifiManager.setCredentials(ssid, password);

        if (!wifiManager.isConnected()) {
            Serial.println("[Main] Verbinding mislukt — portal opnieuw starten");
            startPortal();
        }
    });
}

// ─── Setup ────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    Serial.println("\n╔══════════════════════════════╗");
    Serial.println("║   Deinion Saver ESP32-S3     ║");
    Serial.println("╚══════════════════════════════╝");

    P1Serial.begin(P1_BAUDRATE, SERIAL_8N1, P1_RX_PIN, -1);
    pinMode(P1_RTS_PIN, OUTPUT);
    digitalWrite(P1_RTS_PIN, HIGH);

    failureLog.begin();   // LittleFS + storingenlog laden

    if (!wifiManager.begin()) {
        startPortal();
    }
}

// ─── Loop ─────────────────────────────────────────────────────────────────────

void loop() {
    // Portal actief: verwerk DNS + HTTP requests, skip P1
    if (portal.isRunning()) {
        portal.update();
        return;
    }

    // WiFi-toestand bijhouden
    wifiManager.update();
    if (wifiManager.needsSetup() && !portal.isRunning()) {
        startPortal();
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

    // Stroomstoringen verwerken — direct naar flash als er nieuwe zijn
    if (!data.failure_log.empty()) {
        std::vector<PowerFailure> parsed;
        for (const auto& e : data.failure_log) {
            PowerFailure pf;
            strncpy(pf.timestamp,  e.timestamp, sizeof(pf.timestamp));
            pf.duration_s = e.duration_s;
            memset(pf.detected_at, 0, sizeof(pf.detected_at));
            parsed.push_back(pf);
        }
        int nieuw = failureLog.update(parsed, data.timestamp);
        if (nieuw > 0) {
            Serial.printf("[Main] %d nieuwe storing(en) opgeslagen in flash\n", nieuw);
        }
    }

    Serial.println("─────────────────────────────────────────");
    Serial.printf("[P1] %s%s\n",
                  data.timestamp, data.crc_valid ? "" : "  ⚠ CRC-fout");
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
    if (data.failures_short > 0 || data.failures_long > 0) {
        Serial.printf("     Storingen kort/lang: %d / %d  (log: %d entries)\n",
                      data.failures_short, data.failures_long,
                      (int)data.failure_log.size());
    }
}
