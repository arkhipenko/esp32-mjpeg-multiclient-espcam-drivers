/*

  This is a MJPEG streaming webserver implemented for AI-Thinker ESP32-CAM
  and ESP-EYE modules.
  This is tested to work with VLC and Blynk video widget and can support up to 10
  simultaneously connected streaming clients.
  Simultaneous streaming is implemented with FreeRTOS tools: queue and tasks.

  Inspired by and based on this Instructable: $9 RTSP Video Streamer Using the ESP32-CAM Board
  (https://www.instructables.com/id/9-RTSP-Video-Streamer-Using-the-ESP32-CAM-Board/)

*/

#include "definitions.h"
#include "references.h"


#include "credentials.h"
#include "streaming.h"

const char *c_ssid = AP_SSID;
const char *c_pwd = AP_PWD;

// Camera models overview:
// https://randomnerdtutorials.com/esp32-cam-camera-pin-gpios/


#include "camera_pins.h"

WebServer server(80);

// ===== rtos task handles =========================
// Streaming is implemented with tasks:
TaskHandle_t tMjpeg;            // handles client connections to the webserver
TaskHandle_t tCam;              // handles getting picture frames from the camera and storing them locally
TaskHandle_t tStream;

uint8_t      noActiveClients;   // number of active clients

// frameSync semaphore is used to prevent streaming buffer as it is replaced with the next frame
SemaphoreHandle_t frameSync = NULL;


// ==== SETUP method ==================================================================
void setup() {

  // Setup Serial connection:
  Serial.begin(SERIAL_RATE);
  delay(500); // wait for a bit to let Serial connect

  setupLogging();

  Log.trace("\n\nMulti-client MJPEG Server\n");
  Log.trace("setup: total heap  : %d\n", ESP.getHeapSize());
  Log.trace("setup: free heap   : %d\n", ESP.getFreeHeap());
  Log.trace("setup: total psram : %d\n", ESP.getPsramSize());
  Log.trace("setup: free psram  : %d\n", ESP.getFreePsram());

  static camera_config_t camera_config = {
    .pin_pwdn       = PWDN_GPIO_NUM,
    .pin_reset      = RESET_GPIO_NUM,
    .pin_xclk       = XCLK_GPIO_NUM,
    .pin_sscb_sda   = SIOD_GPIO_NUM,
    .pin_sscb_scl   = SIOC_GPIO_NUM,
    .pin_d7         = Y9_GPIO_NUM,
    .pin_d6         = Y8_GPIO_NUM,
    .pin_d5         = Y7_GPIO_NUM,
    .pin_d4         = Y6_GPIO_NUM,
    .pin_d3         = Y5_GPIO_NUM,
    .pin_d2         = Y4_GPIO_NUM,
    .pin_d1         = Y3_GPIO_NUM,
    .pin_d0         = Y2_GPIO_NUM,
    .pin_vsync      = VSYNC_GPIO_NUM,
    .pin_href       = HREF_GPIO_NUM,
    .pin_pclk       = PCLK_GPIO_NUM,

    .xclk_freq_hz   = XCLK_FREQ,
    .ledc_timer     = LEDC_TIMER_0,
    .ledc_channel   = LEDC_CHANNEL_0,
    .pixel_format   = PIXFORMAT_JPEG,
    .frame_size     = FRAME_SIZE,
    .jpeg_quality   = JPEG_QUALITY,
    .fb_count       = 2,
    .fb_location = CAMERA_FB_IN_DRAM,
    .grab_mode = CAMERA_GRAB_LATEST,
    // .sccb_i2c_port = -1
  };

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  if (esp_camera_init(&camera_config) != ESP_OK) {
    Log.fatal("setup: Error initializing the camera\n");
    delay(10000);
    ESP.restart();
  }

#if defined (FLIP_VERTICALLY)
  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, true);
#endif

  //  Configure and connect to WiFi
  char apname[65];
  char passwd[65];
  IPAddress ip;
  WiFiManager wm;


  WiFi.setAutoConnect(true);

  std::vector<const char *> menu = {"wifi","restart","exit"};
  wm.setMenu(menu);

  wm.setConfigPortalTimeout(180); // seconds
  wm.setConnectTimeout(45);
  wm.setConnectRetries(3);

  strcpy(apname, c_ssid);
  strcpy(passwd, c_pwd);
  if ( !wm.autoConnect((const char*) apname, (const char*) passwd) ) { // password protected ap
    Log.fatal("setup: Unble to connect to WiFI - restarting\n");
    delay(5000);
    ESP.restart();
  }

  ip = WiFi.localIP();
  Log.verbose(F("setup: WiFi connected\n"));
  // Log.verbose("Stream Link: http://%S/mjpeg/1\n\n", ip.toString());
  Serial.printf("Stream Link: http://%s%s\n\n", ip.toString().c_str(), STREAMING_URL);

  // Start main streaming RTOS task
  xTaskCreatePinnedToCore(
    mjpegCB,
    "mjpeg",
    3 * KILOBYTE,
    NULL,
    tskIDLE_PRIORITY + 2,
    &tMjpeg,
    PRO_CPU);

  Log.trace("setup complete: free heap  : %d\n", ESP.getFreeHeap());
}

void loop() {
  vTaskDelete(NULL);
}
