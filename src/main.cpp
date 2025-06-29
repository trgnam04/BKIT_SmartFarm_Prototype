#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BH1750.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "wifi_manager.h"

#include <PubSubClient.h> // Thư viện cho MQTT

// --------- Pin & hằng số ---------
#define DHTPIN 32
#define DHTTYPE DHT21
#define RAIN_SENSOR_PIN 35
#define SOIL_SENSOR_PIN 34
#define RELAY_PIN 19

#define HUMIDITY_MIN 25
#define HUMIDITY_MAX 50

#define MQTT_HOST "mqtt.abcsolutions.com.vn"
#define MQTT_PORT 1883
#define MQTT_USERNAME "abcsolution"
#define MQTT_PASSWORD "CseLAbC5c6"
#define MQTT_TOPIC_SUB "/esp32Relay/test"
#define MQTT_TOPIC_PUB "/esp32Relay/request_test"

constexpr uint32_t T_SENSOR = 1000; // ms
constexpr uint32_t T_LCD = 250;     // ms
constexpr uint32_t T_PAGE = 4000;   // ms
constexpr uint32_t T_RELAY = 500;   // ms
constexpr uint32_t T_SEND = 10000;  // ms

const char *POST_URL = "http://ems.thebestits.vn/Value/GetData";

LiquidCrystal_I2C lcd(0x27, 16, 2);
BH1750 lightMeter;
DHT dht(DHTPIN, DHTTYPE);

DynamicJsonDocument doc(256);

// --------- Biến MQTT Client ---------
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

// --------- Biến runtime ---------
uint32_t tLastSensor{0}, tLastLCD{0}, tLastPage{0},
    tLastRelay{0}, tLastSend{0};
bool page = 0, pumpOn = 0;

float temp = NAN, hum = NAN, lux = 0;
uint16_t rainRaw = 0, soilRaw = 0; // 0‑4095
float soilPct = 0;

// Quản lý máy bơm
bool isPumpRunning = false;
unsigned long pumpStopTime = 0;

// --------- Hàm tiện ích ---------
uint16_t analogAverage(uint8_t pin, uint8_t samples = 8)
{
  uint32_t sum = 0;
  for (uint8_t i = 0; i < samples; ++i)
  {
    sum += analogRead(pin);
    delayMicroseconds(150); // ~12‑bit ADC cần >130 µs
  }
  return sum / samples;
}

bool readDHT(float &t, float &h)
{
  t = dht.readTemperature(false);
  h = dht.readHumidity(false);
  if (isnan(t) || isnan(h))
  { // đọc lại 1 lần
    delay(50);
    t = dht.readTemperature(false);
    h = dht.readHumidity(false);
  }
  return !(isnan(t) || isnan(h));
}

bool sendToServer()
{
  if (WiFi.status() != WL_CONNECTED)
    return false;

  doc["stationCode"] = "K13_TEST";
  doc["temperature"] = temp;
  doc["humidity"] = hum;
  doc["rainValue"] = rainRaw;
  doc["soilPercent"] = soilPct;
  doc["lux"] = lux;

  String payload;
  serializeJson(doc, payload);

  HTTPClient http;
  http.begin(POST_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);

  int code = http.POST(payload);
  Serial.printf("[HTTP] POST… code=%d\n", code);
  if (code > 0)
    Serial.println(http.getString());

  http.end();
  return (code >= 200 && code < 300);
}

// --------- Hàm xử lý callback ---------
/**
 * @brief Hàm callback được gọi khi có tin nhắn MQTT đến.
 * Xử lý lệnh bật máy bơm với thời gian định trước.
 * @param topic Chủ đề của tin nhắn.
 * @param payload Nội dung của tin nhắn (dạng chuỗi).
 */
