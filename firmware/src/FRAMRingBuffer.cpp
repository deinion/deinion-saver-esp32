#include "FRAMRingBuffer.h"

// ─── Header layout in FRAM ─────────────────────────────────────────────────────
// Offset  0-1 : Magic (0xDE1A)
// Offset  2   : Versie (1)
// Offset  3   : Gereserveerd
// Offset  4-5 : write_head (uint16)
// Offset  6-7 : count (uint16)
// Offset  8-9 : interval in seconden (uint16)
// Offset 10-15: Gereserveerd

#define HDR_MAGIC    0
#define HDR_VERSION  2
#define HDR_HEAD     4
#define HDR_COUNT    6
#define HDR_INTERVAL 8

// ─── Initialisatie ─────────────────────────────────────────────────────────────

bool FRAMRingBuffer::begin(uint8_t sda, uint8_t scl, uint32_t freq) {
    _ready     = false;
    _writeHead = 0;
    _count     = 0;
    _interval  = 0;
    _i2cAddr   = FRAM_I2C_ADDR;

    Wire.begin(sda, scl);
    Wire.setClock(freq);

    // Controleer of FRAM reageert op de bus
    Wire.beginTransmission(_i2cAddr);
    if (Wire.endTransmission() != 0) {
        Serial.printf("[FRAM] Niet gevonden op I²C adres 0x%02X\n", _i2cAddr);
        return false;
    }

    Serial.printf("[FRAM] Gevonden op 0x%02X. ", _i2cAddr);

    // Lees header — is de FRAM al geïnitialiseerd?
    if (!_readHeader()) {
        Serial.println("Nieuw — buffer initialiseren...");
        clear();
    } else {
        Serial.printf("Buffer bevat %d entries (max %d).\n", _count, FRAM_CAPACITY);
    }

    _ready = true;
    return true;
}

// ─── Push: schrijf één entry ───────────────────────────────────────────────────

void FRAMRingBuffer::push(const FRAMEntry& entry) {
    if (!_ready) return;

    _writeEntry(_writeHead, entry);

    _writeHead = (_writeHead + 1) % FRAM_CAPACITY;
    if (_count < FRAM_CAPACITY) _count++;

    _writeHeader();
}

// ─── Ophalen: laatste N entries (chronologisch) ───────────────────────────────

int FRAMRingBuffer::getLast(int n, FRAMEntry* out) {
    if (!_ready || _count == 0) return 0;
    if (n > _count) n = _count;

    // Oudste van de gevraagde N entries:
    // write_head wijst naar de VOLGENDE schrijfpositie,
    // dus de nieuwste entry staat op (write_head - 1 + CAPACITY) % CAPACITY
    int startIdx = ((int)_writeHead - n + (int)FRAM_CAPACITY) % (int)FRAM_CAPACITY;

    for (int i = 0; i < n; i++) {
        int idx = (startIdx + i) % FRAM_CAPACITY;
        _readEntry((uint16_t)idx, out[i]);
    }
    return n;
}

int FRAMRingBuffer::getAll(FRAMEntry* out) {
    return getLast(_count, out);
}

// ─── Clear ────────────────────────────────────────────────────────────────────

void FRAMRingBuffer::clear() {
    _writeHead = 0;
    _count     = 0;
    _interval  = 0;
    _writeHeader();
    Serial.println("[FRAM] Buffer gewist.");
}

void FRAMRingBuffer::setInterval(uint16_t sec) {
    if (_interval == sec) return;
    _interval = sec;
    _writeHeader();
    Serial.printf("[FRAM] Interval ingesteld op %d seconden.\n", sec);
}

// ─── Header lezen / schrijven ──────────────────────────────────────────────────

