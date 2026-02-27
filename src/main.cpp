#include <WiFi.h>
#include <WebServer.h> // Добавили стандартный сервер
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "web_page.h"

WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);

// Переменные для хранения текущего состояния (для веб-морды)
bool currentLed = false;
int currentMosfet = 0;

void handleRoot() {
    server.send(200, "text/html", INDEX_HTML);
}

void handleData() {
    StaticJsonDocument<64> doc;
    doc["led"] = currentLed;
    doc["mosfet"] = currentMosfet;
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
}

void callback(char* topic, byte* payload, unsigned int length) {
    char msg[length + 1];
    memcpy(msg, payload, length);
    msg[length] = '\0';

    if (strcmp(topic, TOPIC_CMD) == 0) {
        StaticJsonDocument<128> doc;
        if (deserializeJson(doc, payload, length) == DeserializationError::Ok) {
            if (doc.containsKey("led")) {
                currentLed = doc["led"];
                digitalWrite(LED_PIN, currentLed ? HIGH : LOW);
                client.publish(TOPIC_LED_STATE, currentLed ? "ON" : "OFF", true);
            }
            if (doc.containsKey("mosfet")) {
                currentMosfet = constrain((int)doc["mosfet"], 0, 255);
                ledcWrite(PWM_CHANNEL, currentMosfet);
            }
        }
    } 
    else if (strcmp(topic, TOPIC_LED_SET) == 0) {
        currentLed = (strcmp(msg, "ON") == 0);
        digitalWrite(LED_PIN, currentLed ? HIGH : LOW);
        client.publish(TOPIC_LED_STATE, msg, true);
    } 
    else if (strcmp(topic, TOPIC_MOSFET_SET) == 0) {
        currentMosfet = constrain(atoi(msg), 0, 255);
        ledcWrite(PWM_CHANNEL, currentMosfet);
    }
}

// ... Функция reconnect остается такой же как в прошлом ответе ...
void reconnect() {
    static unsigned long lastAttempt = 0;
    if (!client.connected() && millis() - lastAttempt > 5000) {
        lastAttempt = millis();
        if (client.connect(CLIENT_ID, mqtt_user, mqtt_pass, TOPIC_STATUS, 1, true, "offline")) {
            client.publish(TOPIC_STATUS, "online", true);
            client.subscribe(TOPIC_CMD);
            client.subscribe(TOPIC_LED_SET);
            client.subscribe(TOPIC_MOSFET_SET);
        }
    }
}


void setup() {
    pinMode(LED_PIN, OUTPUT);
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RES);
    ledcAttachPin(MOSFET_PIN, PWM_CHANNEL);

    WiFi.config(local_IP, gateway, subnet, dns);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) delay(500);

    // Настройка веб-сервера
    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.begin();

    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}

void loop() {
    if (!client.connected()) reconnect();
    client.loop();
    server.handleClient(); // Обработка веб-запросов
}
