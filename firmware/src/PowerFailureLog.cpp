#include "PowerFailureLog.h"

// ─── Initialisatie ─────────────────────────────────────────────────────────────

bool PowerFailureLog::begin() {
    if (!LittleFS.begin(true)) {
        Serial.println("[FailureLog] LittleFS mount mislukt");
        return false;
    }
    Serial.printf("[FailureLog] Klaar. %d storingen in log.\n", count());
    return true;
}

// ─── Update: vergelijk telegram-entries met bekende log ───────────────────────

int PowerFailureLog::update(const std::vector<PowerFailure>& fromTelegram,
                             const char* nowUtc) {
    if (fromTelegram.empty()) return 0;

    std::vector<PowerFailure> known;
    _load(known);

    int newCount = 0;
    for (const auto& entry : fromTelegram) {
        if (!_alreadyKnown(known, entry)) {
            PowerFailure newEntry = entry;
            strncpy(newEntry.detected_at, nowUtc, sizeof(newEntry.detected_at) - 1);
            newEntry.detected_at[sizeof(newEntry.detected_at) - 1] = '\0';

            known.insert(known.begin(), newEntry); // Nieuwste vooraan
            newCount++;

            Serial.printf("[FailureLog] ⚡ Nieuwe stroomstoring: %s, %lu sec\n",
                          newEntry.timestamp, newEntry.duration_s);
        }
    }

    if (newCount > 0) {
        // Begrenzen op maximum
        if ((int)known.size() > MAX_STORED_FAILURES) {
            known.resize(MAX_STORED_FAILURES);
        }
        _save(known);
    }

    return newCount;
}

// ─── Laden en opslaan (JSON in LittleFS) ──────────────────────────────────────

bool PowerFailureLog::_load(std::vector<PowerFailure>& out) {
    out.clear();
    if (!LittleFS.exists(FAILURE_LOG_FILE)) return true;

    File f = LittleFS.open(FAILURE_LOG_FILE, "r");
    if (!f) return false;

    String content = f.readString();
    f.close();

    // Eenvoudige JSON-parser voor array van objecten
    int pos = 0;
    while (true) {
        int start = content.indexOf('{', pos);
        if (start < 0) break;
        int end = content.indexOf('}', start);
        if (end < 0) break;

        String obj = content.substring(start, end + 1);
        PowerFailure pf;
        memset(&pf, 0, sizeof(pf));

        // Extract "timestamp"
        _extractStr(obj, "ts", pf.timestamp, sizeof(pf.timestamp));
        // Extract "duration_s"
        pf.duration_s = _extractUint(obj, "dur");
        // Extract "detected_at"
        _extractStr(obj, "det", pf.detected_at, sizeof(pf.detected_at));

        if (strlen(pf.timestamp) > 0) {
            out.push_back(pf);
        }
        pos = end + 1;
    }
    return true;
}

bool PowerFailureLog::_save(const std::vector<PowerFailure>& entries) {
    File f = LittleFS.open(FAILURE_LOG_FILE, "w");
    if (!f) {
        Serial.println("[FailureLog] Opslaan mislukt: kan bestand niet openen");
        return false;
    }

    f.print("[");
    for (size_t i = 0; i < entries.size(); i++) {
        if (i > 0) f.print(",");
        f.printf("{\"ts\":\"%s\",\"dur\":%lu,\"det\":\"%s\"}",
                 entries[i].timestamp,
                 entries[i].duration_s,
                 entries[i].detected_at);
    }
    f.print("]");
    f.close();
    return true;
}

// ─── Alle storingen ophalen ────────────────────────────────────────────────────

std::vector<PowerFailure> PowerFailureLog::getAll() {
    std::vector<PowerFailure> entries;
    _load(entries);
    return entries;
}

int PowerFailureLog::count() {
    std::vector<PowerFailure> entries;
    _load(entries);
    return (int)entries.size();
}

// ─── Dubbel-check: is deze storing al bekend? ─────────────────────────────────
// Vergelijkt op tijdstempel — elke storing heeft een unieke starttijd

bool PowerFailureLog::_alreadyKnown(const std::vector<PowerFailure>& known,
                                     const PowerFailure& entry) {
    for (const auto& k : known) {
        if (strcmp(k.timestamp, entry.timestamp) == 0) return true;
    }
    return false;
}

// ─── HTML pagina ───────────────────────────────────────────────────────────────

