/*

  This is a simple MJPEG streaming webserver implemented for AI-Thinker ESP32-CAM
  and ESP-EYE modules.
  This is tested to work with VLC and Blynk video widget and can support up to 10
  simultaneously connected streaming clients.
  Simultaneous streaming is implemented with dedicated FreeRTOS tasks.

  Inspired by and based on this Instructable: $9 RTSP Video Streamer Using the ESP32-CAM Board
  (https://www.instructables.com/id/9-RTSP-Video-Streamer-Using-the-ESP32-CAM-Board/)

  Board: AI-Thinker ESP32-CAM or ESP-EYE
  Compile as:
   ESP32 Dev Module
   CPU Freq: 240
   Flash Freq: 80
   Flash mode: QIO
   Flash Size: 4Mb
   Partrition: Minimal SPIFFS
   PSRAM: Enabled
*/

// ESP32 has two cores: APPlication core and PROcess core (the one that runs ESP32 SDK stack)
#define APP_CPU 1
#define PRO_CPU 0

#include "esp_camera.h"
#include "ov2640.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>

#include <esp_bt.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT
#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
//#define CAMERA_MODEL_AI_THINKER

#define MAX_CLIENTS 10

#include "camera_pins.h"

/*
  Next one is an include with wifi credentials.
  This is what you need to do:

  1. Create a file called "home_wifi_multi.h" in the same folder   OR   under a separate subfolder of the "libraries" folder of Arduino IDE. (You are creating a "fake" library really - I called it "MySettings").
  2. Place the following text in the file:
  #define SSID1 "replace with your wifi ssid"
  #define PWD1 "replace your wifi password"
  3. Save.

  Should work then
*/
#include "home_wifi_multi.h"

//OV2640 cam;

WebServer server(80);

// ===== rtos task handles =========================
// Streaming is implemented with 3 tasks:
TaskHandle_t tMjpeg;   // handles client connections to the webserver
TaskHandle_t tCam;     // handles getting picture frames from the camera and storing them locally

uint8_t       noActiveClients;       // number of active clients

// frameSync semaphore is used to prevent streaming buffer as it is replaced with the next frame
SemaphoreHandle_t frameSync = NULL;

// We will try to achieve FPS frame rate
const int FPS = 10;

// We will handle web client requests every 100 ms (10 Hz)
const int WSINTERVAL = 100;


// ======== Server Connection Handler Task ==========================
void mjpegCB(void* pvParameters) {
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = pdMS_TO_TICKS(WSINTERVAL);

  // Creating frame synchronization semaphore and initializing it
  frameSync = xSemaphoreCreateBinary();
  xSemaphoreGive( frameSync );

  //=== setup section  ==================

  //  Creating RTOS task for grabbing frames from the camera
  xTaskCreatePinnedToCore(
    camCB,        // callback
    "cam",        // name
    4 * 1024,       // stacj size
    NULL,         // parameters
    2,            // priority
    &tCam,        // RTOS task handle
    PRO_CPU);     // core

  //  Registering webserver handling routines
  server.on("/mjpeg/1", HTTP_GET, handleJPGSstream);
  server.onNotFound(handleNotFound);

  //  Starting webserver
  server.begin();

  noActiveClients = 0;

  Serial.printf("mjpegCB: free heap (start)  : %d\n", ESP.getFreeHeap());

  xLastWakeTime = xTaskGetTickCount();

  //  int ticker = 0;
  for (;;) {
    server.handleClient();

    //  After every server client handling request, we let other tasks run and then pause
    taskYIELD();
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    //    if ( (ticker++ % 10) == 0 ) Serial.printf("mjpegCB: main loop tick\n");
  }
}


// Current frame information
volatile uint32_t frameNumber;
//volatile size_t   camSize;    // size of the current frame, byte
//volatile char*    camBuf;      // pointer to the current frame


struct frameChunck {
  uint8_t   cnt;  // served to clients counter. when equal to number of active clients, could be deleted
  uint32_t* nxt;  // next chunck
  uint32_t  fnm;  // frame number
  uint32_t  siz;  // frame size
  uint8_t*  dat;  // frame pointer
};

frameChunck* fstFrame;  // first frame
frameChunck* curFrame;  // current frame being captured by the camera


