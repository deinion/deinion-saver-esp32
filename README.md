# Deinion Saver — ESP32 Firmware

ESP32-S3-WROOM-1U-N16R8 firmware voor de Deinion Saver energiemonitor.

## Hardware

- **MCU**: ESP32-S3-WROOM-1U-N16R8 (dual-core 240MHz, 16MB flash, 8MB PSRAM)
- **Aansluiting**: RJ12 direct op P1-poort van slimme energiemeter (DSMR 5.0)
- **Communicatie**: WiFi naar Deinion cloud (GitHub)

## Functionaliteit

- P1-telegram uitlezen via seriële poort (DSMR 5.0 protocol)
- Elektriciteit (T1/T2 import/export), gas, voltage en vermogen uitlezen
- Day-ahead prijzen ophalen van GitHub (deinion-saver-providers)
- Dashboard via webinterface
- Aparte grafieken voor gas en elektriciteit
- Voltage-lijn instelbaar (standaard uit)
- Instellingenpagina

## Mappenstructuur

```
firmware/
  src/          Hoofdcode (.cpp)
  include/      Header bestanden (.h)
  lib/          Bibliotheken
docs/           Technische documentatie
```

## Gerelateerde repositories

- [deinion-saver-providers](https://github.com/deinion/deinion-saver-providers) — Dagelijkse energieprijzen en provider-data
