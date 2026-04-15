# Firmware modules

Alle bronbestanden staan in `firmware/src/` en `firmware/include/`.
Het project is gebouwd met PlatformIO (VSCode extensie).

---

## P1Parser

**Bestanden**: `include/P1Parser.h`, `src/P1Parser.cpp`

### Wat het doet
Parseert DSMR 5.0 P1-telegrams naar een `P1Data` struct. Valideert CRC16.

### P1Data struct
```cpp
struct P1Data {
    char     timestamp[20];        // "2026-04-14 21:15:00" (UTC)
    float    elec_import_t1;       // kWh dagtarief
    float    elec_import_t2;       // kWh nachttarief
    float    elec_export_t1;       // kWh teruglever dag
    float    elec_export_t2;       // kWh teruglever nacht
    float    current_power_usage;  // kW huidig verbruik
    float    current_power_return; // kW huidige teruglevering
    float    act_usage_l1/l2/l3;   // kW per fase
    float    act_return_l1/l2/l3;  // kW teruglevering per fase
    float    voltage_l1/l2/l3;     // V spanning per fase
    float    current_l1/l2/l3;     // A stroom per fase
    float    gas_total;            // m³ gasstand
    char     gas_timestamp[20];    // UTC tijdstempel gas
    uint16_t failures_short;       // Totaal korte storingen (nooit gereset)
    uint16_t failures_long;        // Totaal lange storingen (nooit gereset)
    std::vector<FailureEntry> failure_log;  // Laatste storingen met ts + duur
    bool     crc_valid;
};
```

### Tijdstempel conversie
DSMR tijdstempels zijn lokale tijd (Nederland): "260414221500W"
- W = winter = UTC+1 → trek 1 uur af
- S = zomer = UTC+2 → trek 2 uur af
- Dagovergang wordt correct afgehandeld (ook maand-/jaarovergang)

### CRC16
Polynomial: 0xA001 (LSB-first). Berekend over alles van `/` t/m en met `!`.

---

## WiFiManager

**Bestanden**: `include/WiFiManager.h`, `src/WiFiManager.cpp`

### Wat het doet
- Laadt WiFi-credentials uit NVS (Preferences, namespace "wifi")
- Verbindt met WiFi: 3 pogingen, 30 seconden per poging
- Na mislukken: `needsSetup()` geeft `true` → main.cpp start captive portal
- TX vermogen: `WIFI_POWER_17dBm` (EU-compliant)

### NVS sleutels
| Sleutel | Inhoud |
|---------|--------|
| `ssid` | WiFi netwerknaam |
| `password` | WiFi wachtwoord |

### States
```
DISCONNECTED → CONNECTING → CONNECTED
                          ↘ FAILED (na 3× mislukken) → needsSetup() = true
```

### Credential wissen (factory reset WiFi)
Via seriële monitor of extra knop: `Preferences prefs; prefs.begin("wifi"); prefs.clear();`

---

## CaptivePortal

**Bestanden**: `include/CaptivePortal.h`, `src/CaptivePortal.cpp`

### Wat het doet
Zet ESP32 in AP-modus met een captive portal voor WiFi-configuratie.
De portal is identiek aan de Pi Deinion Saver wifi_setup.html (zelfde HTML, CSS, JS).

### Toegangspunt
- SSID: "Deinion Saver Setup"
- IP: 192.168.4.1
- TX vermogen: 17 dBm

### Routes
| Route | Methode | Beschrijving |
|-------|---------|--------------|
| `/` | GET | Portal pagina (HTML in PROGMEM) |
| `/api/wifi/networks` | GET | JSON lijst beschikbare netwerken |
| `/api/wifi/rescan` | POST | Herstart WiFi-scan |
| `/api/wifi/save` | POST | Sla SSID+wachtwoord op, verbind |
| `/api/wifi/retry` | POST | Herverbinden met hetzelfde netwerk |
| `/hotspot-detect.html` | GET | Captive portal detectie (Apple) |
| `/generate_204` | GET | Captive portal detectie (Android) |
| `/ncsi.txt` | GET | Captive portal detectie (Windows) |
| `/connecttest.txt` | GET | Captive portal detectie (Windows) |
| `/redirect` | GET | Captive portal detectie (Windows) |

### Waarom de welcome overlay?
Android vereist een aanraking voordat dropdown-menu's werken in een captive portal.
De overlay ("Ja, instellen") zorgt voor die verplichte aanraking.

### HTML opslag
De HTML staat in PROGMEM (Flash) om het RAM te sparen. De ESP32-S3 heeft 8 MB PSRAM,
maar PROGMEM is alsnog netter voor constante strings.

---

## PowerFailureLog

**Bestanden**: `include/PowerFailureLog.h`, `src/PowerFailureLog.cpp`

### Wat het doet
- Slaat stroomstoringen op in LittleFS (`/failures.json`)
- Max 500 entries, nieuwste bovenaan
- Ontdubbeling op tijdstempel
- Genereert een HTML-overzichtspagina (donker thema)

### PowerFailure struct
```cpp
struct PowerFailure {
    char     timestamp[20];   // Tijdstempel storing (UTC)
    uint32_t duration_s;      // Duur in seconden
    char     detected_at[20]; // Tijdstempel eerste detectie (UTC)
};
```

### JSON formaat (/failures.json)
Compacte sleutels om flash-ruimte te sparen:
```json
[
  {"ts":"2026-04-14 21:15:00","dur":3723,"det":"2026-04-14 22:17:03"},
  ...
]
```

