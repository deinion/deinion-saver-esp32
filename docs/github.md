# GitHub Infrastructuur

## Repositories

### deinion/deinion-saver-esp32
ESP32-S3 firmware broncode.

**Structuur:**
```
firmware/
  include/          # Header bestanden
    P1Parser.h
    WiFiManager.h
    CaptivePortal.h
    PowerFailureLog.h
    FRAMRingBuffer.h
  src/              # Implementatie
    main.cpp
    P1Parser.cpp
    WiFiManager.cpp
    CaptivePortal.cpp
    PowerFailureLog.cpp
    FRAMRingBuffer.cpp
  platformio.ini    # Build configuratie
docs/               # Deze documentatie
```

**Compileren & flashen:**
```bash
# Via Deinionprogrammer (Pi Zero 2W op 192.168.2.8, gebruiker: Deinion)
ssh Deinion@192.168.2.8

cd ~/deinion-saver-esp32/firmware
pio run                    # Compileren
pio run -t upload          # Compileren + flashen
pio device monitor         # Seriële monitor openen
```

**PlatformIO configuratie (`platformio.ini`):**
```ini
[env:esp32-s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_build.mcu = esp32s3
board_build.f_cpu = 240000000L
board_upload.flash_size = 16MB
board_build.partitions = huge_app.csv
board_build.filesystem = littlefs
monitor_speed = 115200
upload_speed = 921600
```

---

### deinion/deinion-saver-providers
Dagelijkse energieprijzen van EnergyZero API.

**Bestanden:**
```
.github/workflows/
  fetch-prices.yml       # GitHub Actions workflow
scripts/
  fetch_prices.py        # Haalt prijzen op bij EnergyZero
data/
  dynamic_prices.json    # Alle historische + toekomstige prijzen
```

**Workflow schema:**
- Dagelijks om 13:15 UTC (kwartier na publicatie door EnergyZero)
- Retry elke 15 minuten t/m 17:00 UTC (voor het geval EnergyZero laat is)
- Sla over als morgen al ≥20 prijspunten aanwezig zijn
- Bij mislukking: e-mail naar deinion.saver@gmail.com

**GitHub Secrets (vereist):**
| Secret | Inhoud |
|--------|--------|
| `GMAIL_APP_PASSWORD` | Gmail app-wachtwoord voor deinion.saver@gmail.com |

**EnergyZero API:**
```
https://api.energyzero.nl/v1/energyprices
  ?fromDate=2026-04-14T00:00:00.000Z
  &tillDate=2026-04-15T00:00:00.000Z
  &interval=9        # 9 = kwartier (elektriciteit)
  &usageType=1       # 1 = verbruik, 2 = teruglevering
  &inclBtw=true      # inclusief BTW

interval=4, usageType=2 → gas (uurprijzen)
```

**dynamic_prices.json formaat:**
```json
{
  "electricity": [
    {"time": "2026-04-14T00:00:00+02:00", "price": 0.1234},
    ...
  ],
  "gas": [
    {"time": "2026-04-14T00:00:00+02:00", "price": 1.2345},
    ...
  ]
}
```
