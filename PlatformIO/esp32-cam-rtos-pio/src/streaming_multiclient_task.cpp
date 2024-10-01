#include "streaming.h"

#if defined(CAMERA_MULTICLIENT_TASK)

volatile size_t   camSize;    // size of the current frame, byte
volatile char*    camBuf;      // pointer to the current frame

#if defined(BENCHMARK)
#include <AverageFilter.h>
#define BENCHMARK_PRINT_INT 1000
averageFilter<uint32_t> captureAvg(10);
uint32_t lastPrintCam = millis();
#endif

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
    size_t s = 0;
    //  Grab a frame from the camera and query its size
    camera_fb_t* fb = NULL;

#if defined(BENCHMARK)
    uint32_t benchmarkStart = micros();
#endif

    s = 0;
    fb = esp_camera_fb_get();
    if ( fb ) {
      s = fb->len;

      //  If frame size is more that we have previously allocated - request  125% of the current frame space
      if (s > fSize[ifb]) {
        fSize[ifb] = s + s/4;
        fbs[ifb] = allocateMemory(fbs[ifb], fSize[ifb], FAIL_IF_OOM, ANY_MEMORY);
      }

      //  Copy current frame into local buffer
      char* b = (char *)fb->buf;
      memcpy(fbs[ifb], b, s);
      esp_camera_fb_return(fb);
    }
    else {
      Log.error("camCB: error capturing image for frame %d\n", frameNumber);
      vTaskDelay(1000);
    }

#if defined(BENCHMARK)
    captureAvg.value(micros()-benchmarkStart);
#endif

    //  Only switch frames around if no frame is currently being streamed to a client
    //  Wait on a semaphore until client operation completes
    //    xSemaphoreTake( frameSync, portMAX_DELAY );

    //  Do not allow frame copying while switching the current frame
    // if ( xSemaphoreTake( frameSync, xFrequency ) ) {
    if ( xSemaphoreTake( frameSync, portMAX_DELAY ) ) {
      camBuf = fbs[ifb];
      camSize = s;
      ifb++;
      ifb &= 1;  // this should produce 1, 0, 1, 0, 1 ... sequence
      frameNumber++;
      //  Let anyone waiting for a frame know that the frame is ready
      xSemaphoreGive( frameSync );
    }

    //  Let other (streaming) tasks run
    if ( xTaskDelayUntil(&xLastWakeTime, xFrequency) != pdTRUE ) taskYIELD();

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

#if defined(BENCHMARK)
    if ( millis() - lastPrintCam > BENCHMARK_PRINT_INT ) {
      lastPrintCam = millis();
      Log.verbose("mjpegCB: average frame capture time: %d microseconds\n", captureAvg.currentValue() );
    }
#endif


  }
}


// ==== Handle connection request from clients ===============================
void handleJPGSstream(void)
{
  if ( noActiveClients >= MAX_CLIENTS ) return;
  Log.verbose("handleJPGSstream start: free heap  : %d\n", ESP.getFreeHeap());

  streamInfo_t* info = new streamInfo_t;
  if ( info == NULL ) {
    Log.error("handleJPGSstream: cannot allocate stream info - OOM\n");
    return;
  }

  WiFiClient* client = new WiFiClient();
  if ( client == NULL ) {
    Log.error("handleJPGSstream: cannot allocate WiFi client for streaming - OOM\n");
    free(info);
    return;
  }

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

#if defined(BENCHMARK)
  averageFilter<int32_t> streamAvg(10);
  averageFilter<int32_t> waitAvg(10);
  averageFilter<uint32_t> frameAvg(10);
  averageFilter<float> fpsAvg(10);
  uint32_t streamStart = 0;
  streamAvg.initialize();
  waitAvg.initialize();
  frameAvg.initialize();
  fpsAvg.initialize();
  uint32_t lastPrint = millis();
  uint32_t lastFrame = millis();
#endif

  for (;;) {
    //  Only send anything if there is someone watching
    if ( info->client->connected() ) {

      if ( info->frame != frameNumber) { // do not send same frame twice

#if defined (BENCHMARK)
        streamStart = micros();
#endif        

        xSemaphoreTake( frameSync, portMAX_DELAY );
        size_t currentSize = camSize;

#if defined (BENCHMARK)
        waitAvg.value(micros()-streamStart);
        frameAvg.value(currentSize);
        streamStart = micros();
#endif


//  ======================== OPTION1 ==================================
//  server a copy of the buffer - overhead: have to allocate and copy a buffer for all frames

// /*
        if ( info->buffer == NULL ) {
          info->buffer = allocateMemory (info->buffer, camSize, FAIL_IF_OOM, ANY_MEMORY);
          info->len = camSize;
        }
        else {
          if ( camSize > info->len ) {
            info->buffer = allocateMemory (info->buffer, camSize, FAIL_IF_OOM, ANY_MEMORY);
            info->len = camSize;
          }
        }
        memcpy(info->buffer, (const void*) camBuf, camSize);
        
        xSemaphoreGive( frameSync );

        sprintf(buf, "%d\r\n\r\n", currentSize);
        info->client->flush();
        info->client->write(CTNTTYPE, cntLen);
        info->client->write(buf, strlen(buf));
        info->client->write((char*) info->buffer, currentSize);
        info->client->write(BOUNDARY, bdrLen);
// */

//  ======================== OPTION2 ==================================
//  just server the comman buffer protected by mutex
/*
        sprintf(buf, "%d\r\n\r\n", camSize);
        info->client->write(CTNTTYPE, cntLen);
        info->client->write(buf, strlen(buf));
        info->client->write((char*) camBuf, (size_t)camSize);

        xSemaphoreGive( frameSync );

        info->client->write(BOUNDARY, bdrLen);
*/

//  ====================================================================
        info->frame = frameNumber;
#if defined (BENCHMARK)
          streamAvg.value(micros()-streamStart);
#endif        
      }
    }
    else {
      //  client disconnected - clean up.
      noActiveClients--;
      Log.verbose("streamCB: Stream Task stack wtrmark  : %d\n", uxTaskGetStackHighWaterMark(info->task));
      info->client->stop();
      if ( info->buffer ) {
        free( info->buffer );
        info->buffer = NULL;
      }
      delete info->client;
      delete info;
      info = NULL;
      Log.trace("streamCB: Client disconnected\n");
      vTaskDelay(100);
      vTaskDelete(NULL);
    }
    //  Let other tasks run after serving every client
    if ( xTaskDelayUntil(&xLastWakeTime, xFrequency) != pdTRUE ) taskYIELD();

#if defined (BENCHMARK)
    fpsAvg.value(1000.0 / (float) (millis()-lastFrame) );
    lastFrame = millis();

    if ( millis() - lastPrint > BENCHMARK_PRINT_INT ) {
      lastPrint = millis();
      Log.verbose("streamCB: wait avg=%d, stream avg=%d us, frame avg size=%d bytes, fps=%S\n", waitAvg.currentValue(), streamAvg.currentValue(), frameAvg.currentValue(), String(fpsAvg.currentValue()));
    }
#endif
  }
}

#endif
