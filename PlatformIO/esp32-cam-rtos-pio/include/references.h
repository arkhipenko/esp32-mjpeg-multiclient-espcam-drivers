#pragma once
#include <Arduino.h>
#include <ArduinoLog.h>
#include <WebServer.h>

extern SemaphoreHandle_t frameSync;
extern WebServer server;
extern TaskHandle_t tMjpeg;   // handles client connections to the webserver
extern TaskHandle_t tCam;     // handles getting picture frames from the camera and storing them locally
extern TaskHandle_t tStream;
extern uint8_t      noActiveClients;       // number of active clients

void handleNotFound();