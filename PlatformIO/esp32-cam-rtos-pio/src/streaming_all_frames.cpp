#include "streaming.h"

#if defined (CAMERA_ALL_FRAMES)

#if defined(BENCHMARK)
#include <AverageFilter.h>
#define BENCHMARK_PRINT_INT 1000
#endif

// ==== RTOS task to grab frames from the camera =========================
void camCB(void* pvParameters) {

  TickType_t xLastWakeTime;

  //  A running interval associated with currently desired frame rate
  const TickType_t xFrequency = pdMS_TO_TICKS(1000 / FPS);

  frameNumber = 0;
  xLastWakeTime = xTaskGetTickCount();

#if defined(BENCHMARK)
    averageFilter<uint32_t> captureAvg(10);
    averageFilter<uint32_t> tickAvg(10);
    captureAvg.initialize();
    tickAvg.initialize();
    uint32_t lastPrintCam = millis();
    uint32_t benchmarkStart;
    uint32_t lastTick = millis();
#endif

  camera_fb_t* fb = NULL;

  for (;;) {

#if defined(BENCHMARK)
    benchmarkStart = micros();
#endif

    //  Grab a frame from the camera and allocate frame chunk for it
    fb = esp_camera_fb_get();
    if ( fb ) {
      frameChunck_t* f = (frameChunck_t*) allocateMemory(NULL, sizeof(frameChunck_t), OK_IF_OOM, PSRAM_ONLY);
      if ( f ) {
        // char* d = (char*) ps_malloc( fb->len );
        char* d = (char*) allocateMemory(NULL, fb->len, OK_IF_OOM, PSRAM_ONLY); 
        if ( d == NULL ) {
          free (f);
        }
        else {
          if ( fstFrame == NULL ) {
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
          // Log.verbose("Captured frame# %d\n", frameNumber);
          frameNumber++;
        }
      }
      else {
        Log.error("camCB: error allocating memory for frame %d - OOM\n", frameNumber);
        vTaskDelay(1000);
      }
      esp_camera_fb_return(fb);
    }
    else {
      Log.error("camCB: error capturing image for frame %d\n", frameNumber);
      vTaskDelay(1000);
    }

#if defined(BENCHMARK)
    captureAvg.value(micros()-benchmarkStart);
#endif

    if ( xTaskDelayUntil(&xLastWakeTime, xFrequency) != pdTRUE ) taskYIELD();

#if defined(BENCHMARK)
    tickAvg.value(millis()-lastTick);
    lastTick = millis();
#endif

    if ( noActiveClients == 0 ) {
      // we need to drain the cache if there are no more clients connected
      Log.trace("mjpegCB: All clients disconneted\n");
      while ( fstFrame !=NULL && fstFrame->nxt ) {
        frameChunck_t* f = (frameChunck_t*) fstFrame->nxt;
        free ( fstFrame->dat );
        free ( fstFrame );
        fstFrame = f;
      }
      fstFrame = NULL;
      curFrame = NULL;
      Log.verbose("mjpegCB: free heap           : %d\n", ESP.getFreeHeap());
      Log.verbose("mjpegCB: min free heap       : %d\n", ESP.getMinFreeHeap());
      Log.verbose("mjpegCB: max alloc free heap : %d\n", ESP.getMaxAllocHeap());
      Log.verbose("mjpegCB: free psram          : %d\n", ESP.getFreePsram());
      Log.verbose("mjpegCB: tCam stack wtrmark  : %d\n", uxTaskGetStackHighWaterMark(tCam));
      vTaskSuspend(NULL);  // passing NULL means "suspend yourself"
    }

#if defined(BENCHMARK)
    if ( millis() - lastPrintCam > BENCHMARK_PRINT_INT ) {
      lastPrintCam = millis();
      Log.verbose("mjpegCB: average frame capture time: %d us (tick avg=%d)\n", captureAvg.currentValue(), tickAvg.currentValue() );
    }
#endif

  }
}


// ==== Handle connection request from clients ===============================
void handleJPGSstream(void)
{
  if ( noActiveClients >= MAX_CLIENTS ) return;
  Log.verbose("handleJPGSstream start: free heap  : %d\n", ESP.getFreeHeap());

  streamInfo_t* info = (streamInfo_t*) malloc( sizeof(streamInfo_t) );
  if ( info == NULL ) {
    Log.error("handleJPGSstream: cannot allocate stream info - OOM\n");
    return;
  }
  info->client = new WiFiClient;
  if ( info->client == NULL ) {
    Log.error("handleJPGSstream: cannot allocate WiFi client for streaming - OOM\n");
    free(info);
    return;
  }

  *(info->client) = server.client();

  //  Creating task to push the stream to all connected clients
  int rc = xTaskCreatePinnedToCore(
             streamCB,
             "streamCB",
             3 * 1024,
             (void*) info,
             2,
             &info->task,
             APP_CPU);
  if ( rc != pdPASS ) {
    Log.error("handleJPGSstream: error creating RTOS task. rc = %d\n", rc);
    Log.error("handleJPGSstream: free heap  : %d\n", ESP.getFreeHeap());
    delete info;
  }

  xSemaphoreTake( frameSync, portMAX_DELAY );
  noActiveClients++;
  xSemaphoreGive( frameSync );

  // Wake up streaming tasks, if they were previously suspended:
  if ( eTaskGetState( tCam ) == eSuspended ) vTaskResume( tCam );
}


// ==== Actually stream content to all connected clients ========================
void streamCB(void * pvParameters) {
  char buf[16];
  TickType_t xLastWakeTime;
  TickType_t xFrequency;

  frameChunck_t* myFrame = fstFrame;

  streamInfo_t* info = (streamInfo_t*) pvParameters;

  //  Immediately send this client a header
  info->client->write(HEADER, hdrLen);
  info->client->write(BOUNDARY, bdrLen);

  xLastWakeTime = xTaskGetTickCount();
  xFrequency = pdMS_TO_TICKS(1000 / FPS);

  Log.trace("streamCB: Client connected\n");

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
    if ( myFrame ) {

#if defined (BENCHMARK)
        frameAvg.value( myFrame->siz );
        streamStart = micros();
#endif        

      if ( info->client->connected() ) {
        sprintf(buf, "%d\r\n\r\n", myFrame->siz);
        info->client->write(CTNTTYPE, cntLen);
        info->client->write(buf, strlen(buf));
        info->client->write((char*) myFrame->dat, (size_t)myFrame->siz);
        info->client->write(BOUNDARY, bdrLen);
        // Log.verbose("streamCB: Served frame# %d\n", fstFrame->fnm);
      }

#if defined (BENCHMARK)
        streamAvg.value(micros()-streamStart);
        fpsAvg.value(1000.0 / (float) (millis()-lastFrame) );
        lastFrame = millis();
        streamStart = micros();
#endif

      xSemaphoreTake( frameSync, portMAX_DELAY );

#if defined (BENCHMARK)
        waitAvg.value(micros()-streamStart);
#endif

      myFrame->cnt++;
      frameChunck_t* myNextFrame = (frameChunck_t*) myFrame->nxt; // this maybe NULL - that's ok

      // if serving first frame in the chain, check if we are the last serving task and it needs to be deleted
      if ( myFrame == fstFrame && fstFrame->cnt >= noActiveClients ) {
        free ( fstFrame->dat );
        fstFrame->dat = NULL;
        free ( fstFrame );
        fstFrame = myNextFrame;
      }
      xSemaphoreGive( frameSync );

      myFrame = myNextFrame;
    }
    else {
      myFrame = fstFrame;
    }

    if ( !info->client->connected() ) {
      //  client disconnected - clean up.
      xSemaphoreTake( frameSync, portMAX_DELAY );
      noActiveClients--;
      xSemaphoreGive( frameSync );

      Log.verbose("streamCB: Stream Task stack wtrmark  : %d\n", uxTaskGetStackHighWaterMark(info->task));
      info->client->stop();
      delete info->client;
      info->client = NULL;
      free( info );
      info = NULL;
      Log.trace("streamCB: Client disconnected\n");
      vTaskDelay(100);
      vTaskDelete(NULL);
    }
    //  Let other tasks run after serving every client
    if ( xTaskDelayUntil(&xLastWakeTime, xFrequency) != pdTRUE ) taskYIELD();

#if defined (BENCHMARK)
    if ( millis() - lastPrint > BENCHMARK_PRINT_INT ) {
      lastPrint = millis();
      Log.verbose("streamCB: wait avg=%d, stream avg=%d us, frame avg size=%d bytes, fps=%S\n", waitAvg.currentValue(), streamAvg.currentValue(), frameAvg.currentValue(), String(fpsAvg.currentValue()));
      if ( fstFrame ) Log.verbose("streamCB: current frame: %d, first frame:%d\n", curFrame->fnm, fstFrame->fnm);
    }
#endif
  }
}

#endif