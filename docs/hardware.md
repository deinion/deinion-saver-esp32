# Hardware

## ESP32-S3-WROOM-1U-N16R8

| Eigenschap | Waarde |
|------------|--------|
| Module | ESP32-S3-WROOM-1U-N16R8 |
| CPU | Xtensa LX7 dual-core, 240 MHz |
| Flash | 16 MB (Quad SPI) |
| PSRAM | 8 MB (Octal SPI) |
| WiFi | 802.11b/g/n, 2.4 GHz |
| Bluetooth | BT 5.0 + BLE |
| Antenne | Externe antenne via U.FL connector (de "U" in de naam) |

## Antenne

- **Model**: 3 dBi externe WiFi/BT antenne
- **Connector**: U.FL (interne pigtail aanwezig op module)
- **Kabelverlies**: al verrekend in de 3 dBi
- **Max TX vermogen**: 17 dBm WiFi (EU-richtlijn: max 20 dBm EIRP met 3 dBi = 20 - 3 = 17 dBm)
- **Bluetooth TX**: standaard 9 dBm (blijft ruim onder EU-limiet)
- **Zowel WiFi als Bluetooth** gebruiken dezelfde antenne (ingebouwde RF switch in module)

## FRAM — MB85RC64TAPNF-G-BDERE1

| Eigenschap | Waarde |
|------------|--------|
| Type | Ferroelectric RAM (FRAM) |
| Capaciteit | 64 Kbit = 8.192 bytes |
| Interface | I²C, max 1 MHz |
| I²C adres | 0x50 (A0=A1=A2=GND; aanpasbaar via hardwarepins) |
| Schrijfcycli | 10^13 (vrijwel onslijtbaar) |
| Data retentie | 10 jaar zonder spanning |
| Pakket | SOP-8 |
| Onderscheidend | Geen schrijfvertraging (EEPROM heeft 5ms, FRAM niet) |

**Waarom FRAM en niet flash/EEPROM?**
- P1 meter stuurt 1 telegram per seconde → 86.400 schrijfoperaties per dag
- Flash/EEPROM is typisch goed voor 100.000 cycli → na 1 dag al 86% verbruikt
- FRAM heeft 10^13 cycli → duurzaam voor vele jaren

## P1 Poort

- **Connector**: RJ12 (6P6C)
- **Baudrate**: 115200 baud (DSMR 5.0)
- **Signaal**: UART, invertert (logisch 0 = hoog, 1 = laag) — check of je omvormer nodig hebt
- **RTS**: actief hoog = meter stuurt telegrams
- **RX pin**: GPIO 4
- **RTS pin**: GPIO 5

**RJ12 pin-out (P1 standaard):**
```
Pin 1: +5V (max 250 mA vanuit meter)
Pin 2: RTS (Request To Send — zet hoog om telegrams te activeren)
Pin 3: Data GND
Pin 4: NC (niet aangesloten)
Pin 5: DATA (TXD van meter, RXD voor ESP32)
Pin 6: Power GND
```

## I²C Pinnen (FRAM)

In `main.cpp` gedefinieerd als:
```cpp
#define FRAM_SDA_PIN 8
#define FRAM_SCL_PIN 9
```
Pas aan als je PCB andere pinnen gebruikt.

## Stroomverbruik (schatting)

| Toestand | Stroom @ 3.3V | Vermogen |
|----------|---------------|---------|
| Actief WiFi TX (17 dBm) | ~220 mA | ~0.73 W |
| Actief, geen TX | ~80 mA | ~0.26 W |
| Gemiddeld (1 TX per seconde) | ~100 mA | ~0.33 W |
| FRAM schrijven (I²C) | +3 mA | verwaarloosbaar |

**Max gemiddeld vermogen: ≈ 1.1 W** (veilig voor standaard USB-voeding 5V/1A via 3.3V LDO)
