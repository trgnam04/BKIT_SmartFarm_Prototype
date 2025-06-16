#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BH1750.h>
#include "wifi_manager.h"
#include "DHT.h"

// Cấu hình các chân
#define DHTPIN 14
#define DHTTYPE DHT11
#define RAIN_SENSOR_PIN 32
#define SOIL_SENSOR_PIN 35
#define RELAY_PIN 26
#define RAIN_THRESHOLD 2000

// Thời gian chu kỳ của từng nhiệm vụ (đơn vị: ms)
#define SENSOR_READ_INTERVAL 5000
#define LCD_UPDATE_INTERVAL 50
#define RELAY_CONTROL_INTERVAL 500
#define SWITCH_PAGE_INTERVAL 4000

// Biến lưu thời gian lần cuối thực hiện từng nhiệm vụ
unsigned long lastSensorReadTime = 0;
unsigned long lastLCDUpdateTime = 0;
unsigned long lastRelayControlTime = 0;
unsigned long lastSwitchPageTime = 0;

// Biến lưu dữ liệu sensor
float temperature = 0;
float humidity = 0;
int rainValue = 0;
int soilValue = 0;
float soilPercent = 0;
float fluxValue = 0;

bool displayPage = 0;

// Khởi tạo cảm biến và LCD
DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Wire.begin(21, 22);
  lcd.begin(16, 2);
  lcd.backlight();

  dht.begin();
  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE))
  {
    Serial.println("Cant connect to BH1750 Sensor");
    // while (1)
    //   ;
  }
  else
  {
    Serial.println("Init BH1750 Successful");
  }
  pinMode(RAIN_SENSOR_PIN, INPUT);
  pinMode(SOIL_SENSOR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);


  WiFiManager_Init();
  if (WiFiManager_IsConnected()) {
    Serial.println("Ready to run application");
  }
}

void loop()
{
  unsigned long currentMillis = millis();

  // 1. Đọc sensor định kỳ
  if (currentMillis - lastSensorReadTime >= SENSOR_READ_INTERVAL)
  {
    lastSensorReadTime = currentMillis;

    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    rainValue = analogRead(RAIN_SENSOR_PIN);
    soilValue = analogRead(SOIL_SENSOR_PIN);
    soilPercent = map(soilValue, 0, 4095, 100, 0);
    fluxValue = lightMeter.readLightLevel();

    Serial.println("----- Data -----");
    if (isnan(temperature) || isnan(humidity))
    {
      Serial.println(" DHT11 error");
    }
    else
    {
      Serial.printf(" Temperature: %.1f C\n", temperature);
      Serial.printf(" Humidity: %.0f %%\n", humidity);
    }
    Serial.printf(" Rain sensor: %d \n", rainValue);
    Serial.printf(" Soil moisture: %.0f %%\n", soilPercent);
    Serial.printf(" Light Intensity: %.0f lux\n", fluxValue);
    Serial.println("----------------------------");
  }

  // 2. Hiển thị LCD với tần số cao hơn
  // Thêm biến điều khiển trang hiển thị
  if (currentMillis - lastSwitchPageTime >= SWITCH_PAGE_INTERVAL)
  {

    lastSwitchPageTime = currentMillis;

    // Đổi trang sau mỗi lần cập nhật
    displayPage = !displayPage;
    lcd.clear(); // Xoá màn hình trước khi vẽ trang mới
  }

  if (currentMillis - lastLCDUpdateTime >= LCD_UPDATE_INTERVAL)
  {
    lastLCDUpdateTime = currentMillis;

    if (displayPage == 0)
    {
      // Trang 1: nhiệt độ, độ ẩm, đất, mưa
      if (isnan(temperature) || isnan(humidity))
      {
        lcd.setCursor(0, 0);
        lcd.print(" DHT11 error!");
      }
      else
      {
        lcd.setCursor(0, 0);
        lcd.print("T:");
        lcd.print(temperature, 1);
        lcd.print((char)223);
        lcd.print("C ");
        lcd.print("H:");
        lcd.print(humidity, 0);
        lcd.print("%");
      }

      lcd.setCursor(0, 1);
      lcd.print("HE: ");
      lcd.print(soilPercent, 0);
      lcd.print("% ");
      if (rainValue < RAIN_THRESHOLD)
      {
        lcd.print("R-ON ");
      }
      else
      {
        lcd.print("UR-OFF");
      }
    }
    else
    {
      lcd.setCursor(0, 0);
      lcd.print("Lux: ");
      lcd.print(fluxValue);
      lcd.print(" lx"); // Đơn vị gọn lại (lux → lx)

      lcd.setCursor(0, 1);
      lcd.print("Rain: ");
      lcd.print(rainValue);
    }
  }

  // 3. Điều khiển relay độc lập
  if (currentMillis - lastRelayControlTime >= RELAY_CONTROL_INTERVAL)
  {
    lastRelayControlTime = currentMillis;

    if (rainValue < RAIN_THRESHOLD)
    {
      digitalWrite(RELAY_PIN, HIGH);
    }
    else
    {
      digitalWrite(RELAY_PIN, LOW);
    }
  }
  
  WiFiManager_Loop();
}
