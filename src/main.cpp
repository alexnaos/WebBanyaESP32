#include <Arduino.h>
#include "secrets.h" // Подключаем наши секреты
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <PubSubClient.h>
#include <GyverDS18.h>
#include <ArduinoJson.h> // Нужно установить через Менеджер библиотек

const char *cmd_topic = "home/ESP32_Sloboda/cmd"; // Топик для JSON сценариев

uint64_t addr = 0x6A000000BC84BF28;

// Теперь используем макросы вместо открытого текста:
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASS;
const char *mqtt_user = MQTT_USER;
const char *mqtt_pass = MQTT_PASS;
const char *mqtt_server = MQTT_HOST;
IPAddress local_IP(192, 168, 1, 104); // Желаемый IP
IPAddress gateway(192, 168, 1, 1);    // IP роутера
IPAddress subnet(255, 255, 255, 0);   // Маска подсети
IPAddress dns(8, 8, 8, 8);            // DNS (например, Google)

const char *client_id = "ESP32_Sloboda";                      // уникальное имя клиента
const char *status_topic = "home/ESP32_Sloboda/status";       // Топик для статуса
const char *will_msg = "offline";                             // Предсмертная записка
const int will_qos = 1;                                       // Качество доставки
const bool will_retain = true;                                // Брокер запомнит статус
const char *led_set_topic = "home/ESP32_Sloboda/led/set";     // Для команд
const char *led_state_topic = "home/ESP32_Sloboda/led/state"; // Для статуса
// Новый топик для слайдера
const char *mosfet_set_topic = "home/ESP32_Sloboda/mosfet/set";
const int ledPin = 2;     // Пин для ВКЛ/ВЫКЛ
const int mosfetPin = 12; // Пин для мосфета (ШИМ)

// Параметры ШИМ
const int freq = 15000;
const int pwmChannel = 0;
const int resolution = 8; // 0-255

WiFiServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_BMP280 bmp;
GyverDS18 ds(14); // пин


void callback(char *topic, byte *payload, unsigned int length)
{
  // --- Оставляем вашу старую логику для обычных топиков ---
  char msg[length + 1];
  memcpy(msg, payload, length);
  msg[length] = '\0';

  if (strcmp(topic, led_set_topic) == 0) {
    if (strcmp(msg, "ON") == 0) digitalWrite(ledPin, HIGH);
    else if (strcmp(msg, "OFF") == 0) digitalWrite(ledPin, LOW);
    client.publish(led_state_topic, msg, true);
    return; // Выходим, если это был обычный топик
  }

  // --- НОВАЯ ЛОГИКА ДЛЯ JSON ---
  if (strcmp(topic, cmd_topic) == 0) {
    StaticJsonDocument<256> doc; // Резервируем память под JSON
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
      Serial.print("JSON Error: ");
      Serial.println(error.c_str());
      return;
    }

    // Пример выполнения сценария: {"led": 1, "mosfet": 128}
    
    // 1. Управление LED
    if (doc.containsKey("led")) {
      int state = doc["led"]; // Примет 0 или 1
      digitalWrite(ledPin, state);
      client.publish(led_state_topic, state ? "ON" : "OFF", true);
    }

    // 2. Управление мосфетом
    if (doc.containsKey("mosfet")) {
      int val = doc["mosfet"]; // Примет значение 0-255
      if (val >= 0 && val <= 255) {
        ledcWrite(pwmChannel, val);
      }
    }
  }
}



void setup()
{
  Serial.begin(115200);
  pinMode(2, OUTPUT);
  pinMode(ledPin, OUTPUT);
  ledcSetup(pwmChannel, freq, resolution);
  ledcAttachPin(mosfetPin, pwmChannel);

  client.setCallback(callback);

  // Инициализация датчика
  ds.requestTemp();
  Serial.println("DS18B20: Запрос температуры...");

  Wire.begin(21, 22);
  if (!bmp.begin(0x76))
  {
    Serial.println("BMP280 error!");
    // Не вешайте систему намертво while(1), лучше просто пометьте ошибку
  }

  // Пока ESP32 подключается к WiFi (это занимает 2-5 секунд),
  // датчик DS18B20 успеет спокойно провести измерение.
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // 2. Настройка статики
  if (!WiFi.config(local_IP, gateway, subnet, dns))
  {
    Serial.println("STA Failed to configure");
  }

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP()); // Проверьте, что выдал именно 150

  // Теперь, когда прошло время, можно прочитать данные в Serial
  if (ds.readTemp(addr))
  {
    Serial.print("TDLS Initial: ");
    Serial.println(ds.getTemp());
  }
  else
  {
    Serial.println("DS18B20 error on startup!");
  }

  server.begin();
  client.setServer(mqtt_server, 1883);
}

// 1. Неблокирующий реконнект
void reconnect()
{
  static unsigned long lastReconnectAttempt = 0;
  if (!client.connected())
  {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000)
    {
      lastReconnectAttempt = now;
      if (client.connect(client_id, mqtt_user, mqtt_pass, status_topic, will_qos, will_retain, will_msg))
      {
        client.subscribe(led_set_topic);
        client.publish(status_topic, "online", true);
        client.subscribe(mosfet_set_topic);
        client.subscribe(cmd_topic);

      }
    }
  }
}

unsigned long lastMsg = 0;

void loop()
{
  reconnect();
  client.loop();
  unsigned long now = millis();

  // 1. Считываем данные и отправляем их (раз в 5 секунд)
  if (now - lastMsg > 5000)
  {
    lastMsg = now;

    // К этому моменту прошло 5 секунд с прошлого запроса, данные ТОЧНО готовы
    float t = bmp.readTemperature();
    float p = bmp.readPressure() / 100.0F;

    // Читаем данные, которые запрашивали 5 секунд назад
    float tdls = -127;
    if (ds.readTemp(addr))
    { // Используем ваш адрес addr из начала скетча
      tdls = ds.getTemp();
    }

    char buf[10];
    dtostrf(t, 4, 2, buf);
    client.publish("home/ESP32_Sloboda/temp", buf);

    dtostrf(p, 4, 2, buf);
    client.publish("home/ESP32_Sloboda/press", buf);

    if (tdls != -127)
    {
      dtostrf(tdls, 4, 2, buf);
      client.publish("home/ESP32_Sloboda/tempDLS", buf);
      Serial.print("DS18B20 Temp: ");
      Serial.println(tdls);
    }

    // 2. СРАЗУ запрашиваем новое измерение для СЛЕДУЮЩЕГО цикла (через 5 сек)
    ds.requestTemp();
  }

  // Веб-сервер...
}
