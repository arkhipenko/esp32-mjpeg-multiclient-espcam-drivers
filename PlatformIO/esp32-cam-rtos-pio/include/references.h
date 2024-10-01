#pragma once
#include <Arduino.h>
#include <ArduinoLog.h>
#include <WebServer.h>
#include "logging.h"

#include "esp_camera.h"
#include "ov2640.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WifiManager.h>
#include <vector>

#include <esp_wifi.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

extern SemaphoreHandle_t frameSync;
extern WebServer server;
extern TaskHandle_t tMjpeg;   // handles client connections to the webserver
extern TaskHandle_t tCam;     // handles getting picture frames from the camera and storing them locally
extern TaskHandle_t tStream;
extern uint8_t      noActiveClients;       // number of active clients

extern const char*  STREAMING_URL;