// ==== RTOS task to grab frames from the camera =========================
void camCB(void* pvParameters) {

  TickType_t xLastWakeTime;

  //  A running interval associated with currently desired frame rate
  const TickType_t xFrequency = pdMS_TO_TICKS(1000 / FPS);

  frameNumber = 0;

  xLastWakeTime = xTaskGetTickCount();

  for (;;) {

    camera_fb_t* fb = NULL;

    //  Grab a frame from the camera and allocate frame chunk for it
    fb = esp_camera_fb_get();
    frameChunck* f = (frameChunck*) ps_malloc( sizeof(frameChunck) );
    if ( f  ) {
      char* d = (char*) ps_malloc( fb->len );
      if ( d == NULL ) {
        free (f);
      }
      else {
        if ( frameNumber == 0 ) {
          fstFrame = f;
        }
        f->dat = (uint8_t*) d;
        f->nxt = NULL;
        f->siz = fb->len;
        f->cnt = 0;
        memcpy(f->dat, (char *)fb->buf, fb->len);
        f->fnm = frameNumber;
        if ( curFrame ) {
          curFrame->nxt = (uint32_t*) f;
        }
        curFrame = f;
        //        Serial.printf("Captured frame# %d\n", frameNumber);
        frameNumber++;
      }
    }
    esp_camera_fb_return(fb);

    taskYIELD();
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    if ( noActiveClients == 0 ) {
      // we need to drain the cache if there are no more clients connected
      while ( fstFrame->nxt ) {
        frameChunck* f = (frameChunck*) fstFrame->nxt;
        free ( fstFrame->dat );
        free ( fstFrame );
        fstFrame = f;
      }
      Serial.printf("mjpegCB: free heap           : %d\n", ESP.getFreeHeap());
      Serial.printf("mjpegCB: min free heap)      : %d\n", ESP.getMinFreeHeap());
      Serial.printf("mjpegCB: max alloc free heap : %d\n", ESP.getMaxAllocHeap());
      Serial.printf("mjpegCB: tCam stack wtrmark  : %d\n", uxTaskGetStackHighWaterMark(tCam));
      Serial.flush();
      vTaskSuspend(NULL);  // passing NULL means "suspend yourself"
    }
  }
}


// ==== STREAMING ======================================================
const char HEADER[] = "HTTP/1.1 200 OK\r\n" \
                      "Access-Control-Allow-Origin: *\r\n" \
                      "Content-Type: multipart/x-mixed-replace; boundary=123456789000000000000987654321\r\n";
const char BOUNDARY[] = "\r\n--123456789000000000000987654321\r\n";
const char CTNTTYPE[] = "Content-Type: image/jpeg\r\nContent-Length: ";
const int hdrLen = strlen(HEADER);
const int bdrLen = strlen(BOUNDARY);
const int cntLen = strlen(CTNTTYPE);


struct streamInfo {
  WiFiClient      *client;
  TaskHandle_t    task;
};

// ==== Handle connection request from clients ===============================
void handleJPGSstream(void)
{
  if ( noActiveClients >= MAX_CLIENTS ) return;
  Serial.printf("handleJPGSstream start: free heap  : %d\n", ESP.getFreeHeap());

  streamInfo* info = (streamInfo*) malloc( sizeof(streamInfo) );
  info->client = new WiFiClient;
  *(info->client) = server.client();

  //  Creating task to push the stream to all connected clients
  int rc = xTaskCreatePinnedToCore(
             streamCB,
             "strmCB",
             3 * 1024,
             (void*) info,
             2,
             &info->task,
             APP_CPU);
  if ( rc != pdPASS ) {
    Serial.printf("handleJPGSstream: error creating RTOS task. rc = %d\n", rc);
    Serial.printf("handleJPGSstream: free heap  : %d\n", ESP.getFreeHeap());
    //    Serial.printf("stk high wm: %d\n", uxTaskGetStackHighWaterMark(tSend));
    delete info;
  }

  noActiveClients++;

  // Wake up streaming tasks, if they were previously suspended:
  if ( eTaskGetState( tCam ) == eSuspended ) vTaskResume( tCam );
}


