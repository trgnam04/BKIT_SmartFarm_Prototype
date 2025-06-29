#include "sensor_service.h"

// --- BIẾN NỘI BỘ (STATIC) CỦA MODULE ---
// Biến static đảm bảo chúng chỉ được truy cập bên trong file này.
// Chúng ta lưu các giá trị đã được lọc để dùng trong lần tính toán tiếp theo.
static float filteredLux = 0.0f;
static float filteredSoilPct = 0.0f;
static float filteredRain = 0.0f;

// --- KHAI BÁO CÁC HÀM TIỆN ÍCH NỘI BỘ ---
static bool read_dht_sensor(float &t, float &h);
static uint16_t read_analog_avg(uint8_t pin, uint8_t samples = 8);


// --- ĐỊNH NGHĨA CÁC HÀM PUBLIC ---

void sensor_init() {
    dht.begin();
    lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
    Serial.println("Sensor Service initialized.");

    // --- Khởi tạo giá trị ban đầu cho bộ lọc ---
    // Đọc giá trị lần đầu để bộ lọc có điểm bắt đầu, tránh khởi động từ 0.
    delay(100); // Chờ cảm biến ổn định một chút
    read_dht_sensor(sensorData.temp, sensorData.hum);
    
    // Gán trực tiếp giá trị đọc lần đầu cho các biến lọc và dữ liệu chung
    filteredLux = lightMeter.readLightLevel();
    sensorData.lux = filteredLux;

    uint16_t rawSoil = read_analog_avg(SOIL_SENSOR_PIN);
    filteredSoilPct = map(rawSoil, 0, 4095, 100, 0);
    sensorData.soilPct = filteredSoilPct;

    uint16_t rawRain = read_analog_avg(RAIN_SENSOR_PIN);
    filteredRain = rawRain;
    sensorData.rainRaw = filteredRain;

    Serial.println("Initial sensor readings complete. Filters are primed.");
}

void sensor_update() {
    // --- 1. ĐỌC GIÁ TRỊ THÔ TỪ CẢM BIẾN ---
    float rawTemp, rawHum, rawLux;
    uint16_t rawSoil, rawRain;

    read_dht_sensor(rawTemp, rawHum);
    rawLux = lightMeter.readLightLevel();
    rawSoil = read_analog_avg(SOIL_SENSOR_PIN);
    rawRain = read_analog_avg(RAIN_SENSOR_PIN);

    // --- 2. ÁP DỤNG BỘ LỌC EMA VÀ CẬP NHẬT DỮ LIỆU ---

    // Đối với DHT, nếu đọc được giá trị hợp lệ thì mới cập nhật
    if (!isnan(rawTemp) && !isnan(rawHum)) {
        // Có thể áp dụng EMA cho temp/hum nếu muốn, nhưng chúng vốn đã khá ổn định
        sensorData.temp = rawTemp;
        sensorData.hum = rawHum;
    }

    // Áp dụng bộ lọc EMA cho các cảm biến còn lại
    filteredLux = (SENSOR_EMA_ALPHA * rawLux) + (1.0f - SENSOR_EMA_ALPHA) * filteredLux;
    sensorData.lux = filteredLux;

    float rawSoilPct = map(rawSoil, 0, 4095, 100, 0);
    filteredSoilPct = (SENSOR_EMA_ALPHA * rawSoilPct) + (1.0f - SENSOR_EMA_ALPHA) * filteredSoilPct;
    sensorData.soilPct = filteredSoilPct;

    // Đối với cảm biến mưa, chúng ta có thể muốn giữ giá trị thô để phát hiện nhanh
    // Nhưng lọc cũng giúp tránh báo động giả. Ở đây ta vẫn lọc.
    filteredRain = (SENSOR_EMA_ALPHA * rawRain) + (1.0f - SENSOR_EMA_ALPHA) * filteredRain;
    sensorData.rainRaw = (uint16_t)filteredRain;


    // In ra để debug (so sánh giá trị thô và giá trị đã lọc)
    Serial.printf("[SensorSvc] Soil: raw=%.0f%%, filtered=%.0f%% | Lux: raw=%.0f, filtered=%.0f\n",
                  rawSoilPct, sensorData.soilPct, rawLux, sensorData.lux);
}


// --- ĐỊNH NGHĨA CÁC HÀM TIỆN ÍCH NỘI BỘ ---

// Hàm đọc DHT, có thử lại 1 lần nếu thất bại
static bool read_dht_sensor(float &t, float &h) {
    t = dht.readTemperature();
    h = dht.readHumidity();
    if (isnan(t) || isnan(h)) {
        delay(50); // Chờ một chút trước khi đọc lại
        t = dht.readTemperature();
        h = dht.readHumidity();
    }
    return !(isnan(t) || isnan(h));
}

// Hàm đọc trung bình giá trị analog để giảm nhiễu tức thời
static uint16_t read_analog_avg(uint8_t pin, uint8_t samples) {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < samples; ++i) {
        sum += analogRead(pin);
        delayMicroseconds(150);
    }
    return sum / samples;
}