#include <Arduino.h>
#include "secrets.h" // Подключаем наши секреты
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <PubSubClient.h>
#include <GyverDS18.h>
#include <ArduinoJson.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

// Нужно установить через Менеджер библиотек
const char *cmd_topic = "home/ESP32_Sloboda/cmd"; // Топик для JSON сценариев
uint64_t addr = 0x6A000000BC84BF28;
// Теперь используем макросы вместо открытого текста:
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASS;
const char *mqtt_user = MQTT_USER;
const char *mqtt_pass = MQTT_PASS;
const char *mqtt_server = MQTT_HOST;
// IPAddress local_IP(192, 168, 1, 104); // Желаемый
IPAddress local_IP(192, 168, 1, 115); // Желаемый для маленькой платы
IPAddress gateway(192, 168, 1, 1);    // IP роутера
IPAddress subnet(255, 255, 255, 0);   // Маска подсети
IPAddress dns(8, 8, 8, 8);            // DNS (например, Google)

const char *client_id = "ESP32 Malek";                      // уникальное имя клиента
const char *status_topic = "home/ESP32 Malek/status";       // Топик для статуса
const char *will_msg = "offline";                             // Предсмертная записка
const int will_qos = 1;                                       // Качество доставки
const bool will_retain = true;                                // Брокер запомнит статус
const char *led_set_topic = "home/ESP32 Malek/led/set";     // Для команд
const char *led_state_topic = "home/ESP32 Malek/led/state"; // Для статуса
// Новый топик для слайдера
const char *mosfet_set_topic = "home/ESP32 Malek/mosfet/set";
const int ledPin = 2;     // Пин для ВКЛ/ВЫКЛ
const int mosfetPin = 12; // Пин для мосфета (ШИМ)

// Параметры ШИМ
const int freq = 15000;
const int pwmChannel = 0;
const int resolution = 8; // 0-255

// Переменные для хранения последних данных
float last_t = 0, last_p = 0, last_tdls = 0;
bool led_status = false;
int mosfet_value = 0;

WiFiServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_BMP280 bmp;
GyverDS18 ds(14); // пин

void callback(char *topic, byte *payload, unsigned int length)
{
  // 1. Подготовка сообщения
  char msg[length + 1];
  memcpy(msg, payload, length);
  msg[length] = '\0';

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(msg);

  // --- ЛОГИКА ДЛЯ JSON (Команда cmd) ---
  if (strcmp(topic, cmd_topic) == 0)
  {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error)
    {
      Serial.print("JSON Error: ");
      Serial.println(error.c_str());
      return;
    }

    // 1. Обработка LED из JSON
    if (doc.containsKey("led"))
    {
      int state = doc["led"]; // 1 или 0
      digitalWrite(ledPin, state ? HIGH : LOW);

      // Отправляем подтверждение в MQTT статус-топик
      client.publish(led_state_topic, state ? "ON" : "OFF", true);
    }

    // 2. Обработка MOSFET из JSON
    if (doc.containsKey("mosfet"))
    {
      int val = doc["mosfet"];

      // Ограничиваем значение (300 превратится в 255)
      mosfet_value = constrain(val, 0, 255);

      ledcWrite(pwmChannel, mosfet_value);

      // Опционально: отправляем подтверждение в топик состояния мосфета
      char buf[5];
      itoa(mosfet_value, buf, 10);
      client.publish("home/ESP32 Malek/mosfet/state", buf, true);
    }

    Serial.println("Command applied: LED and MOSFET updated");
    return;
  }

  // --- ЛОГИКА ДЛЯ ОТДЕЛЬНЫХ ТОПИКОВ (Текстовая) ---
  if (strcmp(topic, led_set_topic) == 0)
  {
    if (strcmp(msg, "ON") == 0)
      digitalWrite(ledPin, HIGH);
    else if (strcmp(msg, "OFF") == 0)
      digitalWrite(ledPin, LOW);

    client.publish(led_state_topic, msg, true);
  }

  if (strcmp(topic, mosfet_set_topic) == 0)
  {
    mosfet_value = atoi(msg); // ОБЯЗАТЕЛЬНО обновляем переменную для веб-страницы
    mosfet_value = constrain(mosfet_value, 0, 255);
    ledcWrite(pwmChannel, mosfet_value);
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

  SerialBT.begin("ESP32_Wemos_D1");
  Serial.println("Устройство готово к сопряжению!");

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

  // --- ОБРАБОТКА ВЕБ-СЕРВЕРА (Мониторинг) ---
  WiFiClient webClient = server.available();
  if (webClient)
  {
    String currentLine = "";
    unsigned long timeout = millis() + 2000;
    while (webClient.connected() && millis() < timeout)
    {
      if (webClient.available())
      {
        char c = webClient.read();
        if (c == '\n')
        {
          if (currentLine.length() == 0)
          {
            webClient.println("HTTP/1.1 200 OK");
            webClient.println("Content-type:text/html");
            webClient.println();
            webClient.println("<html><head><meta charset='UTF-8'><meta http-equiv='refresh' content='3'></head>");
            webClient.println("<body style='font-family:sans-serif; text-align:center;'>");
            webClient.println("<h1>ESP32 Status (from Orange)</h1>");

            // Показываем текущее состояние пинов (то, что пришло по MQTT)
            webClient.print("<p><b>LED Status:</b> ");
            webClient.println(digitalRead(ledPin) ? "<span style='color:green'>ON</span>" : "<span style='color:red'>OFF</span>");
            webClient.println("</p><p><b>Mosfet Level:</b> ");
            webClient.println(mosfet_value); // Значение из переменной, обновленной в callback

            webClient.println("</p><hr><p>Temp BMP: " + String(last_t) + " °C</p>");
            webClient.println("<p>Temp DS18: " + String(last_tdls) + " °C</p>");
            webClient.println("<p>Pressure: " + String(last_p) + " hPa</p>");
            webClient.println("</body></html>");
            break;
          }
          else
          {
            currentLine = "";
          }
        }
        else if (c != '\r')
        {
          currentLine += c;
        }
      }
    }
    webClient.stop();
  }

  // --- ОТПРАВКА MQTT (Сенсоры) ---
  unsigned long now = millis();
  if (now - lastMsg > 5000)
  {
    lastMsg = now;
    last_t = bmp.readTemperature();
    last_p = bmp.readPressure() / 100.0F;
    if (ds.readTemp(addr))
      last_tdls = ds.getTemp();

    char buf[10];
    dtostrf(last_t, 4, 2, buf);
    client.publish("home/ESP32 Malek/temp", buf);
    dtostrf(last_p, 4, 2, buf);
    client.publish("home/ESP32 Malek/press", buf);

    if (last_tdls != -127.00)
    {
      dtostrf(last_tdls, 4, 2, buf);
      client.publish("home/ESP32 Malek/tempDLS", buf);
    }
    ds.requestTemp();
  }
}