// ==== Actually stream content to all connected clients ========================
void streamCB(void * pvParameters) {
  char buf[16];
  TickType_t xLastWakeTime;
  TickType_t xFrequency;
  frameChunck* myFrame = fstFrame;
  portMUX_TYPE xSemaphore = portMUX_INITIALIZER_UNLOCKED;

  streamInfo* info = (streamInfo*) pvParameters;

  if ( info == NULL ) {
    Serial.println("streamCB: a NULL pointer passed");
  }
  //  Immediately send this client a header
  info->client->write(HEADER, hdrLen);
  info->client->write(BOUNDARY, bdrLen);
  taskYIELD();

  xLastWakeTime = xTaskGetTickCount();
  xFrequency = pdMS_TO_TICKS(1000 / FPS);

  for (;;) {
    //  Only bother to send anything if there is someone watching
    if ( info->client->connected() ) {
      if ( myFrame ) {

        info->client->write(CTNTTYPE, cntLen);
        sprintf(buf, "%d\r\n\r\n", fstFrame->siz);
        info->client->write(buf, strlen(buf));
        info->client->write((char*) fstFrame->dat, (size_t)fstFrame->siz);
        info->client->write(BOUNDARY, bdrLen);
        info->client->flush();

        //        Serial.printf("Served frame# %d\n", fstFrame->fnm);


        if ( myFrame->nxt ) {
          frameChunck* f;
          f = (frameChunck*) myFrame->nxt;

          portENTER_CRITICAL(&xSemaphore);
          if ( ++myFrame->cnt == noActiveClients ) {
            assert(myFrame == fstFrame);
            free ( fstFrame->dat );
            fstFrame->dat = NULL;
            free ( fstFrame );
            fstFrame = f;
          }
          portEXIT_CRITICAL(&xSemaphore);
          myFrame = f;
        }
      }
      else {
        myFrame = fstFrame;
      }
    }
    else {
      //  client disconnected - clean up.
      noActiveClients--;
      Serial.printf("streamCB: Stream Task stack wtrmark  : %d\n", uxTaskGetStackHighWaterMark(info->task));
      Serial.flush();
      info->client->flush();
      info->client->stop();
      delete info->client;
      info->client = NULL;
      free( info );
      info = NULL;
      vTaskDelete(NULL);
    }
    //  Let other tasks run after serving every client
    taskYIELD();
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}



// ==== Handle invalid URL requests ============================================
void handleNotFound()
{
  String message = "Server is running!\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  server.send(200, "text / plain", message);
}



// ==== SETUP method ==================================================================
void setup()
{

  // Setup Serial connection:
  Serial.begin(115200);
  delay(1000); // wait for a second to let Serial connect
  Serial.printf("setup: free heap  : %d\n", ESP.getFreeHeap());

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

    .xclk_freq_hz   = 20000000,
    .ledc_timer     = LEDC_TIMER_0,
    .ledc_channel   = LEDC_CHANNEL_0,
    .pixel_format   = PIXFORMAT_JPEG,
    /*
        FRAMESIZE_96X96,    // 96x96
        FRAMESIZE_QQVGA,    // 160x120
        FRAMESIZE_QCIF,     // 176x144
        FRAMESIZE_HQVGA,    // 240x176
        FRAMESIZE_240X240,  // 240x240
        FRAMESIZE_QVGA,     // 320x240
        FRAMESIZE_CIF,      // 400x296
        FRAMESIZE_HVGA,     // 480x320
        FRAMESIZE_VGA,      // 640x480
        FRAMESIZE_SVGA,     // 800x600
        FRAMESIZE_XGA,      // 1024x768
        FRAMESIZE_HD,       // 1280x720
        FRAMESIZE_SXGA,     // 1280x1024
        FRAMESIZE_UXGA,     // 1600x1200
    */
    //    .frame_size     = FRAMESIZE_QVGA,
    //    .frame_size     = FRAMESIZE_UXGA,
    //    .frame_size     = FRAMESIZE_SVGA,
    //    .frame_size     = FRAMESIZE_VGA,
    .frame_size     = FRAMESIZE_HD,
    //    .frame_size     = FRAMESIZE_UXGA,
    .jpeg_quality   = 24,
    .fb_count       = 2
  };

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  if (esp_camera_init(&camera_config) != ESP_OK) {
    Serial.println("Error initializing the camera");
    delay(10000);
    ESP.restart();
  }

  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, true);

  //  Configure and connect to WiFi
  IPAddress ip;

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID1, PWD1);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(F("."));
  }
  ip = WiFi.localIP();
  Serial.println(F("WiFi connected"));
  Serial.println("");
  Serial.print("\nStream Link: http://");
  Serial.print(ip);
  Serial.println("/mjpeg/1\n\n");


  // Start mainstreaming RTOS task
  xTaskCreatePinnedToCore(
    mjpegCB,
    "mjpeg",
    3 * 1024,
    NULL,
    2,
    &tMjpeg,
    APP_CPU);

  Serial.printf("setup complete: free heap  : %d\n", ESP.getFreeHeap());
}

void loop() {
  vTaskDelay(1000);
}