String PowerFailureLog::buildHtmlPage() {
    std::vector<PowerFailure> entries;
    _load(entries);

    String html = F(R"(<!DOCTYPE html>
<html lang="nl">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Stroomstoringen - Deinion Saver</title>
<style>
  body { font-family:'Segoe UI',sans-serif; background:#121212; color:#e0e0e0; margin:0; padding:20px; }
  .container { max-width:800px; margin:0 auto; }
  h1 { color:#fff; border-bottom:2px solid #007bff; padding-bottom:10px; }
  .summary { background:#1e1e1e; border:1px solid #333; border-radius:8px;
             padding:15px 20px; margin-bottom:20px; display:flex; gap:30px; }
  .sum-item { text-align:center; }
  .sum-val  { font-size:2em; font-weight:bold; color:#ff9800; }
  .sum-lbl  { font-size:0.85em; color:#888; margin-top:4px; }
  table { width:100%; border-collapse:collapse; }
  th { background:#252525; padding:12px; text-align:left; color:#aaa;
       font-size:0.85em; letter-spacing:1px; text-transform:uppercase; }
  td { padding:12px; border-bottom:1px solid #2a2a2a; }
  tr:hover td { background:#1e1e1e; }
  .dur { color:#ff9800; font-weight:bold; }
  .long { color:#f44336; }
  .empty { text-align:center; color:#555; padding:40px; }
  a.back { color:#007bff; text-decoration:none; display:inline-block; margin-bottom:20px; }
</style>
</head>
<body>
<div class="container">
  <a class="back" href="/">&#8592; Terug</a>
  <h1>&#9889; Stroomstoringen</h1>)");

    // Samenvatting
    int total = (int)entries.size();
    int longFailures = 0;
    uint32_t longestSec = 0;
    for (const auto& e : entries) {
        if (e.duration_s >= 180) longFailures++;
        if (e.duration_s > longestSec) longestSec = e.duration_s;
    }

    html += "<div class=\"summary\">";
    html += "<div class=\"sum-item\"><div class=\"sum-val\">" + String(total) +
            "</div><div class=\"sum-lbl\">Totaal gelogd</div></div>";
    html += "<div class=\"sum-item\"><div class=\"sum-val\">" + String(longFailures) +
            "</div><div class=\"sum-lbl\">Langer dan 3 min</div></div>";
    html += "<div class=\"sum-item\"><div class=\"sum-val\">" +
            _formatDuration(longestSec) +
            "</div><div class=\"sum-lbl\">Langste storing</div></div>";
    html += "</div>";

    if (entries.empty()) {
        html += "<div class=\"empty\">Geen storingen gelogd &#127881;</div>";
    } else {
        html += F(R"(<table>
<thead><tr>
  <th>Datum &amp; tijd (UTC)</th>
  <th>Duur</th>
  <th>Ontdekt op</th>
</tr></thead><tbody>)");

        for (const auto& e : entries) {
            bool isLong = (e.duration_s >= 180);
            html += "<tr><td>" + String(e.timestamp) + "</td>";
            html += "<td class=\"dur " + String(isLong ? "long" : "") + "\">" +
                    _formatDuration(e.duration_s) + "</td>";
            html += "<td style=\"color:#555;font-size:0.9em\">" +
                    String(e.detected_at) + "</td></tr>";
        }
        html += "</tbody></table>";
    }

    html += "</div></body></html>";
    return html;
}

// ─── JSON voor API ─────────────────────────────────────────────────────────────

String PowerFailureLog::buildJson() {
    std::vector<PowerFailure> entries;
    _load(entries);

    String json = "[";
    for (size_t i = 0; i < entries.size(); i++) {
        if (i > 0) json += ",";
        json += "{\"timestamp\":\"" + String(entries[i].timestamp) +
                "\",\"duration_s\":" + String(entries[i].duration_s) +
                ",\"duration\":\"" + _formatDuration(entries[i].duration_s) +
                "\",\"detected_at\":\"" + String(entries[i].detected_at) + "\"}";
    }
    json += "]";
    return json;
}

// ─── Hulpfuncties ──────────────────────────────────────────────────────────────

String PowerFailureLog::_formatDuration(uint32_t seconds) {
    if (seconds < 60) return String(seconds) + " sec";
    uint32_t min = seconds / 60;
    uint32_t sec = seconds % 60;
    if (min < 60) {
        return String(min) + " min" + (sec > 0 ? " " + String(sec) + " sec" : "");
    }
    uint32_t hrs = min / 60;
    min = min % 60;
    return String(hrs) + " uur" + (min > 0 ? " " + String(min) + " min" : "");
}

void PowerFailureLog::_extractStr(const String& obj, const char* key,
                                   char* buf, size_t bufLen) {
    String search = String("\"") + key + "\":\"";
    int start = obj.indexOf(search);
    if (start < 0) return;
    start += search.length();
    int end = obj.indexOf('"', start);
    if (end < 0) return;
    String val = obj.substring(start, end);
    strncpy(buf, val.c_str(), bufLen - 1);
    buf[bufLen - 1] = '\0';
}

uint32_t PowerFailureLog::_extractUint(const String& obj, const char* key) {
    String search = String("\"") + key + "\":";
    int start = obj.indexOf(search);
    if (start < 0) return 0;
    start += search.length();
    return (uint32_t)obj.substring(start).toInt();
}
