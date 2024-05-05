#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <Firebase_ESP_Client.h>
#include <LoRa.h>
#include <SPI.h>

#include "addons/RTDBHelper.h"
#include "addons/TokenHelper.h"

#define XSTR(x) STR(x)
#define STR(x) #x

#define WIFI_SSID XSTR(ENV_WIFI_SSID)
#define WIFI_PWD XSTR(ENV_WIFI_PWD)
#define FIREBASE_API_KEY XSTR(ENV_FIREBASE_API_KEY)
#define FIREBASE_DB_ENDPOINT XSTR(ENV_FIREBASE_DB_ENDPOINT)
#define FIREBASE_USER_EMAIL XSTR(ENV_FIREBASE_USER_EMAIL)
#define FIREBASE_USER_PWD XSTR(ENV_FIREBASE_USER_PWD)
