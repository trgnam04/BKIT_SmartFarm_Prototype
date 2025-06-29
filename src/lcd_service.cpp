#include "lcd_service.h"

// --- BIẾN NỘI BỘ MODULE LCD ---
static uint32_t tLastLcdUpdate = 0;
static uint32_t tLastPageSwitch = 0;

// --- ĐỊNH NGHĨA HÀM ---
void lcd_init() {
    lcd.begin(16, 2);
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("System Starting");
    Serial.println("LCD Service initialized.");
}

void lcd_update() {
    uint32_t now = millis();

    // Tự động chuyển trang sau một khoảng thời gian
    if (now - tLastPageSwitch >= T_PAGE_SWITCH) {
        tLastPageSwitch = now;
        // Chuyển sang trạng thái tiếp theo
        switch(lcdState) {
            case WIFI_STATUS:   lcdState = SENSOR_PAGE_1; break;
            case SENSOR_PAGE_1: lcdState = SENSOR_PAGE_2; break;
            case SENSOR_PAGE_2: lcdState = WIFI_STATUS;   break;
        }
        lcd.clear(); // Xóa màn hình khi chuyển trang
    }

    // Cập nhật nội dung hiển thị dựa trên trạng thái hiện tại
    if (now - tLastLcdUpdate >= T_LCD_UPDATE) {
        tLastLcdUpdate = now;
        
        switch (lcdState) {
            case WIFI_STATUS:
                lcd.setCursor(0, 0);
                lcd.print("WiFi Status:");
                lcd.setCursor(0, 1);
                if (WiFi.status() == WL_CONNECTED) {
                    lcd.print(WiFi.localIP());
                    lcd.print(" "); // Xóa phần còn lại
                } else {
                    lcd.print("Connecting... ");
                }
                break;

            case SENSOR_PAGE_1:
                lcd.setCursor(0, 0);
                if (isnan(sensorData.temp) || isnan(sensorData.hum)) {
                    lcd.print("DHT21 Error   ");
                } else {
                    lcd.printf("T:%.1fC H:%.0f%%  ", sensorData.temp, sensorData.hum);
                }
                lcd.setCursor(0, 1);
                lcd.printf("Soil: %02.0f%% P:%s", sensorData.soilPct, pumpControl.isRunning ? "ON " : "OFF");
                break;

            case SENSOR_PAGE_2:
                lcd.setCursor(0, 0);
                lcd.printf("Lux: %05.0f lx  ", sensorData.lux);
                lcd.setCursor(0, 1);
                lcd.printf("Rain: %4u    ", sensorData.rainRaw);
                break;
        }
    }
}