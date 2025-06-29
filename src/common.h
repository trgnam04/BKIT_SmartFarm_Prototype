#ifndef COMMON_H
#define COMMON_H
#pragma once

// --------- THƯ VIỆN CHUNG ---------
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BH1750.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <WebServer.h>

// --------- CẤU HÌNH HỆ THỐNG (PINS & CONSTANTS) ---------
#define DHTPIN 32
#define DHTTYPE DHT21
#define RAIN_SENSOR_PIN 35
#define SOIL_SENSOR_PIN 34
#define RELAY_PIN 19

#define LCD_I2C_ADDR 0x27

// --------- CẤU HÌNH MQTT & HTTP ---------
#define MQTT_HOST "mqtt.abcsolutions.com.vn"
#define MQTT_PORT 1883
#define MQTT_USERNAME "abcsolution"
#define MQTT_PASSWORD "CseLAbC5c6"
#define MQTT_TOPIC_SUB "/esp32Relay/test"
#define MQTT_TOPIC_PUB "/esp32Relay/request_test"

extern const char *POST_URL;

// --------- CẤU HÌNH THỜI GIAN (NON-BLOCKING) ---------
constexpr uint32_t T_SENSOR_READ = 2000;  // Đọc cảm biến mỗi giây
constexpr uint32_t T_LCD_UPDATE = 250;    // Cập nhật LCD mỗi 250ms
constexpr uint32_t T_PAGE_SWITCH = 5000;  // Chuyển trang LCD mỗi 5 giây
constexpr uint32_t T_HTTP_SEND = 10000;   // Gửi dữ liệu lên server mỗi 10 giây

// --------- CẤU HÌNH DỊCH VỤ CẢM BIẾN ---------
#define SENSOR_EMA_ALPHA 0.4f // Hệ số lọc cho EMA, f để chỉ định kiểu float

// --------- CẤU TRÚC DỮ LIỆU DÙNG CHUNG ---------
// Dữ liệu từ các cảm biến
struct SensorData {
  float temp = NAN;
  float hum = NAN;
  float lux = 0;
  uint16_t rainRaw = 0;
  uint16_t soilRaw = 0;
  float soilPct = 0;
};

// Trạng thái điều khiển máy bơm
struct PumpControl {
    bool isRunning = false;
    unsigned long stopTime = 0;
};

// Máy trạng thái cho LCD
enum LcdState {
  WIFI_STATUS,
  SENSOR_PAGE_1,
  SENSOR_PAGE_2
};

// --------- KHAI BÁO BIẾN DÙNG CHUNG (EXTERN) ---------
extern SensorData sensorData;
extern PumpControl pumpControl;
extern LcdState lcdState;

// Đối tượng dùng chung
extern LiquidCrystal_I2C lcd;
extern BH1750 lightMeter;
extern DHT dht;

// Đối tượng cho web cấu hình
extern Preferences preferences;
extern WebServer server;
extern bool isAPMode;
extern const char* ap_ssid;
extern const char* ap_pass;
extern const char* htmlForm;


#endif // COMMON_H