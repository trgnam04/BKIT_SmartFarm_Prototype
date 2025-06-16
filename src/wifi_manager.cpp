#include "wifi_manager.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

Preferences preferences;
WebServer server(80);
bool isAPMode = false;
bool wifiConnected = false;

const char* ap_ssid = "SmartFarm_Config";
const char* ap_pass = "12345678";

const char* htmlForm = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 WiFi Setup</title>
<style>body{font-family:Arial;background:#f2f2f2;display:flex;justify-content:center;align-items:center;height:100vh;}
.container{background:#fff;padding:20px;border-radius:8px;box-shadow:0 0 10px rgba(0,0,0,0.1);width:300px;}
input[type="text"],input[type="password"]{width:100%;padding:10px;margin-top:5px;border:1px solid #ccc;border-radius:4px;}
input[type="submit"]{background:#4CAF50;color:white;padding:10px;margin-top:15px;border:none;border-radius:4px;cursor:pointer;width:100%;}
</style></head><body><div class="container"><h2>ESP32 WiFi Setup</h2>
<form action="/save" method="post">
<label>WiFi SSID:</label><input type="text" name="ssid" required>
<label>WiFi Password:</label><input type="password" name="pass" required>
<input type="submit" value="Save"></form></div></body></html>
)rawliteral";

void startWebConfig() {
    if (isAPMode) return;
    isAPMode = true;

    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_pass);
    Serial.println("[WiFiManager] AP Started: SmartFarm_Config");

    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", htmlForm);
    });

    server.on("/save", HTTP_POST, []() {
        String ssid = server.arg("ssid");
        String pass = server.arg("pass");

        if (ssid.length() > 0 && pass.length() > 0) {
            preferences.begin("wifi", false);
            preferences.putString("ssid", ssid);
            preferences.putString("pass", pass);
            preferences.end();

            server.send(200, "text/html", "<h3>Saved! Rebooting...</h3>");
            delay(2000);
            ESP.restart();
        } else {
            server.send(400, "text/html", "<h3>Missing SSID or Password</h3>");
        }
    });

    server.begin();
}

bool tryConnectWiFi() {
    preferences.begin("wifi", true);
    String ssid = preferences.getString("ssid", "");
    String pass = preferences.getString("pass", "");
    preferences.end();

    if (ssid == "") {
        Serial.println("[WiFiManager] No WiFi config found");
        return false;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.printf("[WiFiManager] Connecting to %s\n", ssid.c_str());

    unsigned long startTime = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFiManager] WiFi connected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        wifiConnected = true;
        return true;
    }

    Serial.println("\n[WiFiManager] Failed to connect");
    return false;
}

void WiFiManager_Init() {
    Serial.println("[WiFiManager] Init...");
    if (!tryConnectWiFi()) {
        startWebConfig();
    }
}

void WiFiManager_Loop() {
    if (isAPMode) {
        server.handleClient();
    }
}

bool WiFiManager_IsConnected() {
    return wifiConnected;
}
