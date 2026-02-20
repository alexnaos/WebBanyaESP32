#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <PubSubClient.h>

// --- Настройки ---
const char *ssid = "Sloboda100";
const char *password = "2716192023";
const char *mqtt_server = "192.168.1.23"; // IP твоего малинки/компа с MQTT
const char *mqtt_user = "admin";
const char *mqtt_pass = "2716192023";
const char *client_id = "ESP32_Sloboda"; // уникальное имя клиента

const char *status_topic = "home/ESP32_Sloboda/status";       // Топик для статуса
const char *will_msg = "offline";                             // Предсмертная записка
const int will_qos = 1;                                       // Качество доставки
const bool will_retain = true;                                // Брокер запомнит статус
const char *led_set_topic = "home/ESP32_Sloboda/led/set";     // Для команд
const char *led_state_topic = "home/ESP32_Sloboda/led/state"; // Для статуса

WiFiServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_BMP280 bmp;

void callback(char *topic, byte *payload, unsigned int length)
{
  String msg;
  for (int i = 0; i < length; i++)
    msg += (char)payload[i];

  if (String(topic) == led_set_topic)
  {
    if (msg == "ON")
    {
      digitalWrite(2, HIGH);
      client.publish(led_state_topic, "ON", true); // Подтверждаем включение с флагом Retain
    }
    else if (msg == "OFF")
    {
      digitalWrite(2, LOW);
      client.publish(led_state_topic, "OFF", true); // Подтверждаем выключение
    }
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(2, OUTPUT); // Встроенный светодиод на большинстве ESP32
  client.setCallback(callback);
  Wire.begin(21, 22);
  if (!bmp.begin(0x76))
  {
    Serial.println("Sensor error!");
    while (1)
      ;
  }

  // WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nIP: " + WiFi.localIP().toString());

  server.begin();
  client.setServer(mqtt_server, 1883);
}

void reconnect()
{
  while (!client.connected())
  {
    Serial.print("Connecting to MQTT...");

    // connect(id, user, pass, willTopic, willQos, willRetain, willMessage)
    if (client.connect(client_id, mqtt_user, mqtt_pass, status_topic, will_qos, will_retain, will_msg))
    {
      client.subscribe("home/ESP32_Sloboda/led"); // Подписываемся на команды
      client.publish(status_topic, "online", true);
      Serial.println("connected");

      // Сразу после подключения публикуем, что мы в сети
      client.publish(status_topic, "online", true);

      // Публикуем данные (теперь с именем устройства в пути)
      // client.publish("home/ESP32_Sloboda/temp", ...);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}
unsigned long lastMsg = 0;

void loop()
{
  if (!client.connected())
    reconnect();
  client.loop();

  float t = bmp.readTemperature();
  float p = bmp.readPressure() / 100.0F;

  // Отправка в MQTT каждые 5 секунд
  if (millis() - lastMsg > 5000)
  {
    lastMsg = millis();
    // Используем переменную client_id в пути
    client.publish("home/ESP32_Sloboda/temp", String(t).c_str());
    client.publish("home/ESP32_Sloboda/press", String(p).c_str());
  }

  // Простой Web-сервер
  WiFiClient webClient = server.available();
  if (webClient)
  {
    String response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
    response += "<h1>ESP32 Weather</h1>";
    response += "<p>Temp: " + String(t) + " C</p>";
    response += "<p>Press: " + String(p) + " hPa</p>";
    webClient.print(response);
    webClient.stop();
  }
}