#pragma once

#include <Arduino.h>
#include <Wire.h>

// ─── MB85RC64TAPNF-G-BDERE1 specificaties ─────────────────────────────────────
// Capaciteit : 64Kbit = 8.192 bytes
// Interface  : I²C, max 1 MHz
// Adres      : 0x50 (A0=A1=A2=GND; aanpasbaar via hardwarepins)
// Endurance  : 10^13 schrijfcycli (vrijwel onslijtbaar)
// Retentie   : 10 jaar zonder stroom

#define FRAM_I2C_ADDR     0x50
#define FRAM_SIZE_BYTES   8192

// ─── Ring buffer layout ────────────────────────────────────────────────────────
// Bytes 0-15   : Header (magic, versie, write_head, count, interval)
// Bytes 16+    : Entries (elk 12 bytes)
//
// Maximale capaciteit:
//   (8192 - 16) / 12 = 681 entries
//
// Bij DSMR 5.0 (1/sec)   → ~11 minuten scherpe grafiek
// Bij DSMR 4.x (1/10sec) → ~113 minuten scherpe grafiek

#define FRAM_HEADER_SIZE  16
#define FRAM_ENTRY_SIZE   12
#define FRAM_CAPACITY     ((FRAM_SIZE_BYTES - FRAM_HEADER_SIZE) / FRAM_ENTRY_SIZE)  // 681

#define FRAM_MAGIC        0xDE1A   // "Dein" — herkent geïnitialiseerde FRAM

// ─── Entry structuur (12 bytes) ────────────────────────────────────────────────
// Alleen de meest waardevolle realtime velden voor de scherpe grafiek.
// Meterstanden en gas worden per minuut naar flash geschreven.

struct FRAMEntry {
    uint32_t unix_time;       // 4 bytes — seconden sinds 1970-01-01 UTC
    float    power_usage;     // 4 bytes — actueel verbruik (kW)
    float    power_return;    // 4 bytes — actuele teruglevering (kW)
};
static_assert(sizeof(FRAMEntry) == FRAM_ENTRY_SIZE, "FRAMEntry grootte klopt niet");

// ─── Klasse ────────────────────────────────────────────────────────────────────

class FRAMRingBuffer {
public:
    // sda/scl: I²C pinnen (aanpassen aan PCB-layout)
    // Geeft false terug als FRAM niet gevonden wordt op de bus
    bool begin(uint8_t sda, uint8_t scl, uint32_t freq = 400000);

    // Schrijf één entry naar FRAM (overschrijft oudste als buffer vol is)
    void push(const FRAMEntry& entry);

    // Haal de laatste N entries op, chronologisch (oudste eerst)
    // Geeft het werkelijke aantal terug (kan minder zijn dan n)
    int getLast(int n, FRAMEntry* out);

    // Haal alle entries op (max FRAM_CAPACITY)
    int getAll(FRAMEntry* out);

    // Huidige vulling
    int  count()    const { return _count; }
    bool isFull()   const { return _count >= FRAM_CAPACITY; }
    bool isReady()  const { return _ready; }

    // Reset ring buffer (wist header in FRAM)
    void clear();

    // Auto-gedetecteerd interval tussen telegrams (seconden, 0 = onbekend)
    uint16_t detectedInterval() const { return _interval; }
    void     setInterval(uint16_t sec);

private:
    bool     _readHeader();
    void     _writeHeader();
    void     _writeEntry(uint16_t index, const FRAMEntry& entry);
    bool     _readEntry(uint16_t index, FRAMEntry& out);

    uint16_t _entryOffset(uint16_t index) const {
        return FRAM_HEADER_SIZE + (index * FRAM_ENTRY_SIZE);
    }

    // FRAM lezen/schrijven (I²C)
    void     _write(uint16_t addr, const uint8_t* data, size_t len);
    void     _read(uint16_t addr, uint8_t* data, size_t len);

    uint8_t  _i2cAddr;
    bool     _ready;
    uint16_t _writeHead;   // Index van volgende schrijfpositie
    uint16_t _count;       // Aantal geldige entries (max FRAM_CAPACITY)
    uint16_t _interval;    // Telegrams per seconde (auto-detect)
};
