#include "CaptivePortal.h"

CaptivePortal::CaptivePortal()
    : _server(80), _running(false) {}

// ─── Start ─────────────────────────────────────────────────────────────────────

void CaptivePortal::start(PortalCallback onCredentialsReceived) {
    if (_running) return;
    _callback = onCredentialsReceived;

    // AP-naam: "Deinion-XXXX" met laatste 4 tekens van MAC
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char buf[20];
    snprintf(buf, sizeof(buf), "Deinion-%02X%02X", mac[4], mac[5]);
    _apName = String(buf);

    // Zendvermogen: 17 dBm (EIRP 20 dBm met 3 dBi antenne — EU-limiet)
    WiFi.setTxPower(WIFI_POWER_17dBm);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(_apName.c_str());

    Serial.printf("[Portal] Hotspot gestart: '%s'  IP: %s\n",
                  _apName.c_str(),
                  WiFi.softAPIP().toString().c_str());

    // DNS: alle domeinnamen verwijzen naar de ESP32 (triggert captive portal popup)
    _dns.start(53, "*", WiFi.softAPIP());

    _setupRoutes();
    _server.begin();

    _running = true;
    Serial.println("[Portal] Verbind met het WiFi-netwerk '" + _apName +
                   "' om de Deinion Saver in te stellen.");
}

// ─── Routes ────────────────────────────────────────────────────────────────────

void CaptivePortal::_setupRoutes() {

    // Hoofdpagina met configuratieformulier
    _server.on("/", HTTP_GET, [this]() {
        _server.send(200, "text/html", _buildPage());
    });

    // Formulier opslaan
    _server.on("/save", HTTP_POST, [this]() {
        String ssid     = _server.arg("ssid");
        String password = _server.arg("password");

        if (ssid.isEmpty()) {
            _server.send(200, "text/html",
                _buildPage("<p class='err'>Vul een netwerknaam in.</p>"));
            return;
        }

        Serial.printf("[Portal] Credentials ontvangen voor '%s'\n", ssid.c_str());
        _server.send(200, "text/html",
            _buildPage("<p class='ok'>Opgeslagen! Verbinden met '" + ssid + "'...</p>"));

        // Kleine vertraging zodat de browser de respons kan tonen
        delay(1500);

        if (_callback) _callback(ssid, password);
    });

    // ── Captive portal detectie-URLs ──────────────────────────────────────────
    // iOS, Android en Windows controleren een specifieke URL om te bepalen
    // of er een captive portal is. We sturen een redirect naar de hoofdpagina.

    auto redirect = [this]() {
        _server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
        _server.send(302, "text/plain", "");
    };

    // iOS / macOS
    _server.on("/hotspot-detect.html",          HTTP_GET, redirect);
    _server.on("/library/test/success.html",    HTTP_GET, redirect);
    // Android
    _server.on("/generate_204",                 HTTP_GET, redirect);
    _server.on("/gen_204",                      HTTP_GET, redirect);
    // Windows
    _server.on("/ncsi.txt",                     HTTP_GET, redirect);
    _server.on("/connecttest.txt",              HTTP_GET, redirect);
    // Overige onbekende URLs
    _server.onNotFound([this]() {
        _server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
        _server.send(302, "text/plain", "");
    });
}

// ─── HTML pagina ───────────────────────────────────────────────────────────────

String CaptivePortal::_buildPage(const String& message) {
    String html = R"(<!DOCTYPE html>
<html lang="nl">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Deinion Saver instellen</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
    background: #f0f4f8;
    min-height: 100vh;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 20px;
  }
  .card {
    background: white;
    border-radius: 16px;
    padding: 32px 28px;
    width: 100%;
    max-width: 380px;
    box-shadow: 0 4px 24px rgba(0,0,0,0.10);
  }
  .logo {
    font-size: 22px;
    font-weight: 700;
    color: #1a73e8;
    margin-bottom: 4px;
  }
  .sub {
    font-size: 13px;
    color: #888;
    margin-bottom: 24px;
  }
  label {
    display: block;
    font-size: 13px;
    font-weight: 600;
    color: #444;
    margin-bottom: 6px;
  }
  input {
    width: 100%;
    padding: 12px 14px;
    border: 1.5px solid #ddd;
    border-radius: 10px;
    font-size: 15px;
    margin-bottom: 16px;
    outline: none;
    transition: border-color 0.2s;
  }
  input:focus { border-color: #1a73e8; }
  button {
    width: 100%;
    padding: 13px;
    background: #1a73e8;
    color: white;
    border: none;
    border-radius: 10px;
    font-size: 16px;
    font-weight: 600;
    cursor: pointer;
  }
  button:active { background: #1557b0; }
  .ok  { color: #1a7a1a; background: #e6f4e6; padding: 10px 14px;
         border-radius: 8px; margin-bottom: 16px; font-size: 14px; }
  .err { color: #b00020; background: #fce8ea; padding: 10px 14px;
         border-radius: 8px; margin-bottom: 16px; font-size: 14px; }
  .device { font-size: 11px; color: #bbb; text-align: center; margin-top: 20px; }
</style>
</head>
<body>
<div class="card">
  <div class="logo">Deinion Saver</div>
  <div class="sub">Verbind met je thuisnetwerk</div>
  )";

    if (!message.isEmpty()) html += message;

    html += R"(
  <form method="POST" action="/save">
    <label for="ssid">Netwerknaam (SSID)</label>
    <input type="text" id="ssid" name="ssid"
           placeholder="Mijn WiFi netwerk" autocomplete="off" required>
    <label for="password">Wachtwoord</label>
    <input type="password" id="password" name="password"
           placeholder="••••••••" autocomplete="off">
    <button type="submit">Opslaan en verbinden</button>
  </form>
  <div class="device">)";

    html += _apName;
    html += R"(</div>
</div>
</body>
</html>)";

    return html;
}

// ─── Update & Stop ─────────────────────────────────────────────────────────────

void CaptivePortal::update() {
    if (!_running) return;
    _dns.processNextRequest();
    _server.handleClient();
}

void CaptivePortal::stop() {
    if (!_running) return;
    _server.stop();
    _dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    _running = false;
    Serial.println("[Portal] Gestopt");
}
