#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H
#pragma once
#include "common.h"

void mqtt_init();
void mqtt_loop();
bool mqtt_is_connected();

#endif  // MQTT_SERVICE_H