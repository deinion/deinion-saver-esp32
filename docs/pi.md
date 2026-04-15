# Pi Deinion Saver

De Pi Zero 2W is de productie-hardware van de originele Deinion Saver.
De ESP32 is een nieuwe, goedkopere hardwareversie.

## Installatie locatie

```
/srv/energymind/slots/slot_a/scripts/
  update_prices.py      # Prijzen ophalen van GitHub
```

## update_prices.py

### Wat het doet
1. Controleert of morgen al ≥20 prijspunten aanwezig zijn in lokaal bestand
2. Zo ja: overslaan (al up-to-date)
3. Zo nee: downloaden van GitHub (deinion/deinion-saver-providers/data/dynamic_prices.json)
4. Bij eerste installatie: filtert prijzen tot DB-startdatum - 1 dag

### Nieuwe installatie filtering
Bij een nieuwe Pi Deinion Saver bevat de database nog weinig data.
Het script detecteert de vroegste meting in de database:
```python
MIN(timestamp) FROM readings
```
Prijzen van vóór die datum worden weggefilterd (nutteloos, ook groot bestand).
Er wordt 1 dag extra buffer gehouden (startdatum - 1 dag) voor grafiek-weergave.

### Exit codes
| Code | Betekenis |
|------|-----------|
| 0 | Succes OF GitHub had de prijzen nog niet klaar |
| 1 | Echte fout (netwerk, bestand kapot, etc.) |

"GitHub nog niet klaar" geeft bewust exit 0 — de cron retries het vanzelf.

## Crontab (prijzen ophalen)

```cron
# Haal dynamische prijzen op — 14:15 UTC, retry elke 15 min t/m 17:00 UTC
15 14 * * * /srv/energymind/venv/bin/python /srv/energymind/slots/slot_a/scripts/update_prices.py >> /var/log/energymind/update_prices.log 2>&1
30 14 * * * /srv/energymind/venv/bin/python /srv/energymind/slots/slot_a/scripts/update_prices.py >> /var/log/energymind/update_prices.log 2>&1
45 14 * * * /srv/energymind/venv/bin/python /srv/energymind/slots/slot_a/scripts/update_prices.py >> /var/log/energymind/update_prices.log 2>&1
0  15 * * * /srv/energymind/venv/bin/python /srv/energymind/slots/slot_a/scripts/update_prices.py >> /var/log/energymind/update_prices.log 2>&1
15 15 * * * /srv/energymind/venv/bin/python /srv/energymind/slots/slot_a/scripts/update_prices.py >> /var/log/energymind/update_prices.log 2>&1
30 15 * * * /srv/energymind/venv/bin/python /srv/energymind/slots/slot_a/scripts/update_prices.py >> /var/log/energymind/update_prices.log 2>&1
45 15 * * * /srv/energymind/venv/bin/python /srv/energymind/slots/slot_a/scripts/update_prices.py >> /var/log/energymind/update_prices.log 2>&1
0  16 * * * /srv/energymind/venv/bin/python /srv/energymind/slots/slot_a/scripts/update_prices.py >> /var/log/energymind/update_prices.log 2>&1
15 16 * * * /srv/energymind/venv/bin/python /srv/energymind/slots/slot_a/scripts/update_prices.py >> /var/log/energymind/update_prices.log 2>&1
30 16 * * * /srv/energymind/venv/bin/python /srv/energymind/slots/slot_a/scripts/update_prices.py >> /var/log/energymind/update_prices.log 2>&1
45 16 * * * /srv/energymind/venv/bin/python /srv/energymind/slots/slot_a/scripts/update_prices.py >> /var/log/energymind/update_prices.log 2>&1
0  17 * * * /srv/energymind/venv/bin/python /srv/energymind/slots/slot_a/scripts/update_prices.py >> /var/log/energymind/update_prices.log 2>&1
```

**Waarom 14:15 UTC?**
EnergyZero publiceert morgende prijzen om ±13:00 UTC. GitHub Actions loopt om 13:15 UTC
en haalt ze op. De Pi wacht tot 14:15 UTC om GitHub de kans te geven te committen.

## systemd service (bij opstarten)

**Bestand**: `/etc/systemd/system/energymind-update-prices.service`

```ini
[Unit]
Description=Energymind prijzen ophalen bij opstarten
After=network-online.target
Wants=network-online.target

[Service]
Type=oneshot
ExecStart=/srv/energymind/venv/bin/python /srv/energymind/slots/slot_a/scripts/update_prices.py
User=energymind
StandardOutput=journal
StandardError=journal
SuccessExitStatus=0

[Install]
WantedBy=multi-user.target
```

**Doel**: Als de Pi na een stroomstoring opstart en er zijn ontbrekende prijzen,
worden deze direct opgehaald zonder te wachten op de cron.

**Logboek bekijken:**
```bash
journalctl -u energymind-update-prices.service
```

## Deinionprogrammer (ontwikkelcomputer)

De Pi Zero 2W op IP 192.168.2.8 wordt gebruikt als compiler voor de ESP32 firmware.

```bash
ssh Deinion@192.168.2.8
```

PlatformIO is geïnstalleerd en kan de ESP32 direct flashen via USB.
