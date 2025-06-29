// Thêm include cho service mới
#include "common.h"
#include "mqtt_service.h"
#include "lcd_service.h"
#include "sensor_service.h" 
#include "wifi_manager.h" 


// --- BIẾN THỜI GIAN CỦA MAIN LOOP ---
// Đổi tên biến cho rõ ràng hơn
uint32_t tLastSensorUpdate = 0; 
uint32_t tLastHttpSend = 0;

// --------- ĐỊNH NGHĨA BIẾN VÀ ĐỐI TƯỢNG TOÀN CỤC ---------
SensorData sensorData;
PumpControl pumpControl;
LcdState lcdState = WIFI_STATUS; // Trạng thái LCD ban đầu

// Định nghĩa các đối tượng phần cứng
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, 16, 2);
BH1750 lightMeter;
DHT dht(DHTPIN, DHTTYPE);

// Định nghĩa biến POST_URL
const char *POST_URL = "http://ems.thebestits.vn/Value/GetData";

bool sendDataToServer();

// ========= SETUP =========
void setup() {
    Serial.begin(115200);
    Wire.begin(21, 22);
    
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    WiFiManager_Init();

    // Khởi tạo các dịch vụ
    lcd_init();
    sensor_init(); 
    mqtt_init();
}

// ========= LOOP =========
void loop() {
    uint32_t now = millis();

    // 1. Luôn chạy các hàm loop của dịch vụ
    WiFiManager_Loop();
    mqtt_loop();
    lcd_update();

    // 2. Cập nhật dữ liệu cảm biến theo chu kỳ (tác vụ chính)
    // Logic này giờ đây chỉ gọi một hàm duy nhất!
    if (now - tLastSensorUpdate >= T_SENSOR_READ) {
        tLastSensorUpdate = now;
        sensor_update(); //
    }

    // 3. Gửi dữ liệu lên server theo chu kỳ (tác vụ chính)
    if (now - tLastHttpSend >= T_HTTP_SEND) {
        if (WiFi.status() == WL_CONNECTED) {
            if (sendDataToServer()) { // Hàm sendDataToServer vẫn nằm ở đây
                tLastHttpSend = now;
            }
        }
    }

    // 4. Xử lý điều khiển máy bơm (tác vụ chính)
    // Logic này không thay đổi, nó tự động sử dụng dữ liệu đã được lọc
    if (pumpControl.isRunning) {
        digitalWrite(RELAY_PIN, HIGH);
        if (now >= pumpControl.stopTime) {
            Serial.println("Pump timer finished. Turning pump OFF.");
            digitalWrite(RELAY_PIN, LOW);
            pumpControl.isRunning = false;
        }
    }
}

// Hàm sendDataToServer vẫn giữ lại ở file main
bool sendDataToServer() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[HTTP] WiFi not connected. Aborting.");
        return false;
    }

    // --- 1. Chuẩn bị chuỗi JSON chứa dữ liệu cảm biến ---
    StaticJsonDocument<256> doc;
    // Dữ liệu trong `sensorData` đã được lọc sẵn từ sensor_service
    doc["temperature"] = String(sensorData.temp, 2);       // Lấy 2 chữ số thập phân
    doc["humidity"] = String(sensorData.hum, 2);           // Lấy 2 chữ số thập phân
    doc["soilPercent"] = String(sensorData.soilPct, 2);    // Lấy 2 chữ số thập phân
    doc["lux"] = String(sensorData.lux, 2);                // Lấy 2 chữ số thập phân    
    doc["rainValue"] = sensorData.rainRaw;    

    String jsonData;
    serializeJson(doc, jsonData);
    Serial.print("[HTTP] JSON data to be sent: ");
    Serial.println(jsonData);

    // --- 2. Xây dựng URL động với các tham số ---    
    String url = String(POST_URL); // Tên biến POST_URL hơi gây nhầm lẫn, nhưng ta cứ dùng tạm
    url += "?stationcode=K13_TEST";
    url += "&data=" + jsonData;

    Serial.print("[HTTP] Request URL: ");
    Serial.println(url);

    // --- 3. Thực hiện yêu cầu GET ---
    HTTPClient http;
    http.begin(url); // Bắt đầu với URL đã có đủ tham số

    // TÙY CHỌN: Nếu server yêu cầu Basic Authentication, hãy bỏ comment dòng dưới
    // http.setAuthorization("bkitk13", "123456");

    Serial.println("[HTTP] Sending GET request...");
    int httpCode = http.GET();

    // --- 4. Xử lý kết quả ---
    if (httpCode > 0) {
        String payload = http.getString();
        Serial.printf("[HTTP] GET... response code: %d\n", httpCode);
        Serial.println("[HTTP] Response payload: " + payload);

        // Server thường trả về mã 200 (OK) nếu thành công
        if (httpCode == HTTP_CODE_OK) {
            // Kiểm tra nội dung trả về từ server nếu cần
            // Ví dụ: if (payload.indexOf("SUCCESS") != -1) { ... }
        }
    } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return (httpCode == HTTP_CODE_OK);
}
