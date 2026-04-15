#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <vector>

#define FAILURE_LOG_FILE  "/failures.json"
#define MAX_STORED_FAILURES  500   // Maximaal in flash

struct PowerFailure {
    char  timestamp[20];    // UTC: "YYYY-MM-DD HH:MM:SS"
    uint32_t duration_s;    // Duur in seconden
    char  detected_at[20];  // Wanneer de ESP32 dit ontdekte
};

class PowerFailureLog {
public:
    // Initialiseer LittleFS en laad bestaande log
    bool begin();

    // Verwerk de storingen uit een nieuw telegram.
    // Schrijft alleen entries die nog niet bekend zijn naar flash.
    // Geeft het aantal nieuw opgeslagen storingen terug.
    int update(const std::vector<PowerFailure>& fromTelegram,
               const char* nowUtc);

    // Geef alle opgeslagen storingen terug (nieuwste eerst)
    std::vector<PowerFailure> getAll();

    // HTML-tabel voor de webinterface
    String buildHtmlPage();

    // JSON voor API
    String buildJson();

    int count();

private:
    bool   _load(std::vector<PowerFailure>& out);
    bool   _save(const std::vector<PowerFailure>& entries);
    bool   _alreadyKnown(const std::vector<PowerFailure>& known,
                         const PowerFailure& entry);
    String _formatDuration(uint32_t seconds);
    void   _extractStr(const String& obj, const char* key,
                       char* buf, size_t bufLen);
    uint32_t _extractUint(const String& obj, const char* key);
};
