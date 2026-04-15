# Snel starten

Gebruik deze pagina als je na een pauze snel weer aan de slag wilt.

## Opzet in één oogopslag

```
Jouw laptop / PC
    │
    ├─ SSH naar Deinionprogrammer: ssh Deinion@192.168.2.8
    │      Raspberry Pi Zero 2W — PlatformIO geïnstalleerd
    │      USB-kabel naar ESP32-S3 voor flashen
    │
    └─ GitHub
           deinion/deinion-saver-esp32     ← firmware broncode
           deinion/deinion-saver-providers ← dagelijkse energieprijzen
```

---

## Firmware aanpassen en flashen

### 1. Verbind met Deinionprogrammer
```bash
ssh Deinion@192.168.2.8
cd ~/deinion-saver-esp32/firmware
```

### 2. Haal laatste wijzigingen op
```bash
git pull
```

### 3. Compileren
```bash
pio run
```

### 4. Flashen (ESP32 via USB aangesloten)
```bash
pio run -t upload
```

### 5. Seriële monitor
```bash
pio device monitor
# Stoppen: Ctrl+C
```

---

## Nieuw bestand toevoegen vanuit deze chat

Als je in Claude Code nieuwe bestanden hebt gemaakt in `/tmp/esp32/`:

```bash
# Kopieer naar Deinionprogrammer
scp /tmp/esp32/include/NieuwBestand.h  Deinion@192.168.2.8:~/deinion-saver-esp32/firmware/include/
scp /tmp/esp32/src/NieuwBestand.cpp    Deinion@192.168.2.8:~/deinion-saver-esp32/firmware/src/

# Commit en push op Deinionprogrammer
ssh Deinion@192.168.2.8 "cd ~/deinion-saver-esp32 && git add -A && git commit -m 'Voeg NieuwBestand toe' && git push"
```

---

## Eerste keer instellen (nieuw apparaat)

### ESP32 in gebruik nemen
1. Flash de firmware (zie boven)
2. Verbind USB/voeding
3. ESP32 start in AP-modus: WiFi-netwerk "Deinion Saver Setup" verschijnt
4. Verbind met dit netwerk op je telefoon/laptop
5. Browser opent automatisch de WiFi-configuratiepagina (captive portal)
6. Tik op "Ja, instellen" (Android vereist dit)
7. Kies je WiFi-netwerk, voer wachtwoord in
8. ESP32 verbindt en begint met uitlezen

### WiFi opnieuw instellen
Als je WiFi wilt wijzigen, wis dan de NVS-opslag via seriële monitor:
```cpp
// Tijdelijk in setup() toevoegen, één keer flashen, dan weer verwijderen:
Preferences prefs;
prefs.begin("wifi");
prefs.clear();
prefs.end();
```

---

## Veelvoorkomende problemen

### FRAM niet gevonden
```
[FRAM] Niet gevonden op I²C adres 0x50
```
- Controleer SDA/SCL pinnen in `main.cpp` (`FRAM_SDA_PIN`, `FRAM_SCL_PIN`)
- Controleer soldeerverbindingen en pull-up weerstanden (4.7kΩ naar 3.3V)
- Controleer I²C adres (A0/A1/A2 pins op FRAM chip → alle GND = 0x50)

### P1 telegram timeout
```
[P1] Timeout: geen compleet telegram ontvangen
```
- Controleer RTS pin: moet HIGH zijn (GPIO5)
- Controleer RX pin (GPIO4) en signaalinverter indien aanwezig
- DSMR 4.x stuurt 1 telegram per 10 sec → timeout kan dan aanpassen naar 20 sec

### WiFi verbindt niet na setup
- Het opgeslagen wachtwoord kan fout zijn
- Wis NVS (zie boven) en probeer opnieuw

### Captive portal opent niet
- iOS/Android: sluit andere browsers en wacht ~10 sec
- Windows: klik op "aanmelden" in systeemvak
- Handmatig: open browser en ga naar `192.168.4.1`

---

## Belangrijke constanten

| Constante | Waarde | Bestand |
|-----------|--------|---------|
| P1 RX pin | GPIO 8 | main.cpp |
| P1 RTS pin | GPIO 5 | main.cpp |
| Setup schakelaar | GPIO 9 | main.cpp |
| FRAM SCL | GPIO 47 | main.cpp |
| FRAM SDA | GPIO 48 | main.cpp |
| LED1 (maintenance) | GPIO 1 | main.cpp |
| LED2 (maintenance) | GPIO 2 | main.cpp |
| FRAM I²C adres | 0x50 | FRAMRingBuffer.h |
| FRAM capaciteit | 681 entries | FRAMRingBuffer.h |
| FRAM magic | 0xDE1A | FRAMRingBuffer.h |
| WiFi TX vermogen | 17 dBm | WiFiManager.cpp / CaptivePortal.cpp |
| AP SSID | "Deinion Saver Setup" | CaptivePortal.cpp |
| NVS namespace WiFi | "wifi" | WiFiManager.cpp |
| LittleFS storingen | /failures.json | PowerFailureLog.cpp |
| Max storingen opslag | 500 entries | PowerFailureLog.h |

---

## Nog niet geïmplementeerd (TODO)

- **NTP tijd**: `fe.unix_time = 0` in main.cpp is een placeholder. Voeg toe na WiFi-verbinding:
  ```cpp
  configTime(0, 0, "pool.ntp.org");
  // In loop, na WiFi connected:
  fe.unix_time = (uint32_t)time(nullptr);
  ```
- **Web dashboard**: nog geen HTTP server voor verbruiksgrafiek
- **FRAM → flash flush**: als FRAM bijna vol is, comprimeer oudste minuut naar LittleFS
- **Stopwatch/kostenmeter**: snapshot meterstand bij start/stop, bereken delta × prijs
- **`/failures` route**: webpagina met stroomstoringen (HTML bouwer `buildHtmlPage()` bestaat al)
