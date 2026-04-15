# Deinion Saver ESP32 — Documentatie

Deze map bevat uitgebreide documentatie over alle onderdelen van de Deinion Saver ESP32-S3
firmware. Bedoeld zodat je na weken of maanden snel weer op de hoogte bent.

## Inhoud

- [Hardware](hardware.md) — PCB, ESP32-S3, FRAM, antenne, pinnen
- [Firmware modules](firmware.md) — P1Parser, WiFiManager, CaptivePortal, PowerFailureLog, FRAMRingBuffer
- [GitHub infrastructuur](github.md) — deinion-saver-esp32 en deinion-saver-providers
- [Pi Deinion Saver](pi.md) — update_prices.py, crontab, systemd service
- [Snel starten](quickstart.md) — nieuw apparaat compileren, flashen en in gebruik nemen

## Architectuur op één pagina

```
P1 poort (RJ12)
    │  DSMR 5.0, 115200 baud, 1 telegram/sec
    ▼
ESP32-S3-WROOM-1U-N16R8  (16 MB flash, 8 MB PSRAM)
    │
    ├─ P1Parser         → parseert telegrams (verbruik, gas, storingen)
    │
    ├─ FRAMRingBuffer   → slaat laatste ~11 min op in FRAM (niet-vluchtig)
    │     MB85RC64TAPNF, I²C 0x50, 8 KB, 10^13 schrijfcycli
    │
    ├─ PowerFailureLog  → schrijft stroomstoringen naar LittleFS (flash)
    │
    ├─ WiFiManager      → verbindt met WiFi (credentials in NVS)
    │
    └─ CaptivePortal    → WiFi-setup via browser (identiek aan Pi-portal)
          AP: "Deinion Saver Setup"
          Werkt met iOS, Android, Windows
```

## Snelle oriëntatie

| Vraag | Antwoord |
|-------|----------|
| Waar staan de I²C pins voor FRAM? | `main.cpp`: `FRAM_SDA_PIN 8`, `FRAM_SCL_PIN 9` |
| Hoe reset ik de WiFi-instellingen? | `Preferences` namespace "wifi" verwijderen via seriële monitor |
| Hoeveel entries past er in de FRAM? | 681 (≈11 min bij 1/sec, ≈113 min bij 1/10 sec) |
| Waar worden storingen opgeslagen? | LittleFS `/failures.json`, max 500 entries |
| Wat is het FRAM I²C adres? | 0x50 (A0=A1=A2=GND) |
| Welk WiFi TX vermogen? | 17 dBm (EU-compliant met 3 dBi antenne) |
