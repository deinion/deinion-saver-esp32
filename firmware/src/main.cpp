#include <Arduino.h>
#include "P1Parser.h"

// ─── Pin configuratie ──────────────────────────────────────────────────────────
// Pas aan op basis van de definitieve PCB layout
#define P1_RX_PIN   4    // Data van de P1-poort (RJ12 pin 5)
#define P1_RTS_PIN  5    // Request To Send — HIGH = data opvragen (RJ12 pin 2)
#define P1_BAUDRATE 115200

// ─── Globale objecten ──────────────────────────────────────────────────────────
HardwareSerial P1Serial(1);   // UART1 voor P1 (UART0 = USB debug)
P1Parser       parser;

// ─── Telegram lezen ───────────────────────────────────────────────────────────
// Leest één compleet P1-telegram van de seriële poort.
// Begint bij '/' en eindigt na de CRC-regel achter '!'.

String readTelegram() {
    String telegram = "";
    bool   inTelegram = false;
    unsigned long timeout = millis() + 15000; // Max 15 seconden wachten

    while (millis() < timeout) {
        if (!P1Serial.available()) {
            delay(1);
            continue;
        }

        char c = (char)P1Serial.read();
        telegram += c;

        if (c == '/') {
            // Start van telegram — reset en begin opnieuw
            inTelegram = true;
            telegram   = "/";
        } else if (inTelegram && c == '\n' && telegram.length() > 6) {
            // Controleer of de vorige regel met '!' begon (einde telegram)
            int excl = telegram.lastIndexOf('!');
            if (excl >= 0 && excl < (int)telegram.length() - 2) {
                // We hebben de CRC-regel ontvangen
                return telegram;
            }
        }
    }

    Serial.println("[P1] Timeout: geen compleet telegram ontvangen");
    return "";
}

// ─── Setup ────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    Serial.println("\n[Deinion Saver ESP32] Start...");

    // P1 seriële poort initialiseren
    P1Serial.begin(P1_BAUDRATE, SERIAL_8N1, P1_RX_PIN, -1);

    // RTS-pin instellen en activeren
    pinMode(P1_RTS_PIN, OUTPUT);
    digitalWrite(P1_RTS_PIN, HIGH);

    Serial.println("[P1] Wachten op eerste telegram...");
}

// ─── Loop ─────────────────────────────────────────────────────────────────────

void loop() {
    String telegram = readTelegram();
    if (telegram.isEmpty()) return;

    P1Data data;
    if (!parser.parse(telegram, data)) {
        Serial.println("[P1] Telegram kon niet geparsed worden");
        return;
    }

    // Debug output — wordt later vervangen door database-opslag + webserver
    Serial.println("─────────────────────────────────────");
    Serial.printf("[P1] Tijdstempel:   %s%s\n",
                  data.timestamp,
                  data.crc_valid ? "" : " ⚠ CRC-fout");
    Serial.printf("[P1] Import T1/T2:  %.3f / %.3f kWh\n",
                  data.elec_import_t1, data.elec_import_t2);
    Serial.printf("[P1] Export T1/T2:  %.3f / %.3f kWh\n",
                  data.elec_export_t1, data.elec_export_t2);
    Serial.printf("[P1] Verbruik/Ret:  %.3f / %.3f kW\n",
                  data.current_power_usage, data.current_power_return);
    Serial.printf("[P1] Voltage L1/2/3: %.1f / %.1f / %.1f V\n",
                  data.voltage_l1, data.voltage_l2, data.voltage_l3);
    Serial.printf("[P1] Gas:           %.3f m³  (%s)\n",
                  data.gas_total, data.gas_timestamp);
}
