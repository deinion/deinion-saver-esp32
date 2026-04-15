#include "CaptivePortal.h"

// ─── HTML pagina (PROGMEM — opgeslagen in flash, niet in RAM) ─────────────────
// Identiek aan de Pi-versie: dark theme, welcome overlay (Android vereiste),
// WiFi-dropdown met scan, handmatige invoer, wachtwoord-validatie.

static const char WIFI_PAGE[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="nl">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>WiFi Instellen - Deinion Saver</title>
<style>
  * { box-sizing: border-box; }
  body { font-family: 'Segoe UI', sans-serif; background: #121212; color: #e0e0e0; margin: 0; padding: 20px; }
  .wifi-container { max-width: 600px; margin: 40px auto; padding: 20px; }
  .wifi-section { background: #1e1e1e; border: 1px solid #333; border-radius: 8px; padding: 30px; margin-bottom: 25px; }
  .wifi-section h2 { margin-top: 0; color: #fff; border-bottom: 2px solid #007bff; padding-bottom: 10px; margin-bottom: 20px; text-align: center; }
  .form-row { margin-bottom: 20px; }
  .form-row label { display: block; color: #aaa; margin-bottom: 8px; font-weight: bold; }
  .form-row input, .form-row select {
    width: 100%; padding: 12px; background: #2d2d2d; border: 1px solid #444;
    color: #fff; border-radius: 6px; font-size: 16px; box-sizing: border-box;
  }
  .save-button {
    background: #007bff; color: white; border: none; padding: 16px 32px;
    font-size: 1.2em; border-radius: 6px; cursor: pointer; font-weight: bold;
    width: 100%; margin-top: 10px; transition: background 0.2s;
  }
  .save-button:hover { background: #0056b3; }
  .save-button:disabled { background: #555; cursor: not-allowed; }
  .refresh-btn {
    background: transparent; border: none; color: #007bff; cursor: pointer;
    font-size: 0.9em; float: right; margin-top: -30px;
  }
  .scan-loader { text-align: center; color: #888; display: none; padding: 20px; }
  .spinner { display: inline-block; width: 20px; height: 20px; border: 3px solid rgba(255,255,255,.3);
    border-radius: 50%; border-top-color: #007bff; animation: spin 1s ease-in-out infinite;
    margin-right: 10px; vertical-align: middle; }
  @keyframes spin { to { transform: rotate(360deg); } }
  .manual-toggle { text-align: center; margin-top: 20px; color: #888; font-size: 0.9em; cursor: pointer; text-decoration: underline; }
  #manual-input { display: none; margin-top: 15px; border-top: 1px solid #333; padding-top: 15px; }

  /* Welcome overlay — verplicht voor Android (vereist scherm-aanraking voor dropdowns) */
  .welcome-overlay {
    position: fixed; top: 0; left: 0; width: 100%; height: 100%;
    background: rgba(0,0,0,0.85); backdrop-filter: blur(8px);
    z-index: 9999; display: flex; align-items: center; justify-content: center; padding: 20px;
  }
  .welcome-card {
    background: #1e1e1e; border: 1px solid #444; border-radius: 12px;
    padding: 30px; max-width: 450px; width: 100%; text-align: center;
    box-shadow: 0 10px 40px rgba(0,0,0,0.5);
  }
  .welcome-card h2 { color: #fff; margin-bottom: 20px; font-size: 1.4em; }
  .welcome-card p { color: #ccc; line-height: 1.5; margin-bottom: 30px; }
  .btn-group { display: flex; gap: 15px; flex-direction: column; }
  .btn-welcome { padding: 15px; font-size: 1.1em; border: none; border-radius: 8px; cursor: pointer; font-weight: bold; transition: transform 0.1s; }
  .btn-yes { background: #007bff; color: white; }
  .btn-no  { background: #444;    color: white; }
  .btn-welcome:active { transform: scale(0.98); }

  /* Loading overlay (bij rescan) */
  .loading-overlay {
    position: fixed; top: 0; left: 0; right: 0; bottom: 0;
    background: rgba(0,0,0,0.95); display: none;
    justify-content: center; align-items: center; flex-direction: column; z-index: 10000;
  }
  .loading-overlay.active { display: flex; }
  .loading-text { color: #fff; margin-top: 20px; font-size: 1.1em; text-align: center; line-height: 1.6; }
</style>
</head>
<body>

<!-- Welcome overlay: verplicht eerst aanraken (Android captive portal vereiste) -->
<div id="welcome-overlay" class="welcome-overlay">
  <div class="welcome-card" id="welcome-card">
    <h2>&#9888;&#65039; Verbinding Verbroken</h2>
    <p>Deinion Saver kan het wifi-netwerk niet bereiken.</p>
    <p>Wilt u een <strong>nieuw netwerk</strong> of wachtwoord instellen?</p>
    <div class="btn-group">
      <button class="btn-welcome btn-yes" onclick="startSetup()">Ja, instellen</button>
      <button class="btn-welcome btn-no"  onclick="retryConnection()">Nee, probeer opnieuw</button>
    </div>
  </div>
</div>

<!-- Rescan loading overlay -->
<div class="loading-overlay" id="loading-overlay">
  <div class="spinner" style="width:50px;height:50px;border-width:4px;"></div>
  <div class="loading-text" id="loading-text">
    <strong>&#128260; Netwerkscan Gestart</strong><br><br>
    Deinion Saver zoekt naar beschikbare netwerken.<br>
    Dit duurt ongeveer <strong>15 seconden</strong>.<br><br>
    Verbind opnieuw met het Deinion-netwerk en ververs deze pagina.
  </div>
</div>

<div class="wifi-container">
  <div class="wifi-section">
    <h2>&#128246; WiFi Instellen</h2>
    <p style="color:#ccc;text-align:center;margin-bottom:30px;">
      Selecteer uw WiFi netwerk om de Deinion Saver te verbinden.
    </p>

    <div id="scan-ui">
      <div class="form-row">
        <label>Kies Netwerk:</label>
        <button class="refresh-btn" onclick="scanWifi()">&#8635; Verversen</button>
        <select id="wifi-select" onchange="checkManual(this)">
          <option value="" disabled selected>-- Selecteer Netwerk --</option>
        </select>
      </div>
      <div id="scan-loader" class="scan-loader">
        <div class="spinner"></div> Zoeken naar netwerken...
      </div>
    </div>

    <div id="manual-input">
      <div class="form-row">
        <label>WiFi Naam (SSID):</label>
        <input type="text" id="wifi-ssid" placeholder="Bijv. Ziggo12345">
      </div>
    </div>

    <div class="form-row">
      <label>Wachtwoord:</label>
      <div style="position:relative;">
        <input type="password" id="wifi-pass" placeholder="Uw WiFi wachtwoord" style="padding-right:40px;">
        <span onclick="togglePassword()" style="position:absolute;right:10px;top:50%;transform:translateY(-50%);cursor:pointer;font-size:1.2em;user-select:none;">
          &#128065;&#65039;
        </span>
      </div>
    </div>

    <button class="save-button" onclick="saveWifi()">Verbinden</button>
    <div class="manual-toggle" onclick="toggleManual()">
      Netwerk niet in de lijst? Voer handmatig in.
    </div>
  </div>
</div>

<script>
function togglePassword() {
  const i = document.getElementById('wifi-pass');
  i.type = (i.type === 'password') ? 'text' : 'password';
}

function checkManual(sel) {
  if (sel.value) {
    document.getElementById('wifi-ssid').value = sel.value;
    document.getElementById('manual-input').style.display = 'none';
  }
}

function toggleManual() {
  const d = document.getElementById('manual-input');
  const sel = document.getElementById('wifi-select');
  if (d.style.display === 'block') {
    d.style.display = 'none';
    sel.value = '';
  } else {
    d.style.display = 'block';
    document.getElementById('wifi-ssid').value = '';
    document.getElementById('wifi-ssid').focus();
  }
}

async function scanWifi() {
  document.getElementById('loading-overlay').classList.add('active');
  try { await fetch('/api/wifi/rescan', { method: 'POST' }); } catch(e) {}
}

async function startSetup() {
  document.getElementById('welcome-overlay').style.display = 'none';
  const select = document.getElementById('wifi-select');
  select.innerHTML = '<option disabled selected>Bezig met laden...</option>';
  try {
    const resp = await fetch('/api/wifi/networks');
    const data = await resp.json();
    select.innerHTML = '<option value="" disabled selected>-- Selecteer Netwerk --</option>';
    if (data.networks && data.networks.length > 0) {
      data.networks.forEach(net => {
        const o = document.createElement('option');
        o.value = net.ssid;
        o.textContent = net.ssid + ' (' + net.signal + '%) ' + (net.security ? '\uD83D\uDD12' : '');
        select.appendChild(o);
      });
    } else {
      select.innerHTML = '<option disabled>Geen netwerken gevonden. Klik Verversen.</option>';
    }
  } catch(e) {
    select.innerHTML = '<option disabled>Laden mislukt. Klik Verversen.</option>';
  }
}

async function retryConnection() {
  const card = document.getElementById('welcome-card');
  card.innerHTML = '<div class="spinner" style="width:40px;height:40px;margin:20px auto;"></div>'
    + '<h3>Opnieuw Verbinden...</h3>'
    + '<p>Deinion Saver probeert het laatste bekende netwerk.</p>'
    + '<p style="font-size:0.9em;color:#888;">Dit scherm sluit vanzelf als de verbinding hersteld is.</p>';
  try { await fetch('/api/wifi/retry', { method: 'POST' }); } catch(e) {}
}

async function saveWifi() {
  const manualDiv = document.getElementById('manual-input');
  let ssid = (manualDiv.style.display === 'block')
    ? document.getElementById('wifi-ssid').value
    : document.getElementById('wifi-select').value;
  if (!ssid) ssid = document.getElementById('wifi-ssid').value;

  const pass = document.getElementById('wifi-pass').value;
  if (!ssid || !pass) { alert('Vul a.u.b. een netwerk en wachtwoord in.'); return; }
  if (pass.length < 8) {
    alert('Het ingevoerde wachtwoord is te kort.\n\nEen WiFi-wachtwoord moet uit minimaal 8 tekens bestaan.');
    return;
  }
  if (/['"\\;`]/.test(pass)) {
    alert("Het wachtwoord bevat tekens die niet zijn toegestaan.\n\nDe volgende tekens kunt u niet gebruiken: ' \" \\ ; `");
    return;
  }

  const btn = document.querySelector('.save-button');
  btn.disabled = true;
  btn.textContent = 'Bezig met opslaan...';

  try {
    const resp = await fetch('/api/wifi/save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ ssid: ssid, password: pass })
    });
    const data = await resp.json();
    if (data.success) {
      alert('\u2705 ' + data.message + '\n\nDe Deinion Saver verbindt nu met uw netwerk.');
    } else {
      alert('\u274C Fout: ' + (data.error || 'Onbekende fout'));
      btn.disabled = false;
      btn.textContent = 'Verbinden';
    }
  } catch(e) {
    alert('\u274C Communicatiefout met Deinion Saver.');
    btn.disabled = false;
    btn.textContent = 'Verbinden';
  }
}
</script>
</body>
</html>)rawhtml";

// ─── CaptivePortal implementatie ──────────────────────────────────────────────

CaptivePortal::CaptivePortal()
    : _server(80), _running(false) {}

void CaptivePortal::start(PortalCallback onCredentialsReceived) {
    if (_running) return;
    _callback = onCredentialsReceived;

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char buf[20];
    snprintf(buf, sizeof(buf), "Deinion-%02X%02X", mac[4], mac[5]);
    _apName = String(buf);

    WiFi.setTxPower(WIFI_POWER_17dBm);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(_apName.c_str());

    Serial.printf("[Portal] Hotspot: '%s'  IP: %s\n",
                  _apName.c_str(), WiFi.softAPIP().toString().c_str());

    _dns.start(53, "*", WiFi.softAPIP());
    _setupRoutes();
    _server.begin();
    _running = true;

    // Initiële WiFi-scan starten zodat netwerken beschikbaar zijn bij startSetup()
    WiFi.scanNetworksAsync([](int n) {
        Serial.printf("[Portal] Scan klaar: %d netwerken gevonden\n", n);
    });
}

void CaptivePortal::_setupRoutes() {

    // Hoofdpagina
    _server.on("/", HTTP_GET, [this]() {
        _server.send_P(200, "text/html", WIFI_PAGE);
    });

    // WiFi-netwerken ophalen (gecacht van laatste scan)
    _server.on("/api/wifi/networks", HTTP_GET, [this]() {
        String json = "{\"networks\":[";
        int n = WiFi.scanComplete();
        if (n > 0) {
            for (int i = 0; i < n; i++) {
                if (i > 0) json += ",";
                String ssid = WiFi.SSID(i);
                ssid.replace("\"", "\\\"");
                int signal = constrain(map(WiFi.RSSI(i), -100, -40, 0, 100), 0, 100);
                bool secure = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
                json += "{\"ssid\":\"" + ssid + "\",\"signal\":" +
                        String(signal) + ",\"security\":" +
                        (secure ? "true" : "false") + "}";
            }
        }
        json += "]}";
        _server.send(200, "application/json", json);
    });

    // Opnieuw scannen
    _server.on("/api/wifi/rescan", HTTP_POST, [this]() {
        _server.send(200, "application/json", "{\"ok\":true}");
        WiFi.scanNetworksAsync([](int n) {
            Serial.printf("[Portal] Rescan klaar: %d netwerken\n", n);
        });
    });

    // Credentials opslaan
    _server.on("/api/wifi/save", HTTP_POST, [this]() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "application/json", "{\"error\":\"Geen data\"}");
            return;
        }
        String body = _server.arg("plain");
        // Eenvoudige JSON-parse (geen lib nodig voor twee velden)
        String ssid = _extractJson(body, "ssid");
        String pass = _extractJson(body, "password");

        if (ssid.isEmpty() || pass.isEmpty()) {
            _server.send(400, "application/json", "{\"error\":\"SSID of wachtwoord ontbreekt\"}");
            return;
        }

        _server.send(200, "application/json",
            "{\"success\":true,\"message\":\"Instellingen opgeslagen\"}");

        delay(500);
        if (_callback) _callback(ssid, pass);
    });

    // Opnieuw proberen verbinden (knop "Nee, probeer opnieuw")
    _server.on("/api/wifi/retry", HTTP_POST, [this]() {
        _server.send(200, "application/json", "{\"ok\":true}");
        // Geeft de WiFiManager het signaal om opnieuw te proberen
        // Dit wordt afgehandeld via de callback met lege credentials
        if (_callback) _callback("__RETRY__", "");
    });

    // Captive portal detectie voor iOS, Android en Windows
    auto redirect = [this]() {
        _server.sendHeader("Location",
            "http://" + WiFi.softAPIP().toString() + "/");
        _server.send(302, "text/plain", "");
    };
    _server.on("/hotspot-detect.html",       HTTP_GET, redirect);
    _server.on("/library/test/success.html", HTTP_GET, redirect);
    _server.on("/generate_204",              HTTP_GET, redirect);
    _server.on("/gen_204",                   HTTP_GET, redirect);
    _server.on("/ncsi.txt",                  HTTP_GET, redirect);
    _server.on("/connecttest.txt",           HTTP_GET, redirect);
    _server.onNotFound([this]() {
        _server.sendHeader("Location",
            "http://" + WiFi.softAPIP().toString() + "/");
        _server.send(302, "text/plain", "");
    });
}

// Minimalistische JSON-waarde extractor (vermijdt zware JSON-lib)
String CaptivePortal::_extractJson(const String& json, const String& key) {
    String search = "\"" + key + "\":\"";
    int start = json.indexOf(search);
    if (start < 0) return "";
    start += search.length();
    int end = json.indexOf("\"", start);
    if (end < 0) return "";
    return json.substring(start, end);
}

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
