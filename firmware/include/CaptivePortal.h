#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

using PortalCallback = std::function<void(const String& ssid, const String& password)>;

class CaptivePortal {
public:
    CaptivePortal();

    // Start AP + DNS + webserver. Naam: "Deinion-XXXX" (laatste 4 van MAC)
    void start(PortalCallback onCredentialsReceived);

    // Aanroepen in loop() — verwerkt DNS en HTTP requests
    void update();

    // Stop AP en geef RF vrij voor WiFi STA
    void stop();

    bool isRunning() const { return _running; }

private:
    void _setupRoutes();
    String _buildPage(const String& message = "");

    WebServer   _server;
    DNSServer   _dns;
    bool        _running;
    PortalCallback _callback;
    String      _apName;
};
