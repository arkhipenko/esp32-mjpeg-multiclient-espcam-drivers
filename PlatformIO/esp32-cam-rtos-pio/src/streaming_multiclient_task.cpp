#include "streaming.h"

#if defined(CAMERA_MULTICLIENT_TASK)

volatile size_t   camSize;    // size of the current frame, byte
volatile char*    camBuf;      // pointer to the current frame


void camCB(void* pvParameters) {

  TickType_t xLastWakeTime;

  //  A running interval associated with currently desired frame rate
  const TickType_t xFrequency = pdMS_TO_TICKS(1000 / FPS);

  //  Pointers to the 2 frames, their respective sizes and index of the current frame
  char* fbs[2] = { NULL, NULL };
  size_t fSize[2] = { 0, 0 };
  int ifb = 0;
  frameNumber = 0;

  //=== loop() section  ===================
  xLastWakeTime = xTaskGetTickCount();

  for (;;) {

    //  Grab a frame from the camera and query its size
    camera_fb_t* fb = NULL;

    fb = esp_camera_fb_get();
    size_t s = fb->len;

    //  If frame size is more that we have previously allocated - request  125% of the current frame space
    if (s > fSize[ifb]) {
      fSize[ifb] = s + s/4;
      fbs[ifb] = allocateMemory(fbs[ifb], fSize[ifb]);
    }

    //  Copy current frame into local buffer
    char* b = (char *)fb->buf;
    memcpy(fbs[ifb], b, s);
    esp_camera_fb_return(fb);

    //  Let other tasks run and wait until the end of the current frame rate interval (if any time left)
    // taskYIELD();
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    //  Only switch frames around if no frame is currently being streamed to a client
    //  Wait on a semaphore until client operation completes
    //    xSemaphoreTake( frameSync, portMAX_DELAY );

    //  Do not allow frame copying while switching the current frame
    xSemaphoreTake( frameSync, xFrequency );
    camBuf = fbs[ifb];
    camSize = s;
    ifb++;
    ifb &= 1;  // this should produce 1, 0, 1, 0, 1 ... sequence
    frameNumber++;
    //  Let anyone waiting for a frame know that the frame is ready
    xSemaphoreGive( frameSync );

    //  Immediately let other (streaming) tasks run
    taskYIELD();

    //  If streaming task has suspended itself (no active clients to stream to)
    //  there is no need to grab frames from the camera. We can save some juice
    //  by suspedning the tasks
    if ( noActiveClients == 0 ) {
      Log.verbose("mjpegCB: free heap           : %d\n", ESP.getFreeHeap());
      Log.verbose("mjpegCB: min free heap)      : %d\n", ESP.getMinFreeHeap());
      Log.verbose("mjpegCB: max alloc free heap : %d\n", ESP.getMaxAllocHeap());
      Log.verbose("mjpegCB: tCam stack wtrmark  : %d\n", uxTaskGetStackHighWaterMark(tCam));
      vTaskSuspend(NULL);  // passing NULL means "suspend yourself"
    }
  }
}


// ==== Handle connection request from clients ===============================
void handleJPGSstream(void)
{
  if ( noActiveClients >= MAX_CLIENTS ) return;
  Log.verbose("handleJPGSstream start: free heap  : %d\n", ESP.getFreeHeap());

  streamInfo_t* info = new streamInfo_t;

  WiFiClient* client = new WiFiClient();
  *client = server.client();

  info->frame = frameNumber - 1;
  info->client = client;
  info->buffer = NULL;
  info->len = 0;

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
    Log.error("handleJPGSstream: error creating RTOS task. rc = %d\n", rc);
    Log.error("handleJPGSstream: free heap  : %d\n", ESP.getFreeHeap());
    //    Log.error("stk high wm: %d\n", uxTaskGetStackHighWaterMark(tSend));
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

  streamInfo_t* info = (streamInfo_t*) pvParameters;

  if ( info == NULL ) {
    Log.fatal("streamCB: a NULL pointer passed");
    delay(5000);
    ESP.restart();
  }

  
  xLastWakeTime = xTaskGetTickCount();
  xFrequency = pdMS_TO_TICKS(1000 / FPS);
  Log.trace("streamCB: Client Connected\n");

  //  Immediately send this client a header
  info->client->write(HEADER, hdrLen);
  info->client->write(BOUNDARY, bdrLen);

  for (;;) {
    //  Only bother to send anything if there is someone watching
    if ( info->client->connected() ) {

      if ( info->frame != frameNumber) {
        xSemaphoreTake( frameSync, portMAX_DELAY );
        if ( info->buffer == NULL ) {
          info->buffer = allocateMemory (info->buffer, camSize);
          info->len = camSize;
        }
        else {
          if ( camSize > info->len ) {
            info->buffer = allocateMemory (info->buffer, camSize);
            info->len = camSize;
          }
        }
        memcpy(info->buffer, (const void*) camBuf, info->len);
        xSemaphoreGive( frameSync );

        info->frame = frameNumber;
        info->client->write(CTNTTYPE, cntLen);
        sprintf(buf, "%d\r\n\r\n", info->len);
        info->client->write(buf, strlen(buf));
        info->client->write((char*) info->buffer, (size_t)info->len);
        info->client->write(BOUNDARY, bdrLen);
      }
    }
    else {
      //  client disconnected - clean up.
      noActiveClients--;
      Log.trace("streamCB: Client disconnected\n");
      Log.verbose("streamCB: Stream Task stack wtrmark  : %d\n", uxTaskGetStackHighWaterMark(info->task));
      info->client->stop();
      if ( info->buffer ) {
        free( info->buffer );
        info->buffer = NULL;
      }
      delete info->client;
      delete info;
      info = NULL;
      vTaskDelete(NULL);
    }
    //  Let other tasks run after serving every client
    if ( !xTaskDelayUntil(&xLastWakeTime, xFrequency) ) taskYIELD();
  }
}

#endif
