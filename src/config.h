#ifndef CONFIG_H
#define CONFIG_H
#include <Arduino.h>
#include "secrets.h"
// Сетевые настройки (берутся из secrets.h и ваших данных)
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;
const char* mqtt_user = MQTT_USER;
const char* mqtt_pass = MQTT_PASS;
const char* mqtt_server = MQTT_HOST;

const IPAddress local_IP(192, 168, 1, 115);
const IPAddress gateway(192, 168, 1, 1);
const IPAddress subnet(255, 255, 255, 0);
const IPAddress dns(8, 8, 8, 8);

// Пины
const int LED_PIN = 3; // 4снизу справа!
//const int MOSFET_PIN = 3; // 4снизу справа!
//const int MOSFET_PIN = 4; // 5снизу справа нет сигнала
const int MOSFET_PIN = 25;



// Параметры ШИМ
const int PWM_FREQ = 15000;
const int PWM_CHANNEL = 0;
const int PWM_RES = 8;

// MQTT Топики
const char* CLIENT_ID = "ESP32_Malek";
const char* TOPIC_CMD = "home/ESP32 Malek/cmd";
const char* TOPIC_LED_SET = "home/ESP32 Malek/led/set";
const char* TOPIC_LED_STATE = "home/ESP32 Malek/led/state";
const char* TOPIC_MOSFET_SET = "home/ESP32 Malek/mosfet/set";
const char* TOPIC_STATUS = "home/ESP32 Malek/status";

#endif
