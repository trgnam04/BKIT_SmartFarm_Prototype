#include "mqtt_service.h"

// --- BIẾN NỘI BỘ CỦA MODULE MQTT ---
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

// --- KHAI BÁO HÀM NỘI BỘ ---
void callback(char *topic, byte *payload, unsigned int length);
void reconnect();

// --- ĐỊNH NGHĨA CÁC HÀM PUBLIC ---
void mqtt_init() {
    mqtt_client.setServer(MQTT_HOST, MQTT_PORT);
    mqtt_client.setCallback(callback);
    Serial.println("MQTT Service initialized.");
}

void mqtt_loop() {
    if (!mqtt_client.connected()) {
        reconnect();
    }
    mqtt_client.loop(); // Phải được gọi thường xuyên
}

bool mqtt_is_connected() {
    return mqtt_client.connected();
}

// --- ĐỊNH NGHĨA CÁC HÀM NỘI BỘ ---
void callback(char* topic, byte* payload, unsigned int length) {
    Serial.println("--- MQTT Message Received ---");
    
    // Tạo một chuỗi từ payload nhận được
    String message;
    message.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.println("Topic: " + String(topic));
    Serial.println("Payload: " + message);

    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, message);

    if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
    }

    // Xử lý logic, nhưng thay vì trực tiếp điều khiển,
    // nó sẽ cập nhật vào cấu trúc dùng chung `pumpControl`
    if (doc.containsKey("command") && strcmp(doc["command"], "PUMP_ON") == 0 && doc.containsKey("duration")) {
        unsigned long duration = doc["duration"];
        if (duration > 0) {
            Serial.printf("Received PUMP_ON command. Duration: %lu ms.\n", duration);
            
            // Cập nhật trạng thái điều khiển, file main.cpp sẽ xử lý việc bật relay
            pumpControl.isRunning = true;
            pumpControl.stopTime = millis() + duration;

            // Gửi ACK
            StaticJsonDocument<100> ackDoc;
            ackDoc["status"] = "ACK";
            ackDoc["command_received"] = "PUMP_ON";
            String ackPayload;
            serializeJson(ackDoc, ackPayload);
            mqtt_client.publish(MQTT_TOPIC_PUB, ackPayload.c_str());
        }
    }
}

void reconnect() {
    while (!mqtt_client.connected()) {
        Serial.print("Attempting MQTT connection... ");
        String clientId = "ESP32Client-" + String(random(0xffff), HEX);

        if (mqtt_client.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
            Serial.println("connected!");
            mqtt_client.subscribe(MQTT_TOPIC_SUB);
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqtt_client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}