bool FRAMRingBuffer::_readHeader() {
    uint8_t hdr[FRAM_HEADER_SIZE];
    _read(0, hdr, FRAM_HEADER_SIZE);

    uint16_t magic = ((uint16_t)hdr[HDR_MAGIC] << 8) | hdr[HDR_MAGIC + 1];
    if (magic != FRAM_MAGIC) return false;

    _writeHead = ((uint16_t)hdr[HDR_HEAD]     << 8) | hdr[HDR_HEAD + 1];
    _count     = ((uint16_t)hdr[HDR_COUNT]    << 8) | hdr[HDR_COUNT + 1];
    _interval  = ((uint16_t)hdr[HDR_INTERVAL] << 8) | hdr[HDR_INTERVAL + 1];

    // Sanity check
    if (_writeHead >= FRAM_CAPACITY) _writeHead = 0;
    if (_count     >  FRAM_CAPACITY) _count     = 0;

    return true;
}

void FRAMRingBuffer::_writeHeader() {
    uint8_t hdr[FRAM_HEADER_SIZE] = {0};

    hdr[HDR_MAGIC]        = (FRAM_MAGIC >> 8) & 0xFF;
    hdr[HDR_MAGIC + 1]    =  FRAM_MAGIC       & 0xFF;
    hdr[HDR_VERSION]      = 1;
    hdr[HDR_HEAD]         = (_writeHead >> 8) & 0xFF;
    hdr[HDR_HEAD + 1]     =  _writeHead       & 0xFF;
    hdr[HDR_COUNT]        = (_count     >> 8) & 0xFF;
    hdr[HDR_COUNT + 1]    =  _count           & 0xFF;
    hdr[HDR_INTERVAL]     = (_interval  >> 8) & 0xFF;
    hdr[HDR_INTERVAL + 1] =  _interval        & 0xFF;

    _write(0, hdr, FRAM_HEADER_SIZE);
}

// ─── Entry lezen / schrijven ───────────────────────────────────────────────────
// FRAMEntry wordt als raw bytes opgeslagen (little-endian, ESP32-native)

void FRAMRingBuffer::_writeEntry(uint16_t index, const FRAMEntry& entry) {
    uint16_t addr = _entryOffset(index);
    _write(addr, (const uint8_t*)&entry, FRAM_ENTRY_SIZE);
}

bool FRAMRingBuffer::_readEntry(uint16_t index, FRAMEntry& out) {
    uint16_t addr = _entryOffset(index);
    _read(addr, (uint8_t*)&out, FRAM_ENTRY_SIZE);
    return true;
}

// ─── I²C lees- en schrijffuncties ─────────────────────────────────────────────
// MB85RC64TAPNF gebruikt 16-bit geheugenadressering

void FRAMRingBuffer::_write(uint16_t addr, const uint8_t* data, size_t len) {
    // Schrijf in blokken van max 30 bytes (I²C buffer limiet)
    size_t offset = 0;
    while (offset < len) {
        size_t chunk = min((size_t)30, len - offset);
        Wire.beginTransmission(_i2cAddr);
        Wire.write((uint8_t)((addr + offset) >> 8));   // Hoog adresbyte
        Wire.write((uint8_t)((addr + offset) & 0xFF)); // Laag adresbyte
        Wire.write(data + offset, chunk);
        Wire.endTransmission();
        offset += chunk;
    }
}

void FRAMRingBuffer::_read(uint16_t addr, uint8_t* data, size_t len) {
    size_t offset = 0;
    while (offset < len) {
        size_t chunk = min((size_t)30, len - offset);

        Wire.beginTransmission(_i2cAddr);
        Wire.write((uint8_t)((addr + offset) >> 8));
        Wire.write((uint8_t)((addr + offset) & 0xFF));
        Wire.endTransmission(false); // Geen STOP — repeated START voor lezen

        Wire.requestFrom((uint8_t)_i2cAddr, (uint8_t)chunk);
        for (size_t i = 0; i < chunk && Wire.available(); i++) {
            data[offset + i] = Wire.read();
        }
        offset += chunk;
    }
}
