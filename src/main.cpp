#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BH1750.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "wifi_manager.h"

// --------- Pin & hằng số ---------
#define DHTPIN 32
#define DHTTYPE DHT21
#define RAIN_SENSOR_PIN 35
#define SOIL_SENSOR_PIN 34
#define RELAY_PIN 19

#define HUMIDITY_MIN 25
#define HUMIDITY_MAX 50

constexpr uint32_t T_SENSOR   = 1000;   // ms
constexpr uint32_t T_LCD      = 250;    // ms
constexpr uint32_t T_PAGE     = 4000;   // ms
constexpr uint32_t T_RELAY    = 500;    // ms
constexpr uint32_t T_SEND     = 10000;  // ms

const char* POST_URL = "http://ems.thebestits.vn/Value/GetData";

LiquidCrystal_I2C lcd(0x27, 16, 2);
BH1750 lightMeter;
DHT dht(DHTPIN, DHTTYPE);

DynamicJsonDocument doc(256);

// --------- Biến runtime ---------
uint32_t tLastSensor{0}, tLastLCD{0}, tLastPage{0},
         tLastRelay{0},  tLastSend{0};
bool page = 0, pumpOn = 0;

float  temp = NAN, hum = NAN, lux = 0;
uint16_t rainRaw = 0, soilRaw = 0;   // 0‑4095
float soilPct = 0;

// --------- Hàm tiện ích ---------
uint16_t analogAverage(uint8_t pin, uint8_t samples = 8) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < samples; ++i) {
    sum += analogRead(pin);
    delayMicroseconds(150);          // ~12‑bit ADC cần >130 µs
  }
  return sum / samples;
}

bool readDHT(float &t, float &h) {
  t = dht.readTemperature(false);
  h = dht.readHumidity(false);
  if (isnan(t) || isnan(h)) {                 // đọc lại 1 lần
    delay(50);
    t = dht.readTemperature(false);
    h = dht.readHumidity(false);
  }
  return !(isnan(t) || isnan(h));
}

bool sendToServer() {
  if (WiFi.status() != WL_CONNECTED) return false;  
  
  doc["temperature"] = temp;
  doc["humidity"]    = hum;
  doc["rainValue"]   = rainRaw;  
  doc["soilPercent"] = soilPct;
  doc["lux"]         = lux;

  String payload;
  serializeJson(doc, payload);

  HTTPClient http;
  http.begin(POST_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);

  int code = http.POST(payload);
  Serial.printf("[HTTP] POST… code=%d\n", code);
  if (code > 0) Serial.println(http.getString());

  http.end();
  return (code >= 200 && code < 300);
}


// --------- setup ---------
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  lcd.begin(16, 2); lcd.backlight();
  dht.begin();
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);      // HIGH = off (tùy module)

  // TODO: WiFiManager_Init();
}

// --------- loop ---------
void loop() {
  uint32_t now = millis();

  // 1. Đọc cảm biến
  if (now - tLastSensor >= T_SENSOR) {
    tLastSensor = now;

    readDHT(temp, hum);
    lux      = lightMeter.readLightLevel();
    rainRaw  = analogAverage(RAIN_SENSOR_PIN);
    soilRaw  = analogAverage(SOIL_SENSOR_PIN);
    soilPct  = map(soilRaw, 0, 4095, 100, 0);

    // debug
    Serial.printf("T=%.1fC H=%.0f%% Soil=%.0f%% Rain=%u Lux=%.0f\n",
                  temp, hum, soilPct, rainRaw, lux);
  }

  // 2. Điều khiển bơm
  if (now - tLastRelay >= T_RELAY) {
    tLastRelay = now;
    if (soilPct < HUMIDITY_MIN && !pumpOn) {
      digitalWrite(RELAY_PIN, LOW);   // bật
      pumpOn = true;
    } else if (soilPct > HUMIDITY_MAX && pumpOn) {
      digitalWrite(RELAY_PIN, HIGH);  // tắt
      pumpOn = false;
    }
  }

  // 3. Gửi server
  if (now - tLastSend >= T_SEND) {
    if (sendToServer()) tLastSend = millis();   // cập nhật SAU khi gửi
  }

  // 4. Cập nhật LCD
  if (now - tLastPage >= T_PAGE) {
    tLastPage = now; page = !page; lcd.clear();
  }
  if (now - tLastLCD >= T_LCD) {
    tLastLCD = now;
    if (!page) {
      if (isnan(temp) || isnan(hum)) {
        lcd.setCursor(0,0); lcd.print("DHT21 error   ");
      } else {
        lcd.setCursor(0,0);
        lcd.printf("T:%.1fC H:%.0f%% ", temp, hum);
      }
      lcd.setCursor(0,1);
      lcd.printf("Soil:%02.0f%% %s ", soilPct, pumpOn?"ON ":"OFF");
    } else {
      lcd.setCursor(0,0); lcd.printf("Lux:%05.0f lx", lux);
      lcd.setCursor(0,1); lcd.printf("Rain:%4u     ", rainRaw);
    }
  }

  WiFiManager_Loop();  // nếu dùng
}
