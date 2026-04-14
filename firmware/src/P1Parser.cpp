#include "P1Parser.h"
#include <string.h>
#include <stdio.h>

// ─── Publieke methode: parse() ─────────────────────────────────────────────────

bool P1Parser::parse(const String& telegram, P1Data& out) {
    memset(&out, 0, sizeof(P1Data));

    // Telegram moet beginnen met '/' en ergens een '!' bevatten
    if (telegram.indexOf('/') < 0 || telegram.indexOf('!') < 0) {
        return false;
    }

    // CRC validatie (DSMR 5.0 vereiste)
    out.crc_valid = validateCRC(telegram);
    if (!out.crc_valid) {
        Serial.println("[P1] Waarschuwing: CRC-fout in telegram");
        // We parsen toch door — nuttig voor debugging
    }

    // ── Elektriciteit: tellers ────────────────────────────────────────────────
    out.elec_import_t1 = extractFloat(telegram, "1-0:1.8.1", "kWh");
    out.elec_import_t2 = extractFloat(telegram, "1-0:1.8.2", "kWh");
    out.elec_export_t1 = extractFloat(telegram, "1-0:2.8.1", "kWh");
    out.elec_export_t2 = extractFloat(telegram, "1-0:2.8.2", "kWh");

    // ── Elektriciteit: actueel vermogen ───────────────────────────────────────
    out.current_power_usage  = extractFloat(telegram, "1-0:1.7.0", "kW");
    out.current_power_return = extractFloat(telegram, "1-0:2.7.0", "kW");

    // ── Per fase: vermogen ────────────────────────────────────────────────────
    out.act_usage_l1  = extractFloat(telegram, "1-0:21.7.0", "kW");
    out.act_usage_l2  = extractFloat(telegram, "1-0:41.7.0", "kW");
    out.act_usage_l3  = extractFloat(telegram, "1-0:61.7.0", "kW");
    out.act_return_l1 = extractFloat(telegram, "1-0:22.7.0", "kW");
    out.act_return_l2 = extractFloat(telegram, "1-0:42.7.0", "kW");
    out.act_return_l3 = extractFloat(telegram, "1-0:62.7.0", "kW");

    // ── Per fase: spanning en stroom ──────────────────────────────────────────
    out.voltage_l1  = extractFloat(telegram, "1-0:32.7.0", "V");
    out.voltage_l2  = extractFloat(telegram, "1-0:52.7.0", "V");
    out.voltage_l3  = extractFloat(telegram, "1-0:72.7.0", "V");
    out.current_l1  = extractFloat(telegram, "1-0:31.7.0", "A");
    out.current_l2  = extractFloat(telegram, "1-0:51.7.0", "A");
    out.current_l3  = extractFloat(telegram, "1-0:71.7.0", "A");

    // ── Gas ───────────────────────────────────────────────────────────────────
    // Gasmeterwaarde staat na het tweede haakjespaar op de 0-n:24.2.1 regel
    // Formaat: 0-1:24.2.1(timestamp)(waarde*m3)
    {
        int gasIdx = telegram.indexOf(":24.2.1(");
        if (gasIdx >= 0) {
            // Sla het eerste haakjespaar (timestamp) over
            int firstClose = telegram.indexOf(')', gasIdx);
            if (firstClose >= 0) {
                int secondOpen  = telegram.indexOf('(', firstClose);
                int secondClose = telegram.indexOf(')', secondOpen);
                if (secondOpen >= 0 && secondClose > secondOpen) {
                    String val = telegram.substring(secondOpen + 1, secondClose);
                    int starIdx = val.indexOf('*');
                    if (starIdx >= 0) val = val.substring(0, starIdx);
                    out.gas_total = val.toFloat();
                }
            }

            // Gas tijdstempel: eerste haakjespaar bevat YYMMDDHHmmSSW/S
            int tsOpen  = telegram.indexOf('(', gasIdx + 7);
            int tsClose = telegram.indexOf(')', tsOpen);
            if (tsOpen >= 0 && tsClose > tsOpen) {
                String tsRaw = telegram.substring(tsOpen + 1, tsClose);
                if (tsRaw.length() >= 13) {
                    char dst = tsRaw[12]; // W of S
                    dsmrToUtc(tsRaw.c_str(), dst, out.gas_timestamp, sizeof(out.gas_timestamp));
                }
            }
        }
    }

    // ── Tijdstempel (OBIS 0-0:1.0.0) ─────────────────────────────────────────
    {
        int tsIdx = telegram.indexOf("0-0:1.0.0(");
        if (tsIdx >= 0) {
            int tsOpen  = telegram.indexOf('(', tsIdx);
            int tsClose = telegram.indexOf(')', tsOpen);
            if (tsOpen >= 0 && tsClose > tsOpen) {
                String tsRaw = telegram.substring(tsOpen + 1, tsClose);
                if (tsRaw.length() >= 13) {
                    char dst = tsRaw[12];
                    dsmrToUtc(tsRaw.c_str(), dst, out.timestamp, sizeof(out.timestamp));
                }
            }
        }
    }

    return true;
}

