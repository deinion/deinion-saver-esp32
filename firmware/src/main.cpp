#include <Arduino.h>
#include "P1Parser.h"
#include "WiFiManager.h"
#include "CaptivePortal.h"
#include "PowerFailureLog.h"
#include "FRAMRingBuffer.h"

// ─── Pin configuratie ──────────────────────────────────────────────────────────
#define P1_RX_PIN    8
#define P1_RTS_PIN   5
#define P1_BAUDRATE  115200

// Puls-schakelaar: indrukken activeert captive portal (AP-modus)
// Toekomstige functies mogelijk — overige WiFi-fallback logica blijft ongewijzigd
#define SETUP_BTN_PIN 9

// I²C voor FRAM (MB85RC64TAPNF-G-BDERE1)
#define FRAM_SCL_PIN 47
#define FRAM_SDA_PIN 48

// Maintenance-LEDs
#define LED1_PIN 1
#define LED2_PIN 2

// ─── Globale objecten ──────────────────────────────────────────────────────────
HardwareSerial  P1Serial(1);
P1Parser        parser;
WiFiManager     wifiManager;
CaptivePortal   portal;
PowerFailureLog failureLog;
FRAMRingBuffer  framBuffer;

// ─── Interval auto-detectie ───────────────────────────────────────────────────
static uint32_t _lastTelegramMs = 0;
static uint16_t _detectedInterval = 0;  // 0 = nog niet bepaald

void _updateInterval() {
    uint32_t now = millis();
    if (_lastTelegramMs == 0) {
        _lastTelegramMs = now;
        return;
    }
    uint32_t delta = now - _lastTelegramMs;
    _lastTelegramMs = now;

    // Afronden naar dichtstbijzijnde seconde; negeer onbetrouwbare metingen
    uint16_t sec = (uint16_t)((delta + 500) / 1000);
    if (sec < 1 || sec > 60) return;

    // Sla op als interval veranderd is
    if (sec != _detectedInterval) {
        _detectedInterval = sec;
        framBuffer.setInterval(sec);
        Serial.printf("[Main] Telegram interval gedetecteerd: %d seconde(n)\n", sec);
    }
}

// ─── Schakelaar: lange druk (≥3 sec) detectie ────────────────────────────────
// Geeft true als de knop minimaal 3 seconden ingedrukt blijft.
// Blokkeert maximaal 3 seconden; bij eerder loslaten direct false.
// Let op: wordt zowel in loop() als in readTelegram() gecontroleerd. Als de
// druk precies tijdens een telegram-leescyclus begint, kan 3+3 = 6 sec nodig zijn.
// In de praktijk is dit niet merkbaar omdat de knop zelden precies dan ingedrukt wordt.

bool setupBtnLongPress() {
    if (digitalRead(SETUP_BTN_PIN) != LOW) return false;
    unsigned long pressStart = millis();
    while (digitalRead(SETUP_BTN_PIN) == LOW) {
        if (millis() - pressStart >= 3000) return true;
        delay(10);
    }
    return false;  // Te snel losgelaten
}

// ─── Telegram lezen ───────────────────────────────────────────────────────────
// Controleert ook de schakelaar tijdens wachten, zodat een lange druk nooit
// gemist wordt terwijl de P1-lus actief is.

String readTelegram() {
    String telegram = "";
    bool inTelegram = false;
    unsigned long timeout = millis() + 15000;

    while (millis() < timeout) {
        if (setupBtnLongPress()) return "";  // Lange druk — breek af voor portal

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
// byButton=true → portal is handmatig gestart; 5-minuten timeout actief als
//                 niemand verbindt en er een bekend netwerk aanwezig is.
// byButton=false → automatisch gestart (geen WiFi beschikbaar); geen timeout.

static bool     _portalByButton = false;
static uint32_t _portalStartMs  = 0;

void startPortal(bool byButton = false) {
    _portalByButton = byButton;
    _portalStartMs  = millis();

    Serial.println(byButton
        ? "[Main] Setup-knop: captive portal starten (5 min timeout als al WiFi bekend)"
        : "[Main] WiFi niet beschikbaar — captive portal starten");

    portal.start([](const String& ssid, const String& password) {
        _portalByButton = false;
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

    pinMode(SETUP_BTN_PIN, INPUT_PULLUP);
    pinMode(LED1_PIN, OUTPUT);
    pinMode(LED2_PIN, OUTPUT);
    digitalWrite(LED1_PIN, LOW);
    digitalWrite(LED2_PIN, LOW);

    failureLog.begin();   // LittleFS + storingenlog laden
    framBuffer.begin(FRAM_SDA_PIN, FRAM_SCL_PIN);  // FRAM ring buffer

    // Schakelaar bij opstarten 3 sec ingedrukt → direct portal starten
    if (setupBtnLongPress()) {
        Serial.println("[Main] Setup-knop (lange druk) bij opstarten — portal direct starten");
        startPortal(true);
        return;
    }

    if (!wifiManager.begin()) {
        startPortal();
    }
}

// ─── Loop ─────────────────────────────────────────────────────────────────────

void loop() {
    // Portal actief: verwerk DNS + HTTP requests, skip P1
    if (portal.isRunning()) {
        portal.update();

        // 5-minuten timeout: alleen als portal door knop gestart is,
        // geen client verbonden, en er een bekend netwerk is om naar terug te keren
        if (_portalByButton
                && WiFi.softAPgetStationNum() == 0
                && millis() - _portalStartMs >= 5UL * 60 * 1000) {
            if (wifiManager.hasSavedCredentials()) {
                Serial.println("[Main] 5 min verstreken zonder verbinding — portal stoppen, WiFi herverbinden");
                _portalByButton = false;
                portal.stop();
                delay(200);
                wifiManager.begin();
            } else {
                _portalStartMs = millis();  // Geen credentials: timer resetten, blijven wachten
            }
        }
        return;
    }

    // Schakelaar 3 seconden ingedrukt → portal activeren
    if (setupBtnLongPress()) {
        Serial.println("[Main] Setup-knop (lange druk) — captive portal activeren");
        startPortal(true);
        return;
    }

    // WiFi-toestand bijhouden (bestaande fallback — ongewijzigd)
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

    // FRAM ring buffer: sla huidig verbruik op + update interval
    _updateInterval();
    if (framBuffer.isReady()) {
        FRAMEntry fe;
        fe.unix_time    = 0;  // TODO: NTP-tijd invullen zodra beschikbaar
        fe.power_usage  = data.current_power_usage;
        fe.power_return = data.current_power_return;
        framBuffer.push(fe);
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
