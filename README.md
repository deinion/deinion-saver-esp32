# Deinion Saver — ESP32 Firmware

ESP32-S3-WROOM-1U-N16R8 firmware voor de Deinion Saver energiemonitor.

## Hardware
- **MCU**: ESP32-S3-WROOM-1U-N16R8 (dual-core 240MHz, 16MB flash, 8MB PSRAM)
- **Aansluiting**: RJ12 direct op P1-poort van slimme energiemeter (DSMR 5.0)
- **Communicatie**: WiFi → GitHub → Deinion Saver dashboard

## Functionaliteit
- P1-telegram uitlezen via seriële poort (DSMR 5.0 protocol)
- Elektriciteit (T1/T2), gas, voltage, vermogen uitlezen
- Day-ahead prijzen ophalen van GitHub (deinion-saver-providers)
- Dashboard via webinterface (grafisch verbeterd t.o.v. Pi-versie)
- Aparte grafieken voor gas en elektriciteit
- Voltage-lijn instelbaar (standaard uit)

## Repository structuur

