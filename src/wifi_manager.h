#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H
#pragma once
#include "common.h"

void WiFiManager_Init();
void WiFiManager_Loop();
bool WiFiManager_IsConnected();

#endif // WIFI_MANAGER_H