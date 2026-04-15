#pragma once

#include <Arduino.h>
#include <vector>

// ─── Data structuur ────────────────────────────────────────────────────────────
// Bevat alle waarden die uit één P1-telegram worden gelezen.
// Velden die niet aanwezig zijn in het telegram blijven op 0.0 of -1.

struct P1Data {
    // Tijdstempel (UTC, afgeleid van P1-tijd + W/S indicator)
    // Formaat: "YYYY-MM-DD HH:MM:SS"
    char timestamp[20];

    // Elektriciteit — tellers (kWh)
    float elec_import_t1;   // 1-0:1.8.1  Nachtverbruik
    float elec_import_t2;   // 1-0:1.8.2  Dagverbruik
    float elec_export_t1;   // 1-0:2.8.1  Nachtteruglevering
    float elec_export_t2;   // 1-0:2.8.2  Dagteruglevering

    // Elektriciteit — actueel vermogen (kW)
    float current_power_usage;   // 1-0:1.7.0  Totaal verbruik
    float current_power_return;  // 1-0:2.7.0  Totaal teruglevering

    // Per fase — vermogen (kW)
    float act_usage_l1;    // 1-0:21.7.0
    float act_usage_l2;    // 1-0:41.7.0
    float act_usage_l3;    // 1-0:61.7.0
    float act_return_l1;   // 1-0:22.7.0
    float act_return_l2;   // 1-0:42.7.0
    float act_return_l3;   // 1-0:62.7.0

    // Per fase — spanning (V) en stroom (A)
    float voltage_l1;      // 1-0:32.7.0
    float voltage_l2;      // 1-0:52.7.0
    float voltage_l3;      // 1-0:72.7.0
    float current_l1;      // 1-0:31.7.0
    float current_l2;      // 1-0:51.7.0
    float current_l3;      // 1-0:71.7.0

    // Gas
    float gas_total;         // 0-n:24.2.1  Gasmeter teller (m³)
    char  gas_timestamp[20]; // Tijdstempel gasmeting (UTC)

    // Stroomstoringen (uit meter-log, OBIS 1-0:99.97.0)
    uint16_t failures_short;    // 0-0:96.7.21 — totaal korte storingen (teller)
    uint16_t failures_long;     // 0-0:96.7.9  — totaal lange storingen (teller)
    // Gedetailleerde log (max 10 entries per telegram, DSMR 5.0 spec)
    struct FailureEntry {
        char     timestamp[20]; // UTC
        uint32_t duration_s;
    };
    std::vector<FailureEntry> failure_log;

    // Validatie
    bool  crc_valid;         // CRC16-check geslaagd
};

// ─── Parser klasse ─────────────────────────────────────────────────────────────

class P1Parser {
public:
    // Verwerk een compleet telegram (vector van regels, of één grote string).
    // Geeft true terug als het telegram geldig en volledig is.
    bool parse(const String& telegram, P1Data& out);

private:
    // Hulpfuncties
    float  extractFloat(const String& text, const char* obisCode, const char* unit);
    bool   extractTimestamp(const String& text, const char* obisPattern,
                            char* outBuf, size_t bufLen);
    bool   validateCRC(const String& telegram);
    uint16_t crc16(const char* data, size_t len);

    // Zet DSMR-tijdstempel (YYMMDDHHmmSS + W/S) om naar UTC-string
    bool dsmrToUtc(const char* dsmrTime, char dst, char* outBuf, size_t bufLen);
};