### Duratie weergave
- < 60 sec: "45 sec"
- < 3600 sec: "3 min 12 sec"
- ≥ 3600 sec: "2 uur 15 min"

### Integratie in main.cpp
```cpp
if (!data.failure_log.empty()) {
    std::vector<PowerFailure> parsed;
    for (const auto& e : data.failure_log) {
        PowerFailure pf;
        strncpy(pf.timestamp, e.timestamp, sizeof(pf.timestamp));
        pf.duration_s = e.duration_s;
        memset(pf.detected_at, 0, sizeof(pf.detected_at));
        parsed.push_back(pf);
    }
    int nieuw = failureLog.update(parsed, data.timestamp);
}
```

---

## FRAMRingBuffer

**Bestanden**: `include/FRAMRingBuffer.h`, `src/FRAMRingBuffer.cpp`

### Wat het doet
Slaat P1-meetwaarden op in de FRAM-chip (niet-vluchtig, razendsnel).
Werkt als een ringbuffer: oudste entry wordt overschreven als de buffer vol is.

### FRAM lay-out
```
Adres 0-15  : Header (16 bytes)
Adres 16+   : 681 × 12-byte entries

Header:
  0-1  : Magic 0xDE1A  ("Dein" — herkent geïnitialiseerde FRAM)
  2    : Versie (1)
  3    : Gereserveerd
  4-5  : write_head (uint16, big-endian)
  6-7  : count (uint16, big-endian)
  8-9  : interval in seconden (uint16, big-endian)
  10-15: Gereserveerd
```

### FRAMEntry struct (12 bytes)
```cpp
struct FRAMEntry {
    uint32_t unix_time;    // Seconden sinds 1970-01-01 UTC
    float    power_usage;  // Actueel verbruik (kW)
    float    power_return; // Actuele teruglevering (kW)
};
```

### Capaciteit
| DSMR versie | Interval | Entries in FRAM | Tijdsduur |
|-------------|----------|-----------------|-----------|
| DSMR 5.0 | 1 sec | 681 | ~11 minuten |
| DSMR 4.x | 10 sec | 681 | ~113 minuten |

### Ringbuffer werking
```
write_head wijst altijd naar de VOLGENDE schrijfpositie.
Nieuwste entry: (write_head - 1 + CAPACITY) % CAPACITY
Oudste entry:   (write_head - count + CAPACITY) % CAPACITY

Push:
  entry[write_head] = nieuwe_waarde
  write_head = (write_head + 1) % CAPACITY
  if count < CAPACITY: count++

getLast(n):
  start = (write_head - n + CAPACITY) % CAPACITY
  lees entries start, start+1, ..., start+n-1 (modulo CAPACITY)
```

### I²C details
- Adres: 0x50
- 16-bit geheugenadressering (high byte eerst)
- Schrijven in blokken van max 30 bytes (Wire buffer limiet op ESP32)
- Lezen: Repeated START (geen STOP-conditie tussen adres en data)

### API
```cpp
framBuffer.begin(SDA_PIN, SCL_PIN);     // Geeft false als FRAM niet gevonden
framBuffer.push(entry);                  // Schrijf entry (overschrijft oudste)
framBuffer.getLast(n, outArray);        // Haal laatste n entries op (chronologisch)
framBuffer.getAll(outArray);            // Alle opgeslagen entries
framBuffer.count();                      // Aantal geldige entries
framBuffer.isFull();                     // true als buffer vol
framBuffer.isReady();                    // false als FRAM niet reageert
framBuffer.clear();                      // Wis de buffer
framBuffer.setInterval(sec);            // Sla interval op in header
framBuffer.detectedInterval();          // Lees opgeslagen interval
```

### Interval auto-detectie (in main.cpp)
Elke keer dat een telegram binnenkomt wordt de tijd gemeten. Als het interval
stabiel is, wordt het opgeslagen in de FRAM-header zodat het na een stroomstoring
bekend blijft.

---

## main.cpp — Hoofdprogramma

### Globale objecten
```cpp
HardwareSerial  P1Serial(1);    // UART1 voor P1 poort
P1Parser        parser;
WiFiManager     wifiManager;
CaptivePortal   portal;
PowerFailureLog failureLog;
FRAMRingBuffer  framBuffer;
```

### Setup volgorde
1. Serial monitor starten (115200 baud)
2. P1Serial initialiseren (GPIO4 RX, RTS op GPIO5 HIGH)
3. `failureLog.begin()` — LittleFS koppelen, storingenlog laden
4. `framBuffer.begin(SDA=8, SCL=9)` — FRAM initialiseren
5. `wifiManager.begin()` — WiFi verbinden (of captive portal starten)

### Loop flow
```
Portal actief? → portal.update(), return
WiFi check   → wifiManager.update()
WiFi nodig?  → startPortal(), return
P1 lezen     → readTelegram() (15 sec timeout)
Parsen       → parser.parse()
FRAM push    → framBuffer.push() (huidig verbruik)
Storingen    → failureLog.update() (als nieuwe storingen)
Loggen       → Serial.printf() overzicht
```

### TODO: NTP tijd
`fe.unix_time = 0` is een tijdelijke placeholder. Zodra NTP beschikbaar is:
```cpp
#include <time.h>
fe.unix_time = (uint32_t)time(nullptr);
```
Configureer NTP in setup() na WiFi-verbinding:
```cpp
configTime(0, 0, "pool.ntp.org");  // UTC
```
