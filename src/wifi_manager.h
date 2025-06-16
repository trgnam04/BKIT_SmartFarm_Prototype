#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H
#pragma once
#include <Preferences.h>
#include <WebServer.h>

extern Preferences preferences;
extern WebServer server;
extern bool isAPMode;
extern const char* ap_ssid;
extern const char* ap_pass;
extern const char* htmlForm;

void WiFiManager_Init();
void WiFiManager_Loop();
bool WiFiManager_IsConnected();

#endif // WIFI_MANAGER_H