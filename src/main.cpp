#include <Arduino.h>
#include "secrets.h" // Подключаем наши секреты
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <PubSubClient.h>
#include <GyverDS18.h>

uint64_t addr = 0x6A000000BC84BF28;

// Теперь используем макросы вместо открытого текста:
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASS;
const char *mqtt_user = MQTT_USER;
const char *mqtt_pass = MQTT_PASS;
const char *mqtt_server = MQTT_HOST;

const char *client_id = "ESP32_Sloboda";                      // уникальное имя клиента
const char *status_topic = "home/ESP32_Sloboda/status";       // Топик для статуса
const char *will_msg = "offline";                             // Предсмертная записка
const int will_qos = 1;                                       // Качество доставки
const bool will_retain = true;                                // Брокер запомнит статус
const char *led_set_topic = "home/ESP32_Sloboda/led/set";     // Для команд
const char *led_state_topic = "home/ESP32_Sloboda/led/state"; // Для статуса
// Новый топик для слайдера
const char *mosfet_set_topic = "home/ESP32_Sloboda/mosfet/set";
const int ledPin = 2;    // Пин для ВКЛ/ВЫКЛ
const int mosfetPin = 4; // Пин для мосфета (ШИМ)

// Параметры ШИМ
const int freq = 5000;
const int pwmChannel = 0;
const int resolution = 8; // 0-255

WiFiServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_BMP280 bmp;
GyverDS18 ds(14); // пин

void callback(char *topic, byte *payload, unsigned int length) {
  // Создаем временную строку для обработки данных
  char msg[length + 1];
  memcpy(msg, payload, length);
  msg[length] = '\0';

  // 1. Управление обычным пином (ON/OFF)
  if (strcmp(topic, led_set_topic) == 0) {
    if (strcmp(msg, "ON") == 0) digitalWrite(ledPin, HIGH);
    else if (strcmp(msg, "OFF") == 0) digitalWrite(ledPin, LOW);
    client.publish(led_state_topic, msg, true);
  }

  // 2. Управление мосфетом через слайдер (0-255)
  if (strcmp(topic, mosfet_set_topic) == 0) {
    int brightness = atoi(msg); // Конвертируем текст в число
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
    
    ledcWrite(pwmChannel, brightness);
    // Опционально: подтверждаем получение значения
    // client.publish("home/ESP32_Sloboda/mosfet/state", msg, true);
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
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

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