void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
  Serial.println("---");
  Serial.println("New message received:");
  Serial.print("Topic: ");
  Serial.println(topic);

  // Chuyển payload thành chuỗi
  String message;
  for (unsigned int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }
  Serial.print("Payload: ");
  Serial.println(message);

  // Phân tích JSON
  DynamicJsonDocument doc(256); // Kích thước đủ cho payload đơn giản
  DeserializationError error = deserializeJson(doc, message);

  // Kiểm tra xem parse JSON có thành công không
  if (error)
  {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return; // Thoát nếu JSON không hợp lệ
  }

  // Kiểm tra xem có phải lệnh bật máy bơm không
  if (doc.containsKey("command") && strcmp(doc["command"], "PUMP_ON") == 0 && doc.containsKey("duration"))
  {

    unsigned long duration = doc["duration"]; // Lấy giá trị duration

    // Chỉ thực thi nếu duration > 0
    if (duration > 0)
    {
      Serial.printf("Received PUMP_ON command. Duration: %lu ms.\n", duration);

      digitalWrite(RELAY_PIN, HIGH); // Bật relay
      isPumpRunning = true;
      pumpStopTime = millis() + duration;

      Serial.printf("Pump turned ON. Will turn off at: %lu\n", pumpStopTime);

      // Gửi tin nhắn xác nhận đã nhận lệnh
      StaticJsonDocument<100> ackDoc;
      ackDoc["status"] = "ACK";
      ackDoc["command_received"] = "PUMP_ON";
      String ackPayload;
      serializeJson(ackDoc, ackPayload);
      mqtt_client.publish(MQTT_TOPIC_PUB, ackPayload.c_str());
    }
    else
    {
      Serial.println("Ignoring command: duration is zero.");
    }
  }
  else
  {
    Serial.println("Message is not a valid PUMP_ON command.");
    // Có thể xử lý các lệnh khác tại đây
  }
}

void mqttReconnect()
{
  // Lặp lại cho đến khi kết nối thành công
  while (!mqtt_client.connected())
  {
    Serial.print("Attempting MQTT connection... ");

    // Kết nối với username & password
    if (mqtt_client.connect("ESP32Client", MQTT_USERNAME, MQTT_PASSWORD))
    {
      Serial.println("connected!");

      // Đăng ký lắng nghe topic
      if (mqtt_client.subscribe(MQTT_TOPIC_SUB))
      {
        Serial.print("Subscribed to topic: ");
        Serial.println(MQTT_TOPIC_SUB);
      }
      else
      {
        Serial.println("Failed to subscribe!");
      }
    }
    else
    {
      Serial.print("Failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000); // Đợi trước khi thử lại
    }
  }
}

// --------- setup ---------
void setup()
{
  Serial.begin(115200);
  Wire.begin(21, 22);

  lcd.begin(16, 2);
  lcd.backlight();
  dht.begin();
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // HIGH = off (tùy module)

  WiFiManager_Init();
  mqtt_client.setServer(MQTT_HOST, MQTT_PORT);
  mqtt_client.setCallback(mqtt_callback);
  Serial.println("MQTT setup complete.");
}

// --------- loop ---------
void loop()
{
  uint32_t now = millis();

  // // 1. Đọc cảm biến
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

  // 3. Gửi server
  if (now - tLastSend >= T_SEND) {
    if (sendToServer()) tLastSend = millis();   // cập nhật SAU khi gửi
  }

  // 4. Cập nhật LCD
  if (now - tLastPage >= T_PAGE)
  {
    tLastPage = now;
    page = !page;
    lcd.clear();
  }
  if (now - tLastLCD >= T_LCD)
  {
    tLastLCD = now;
    if (!page)
    {
      if (isnan(temp) || isnan(hum))
      {
        lcd.setCursor(0, 0);
        lcd.print("DHT21 error   ");
      }
      else
      {
        lcd.setCursor(0, 0);
        lcd.printf("T:%.1fC H:%.0f%% ", temp, hum);
      }
      lcd.setCursor(0, 1);
      lcd.printf("Soil:%02.0f%% %s ", soilPct, pumpOn ? "ON " : "OFF");
    }
    else
    {
      lcd.setCursor(0, 0);
      lcd.printf("Lux:%05.0f lx", lux);
      lcd.setCursor(0, 1);
      lcd.printf("Rain:%4u     ", rainRaw);
    }
  }

  // --- Logic kiểm soát thời gian chạy máy bơm ---
  // 1. Kiểm tra xem máy bơm có đang trong trạng thái cần chạy không
  if (isPumpRunning)
  {
    // 2. Kiểm tra xem thời gian hiện tại đã đến lúc phải tắt chưa
    if (millis() >= pumpStopTime)
    {
      Serial.println("Pump timer finished. Turning pump OFF.");

      // Tắt máy bơm
      digitalWrite(RELAY_PIN, LOW);
      isPumpRunning = false; // Reset trạng thái
    }
  }

  WiFiManager_Loop();
  if (!mqtt_client.connected())
  {
    mqttReconnect();
  }
  mqtt_client.loop();
}