// ─── Hulpfunctie: extractFloat() ──────────────────────────────────────────────
// Zoekt een OBIS-code in de tekst en leest de float waarde eruit.
// Voorbeeld: "1-0:1.8.1(001234.567*kWh)" → 1234.567

float P1Parser::extractFloat(const String& text, const char* obisCode, const char* unit) {
    int idx = text.indexOf(obisCode);
    if (idx < 0) return 0.0f;

    int open  = text.indexOf('(', idx);
    int close = text.indexOf(')', open);
    if (open < 0 || close <= open) return 0.0f;

    String val = text.substring(open + 1, close);
    int starIdx = val.indexOf('*');
    if (starIdx >= 0) val = val.substring(0, starIdx);

    return val.toFloat();
}

// ─── Hulpfunctie: dsmrToUtc() ─────────────────────────────────────────────────
// Zet DSMR-tijdstempel om naar UTC.
// Invoer: "260414221500" + 'W' (winter=UTC+1) of 'S' (zomer=UTC+2)
// Uitvoer: "2026-04-14 21:15:00"

bool P1Parser::dsmrToUtc(const char* dsmrTime, char dst, char* outBuf, size_t bufLen) {
    if (strlen(dsmrTime) < 12) return false;

    int year   = 2000 + (dsmrTime[0]-'0')*10 + (dsmrTime[1]-'0');
    int month  = (dsmrTime[2]-'0')*10 + (dsmrTime[3]-'0');
    int day    = (dsmrTime[4]-'0')*10 + (dsmrTime[5]-'0');
    int hour   = (dsmrTime[6]-'0')*10 + (dsmrTime[7]-'0');
    int minute = (dsmrTime[8]-'0')*10 + (dsmrTime[9]-'0');
    int second = (dsmrTime[10]-'0')*10 + (dsmrTime[11]-'0');

    // Corrigeer naar UTC (W=UTC+1, S=UTC+2)
    int offsetHours = (dst == 'W') ? 1 : 2;
    hour -= offsetHours;

    // Dagwissel afhandelen
    if (hour < 0) {
        hour += 24;
        day  -= 1;
        if (day < 1) {
            month -= 1;
            if (month < 1) { month = 12; year -= 1; }
            // Dagen per maand (geen schrikkeljaar-edge-case voor P1 tijdstempels)
            const int daysInMonth[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
            int maxDay = daysInMonth[month];
            if (month == 2 && (year % 4 == 0)) maxDay = 29;
            day = maxDay;
        }
    }

    snprintf(outBuf, bufLen, "%04d-%02d-%02d %02d:%02d:%02d",
             year, month, day, hour, minute, second);
    return true;
}

// ─── CRC16 validatie ──────────────────────────────────────────────────────────
// DSMR 5.0 gebruikt CRC16 over alles van '/' t/m en met '!'.
// Het CRC staat als 4 hex-tekens direct na de '!'.

uint16_t P1Parser::crc16(const char* data, size_t len) {
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint8_t)data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

bool P1Parser::validateCRC(const String& telegram) {
    int exclamIdx = telegram.lastIndexOf('!');
    if (exclamIdx < 0) return false;

    // CRC staat na de '!' als 4 hex-tekens
    if (telegram.length() < (size_t)(exclamIdx + 5)) return false;
    String crcStr = telegram.substring(exclamIdx + 1, exclamIdx + 5);
    crcStr.trim();
    if (crcStr.length() < 4) return false;

    uint16_t receivedCRC = (uint16_t)strtol(crcStr.c_str(), nullptr, 16);

    // Bereken CRC over alles van '/' t/m en met '!'
    int startIdx = telegram.indexOf('/');
    if (startIdx < 0) return false;
    String dataToCheck = telegram.substring(startIdx, exclamIdx + 1);
    uint16_t calculatedCRC = crc16(dataToCheck.c_str(), dataToCheck.length());

    return (calculatedCRC == receivedCRC);
}
