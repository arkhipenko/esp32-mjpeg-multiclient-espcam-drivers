#include "streaming.h"

#if defined (CAMERA_ALL_FRAMES)


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
//    frameChunck_t* f = (frameChunck_t*) ps_malloc( sizeof(frameChunck_t) );
    frameChunck_t* f = (frameChunck_t*) allocateMemory(NULL, sizeof(frameChunck_t), true);
    if ( f  ) {
      // char* d = (char*) ps_malloc( fb->len );
      char* d = (char*) allocateMemory(NULL, fb->len, true); 
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
        // Log.verbose("Captured frame# %d\n", frameNumber);
        frameNumber++;
      }
    }
    esp_camera_fb_return(fb);

    if ( !xTaskDelayUntil(&xLastWakeTime, xFrequency) ) taskYIELD();

    if ( noActiveClients == 0 ) {
      // we need to drain the cache if there are no more clients connected
      Log.trace("mjpegCB: All clients disconneted\n");
      while ( fstFrame->nxt ) {
        frameChunck_t* f = (frameChunck_t*) fstFrame->nxt;
        free ( fstFrame->dat );
        free ( fstFrame );
        fstFrame = f;
      }
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

  streamInfo_t* info = (streamInfo_t*) malloc( sizeof(streamInfo_t) );
  info->client = new WiFiClient;
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

  noActiveClients++;

  // Wake up streaming tasks, if they were previously suspended:
  if ( eTaskGetState( tCam ) == eSuspended ) vTaskResume( tCam );
}


// ==== Actually stream content to all connected clients ========================
void streamCB(void * pvParameters) {
  char buf[16];
  TickType_t xLastWakeTime;
  TickType_t xFrequency;
  frameChunck_t* myFrame = fstFrame;
  portMUX_TYPE xSemaphore = portMUX_INITIALIZER_UNLOCKED;

  streamInfo_t* info = (streamInfo_t*) pvParameters;

  if ( info == NULL ) {
    Log.fatal("streamCB: a NULL pointer passed\n");
    delay(5000);
    ESP.restart();
  }
  //  Immediately send this client a header
  info->client->write(HEADER, hdrLen);
  info->client->write(BOUNDARY, bdrLen);

  xLastWakeTime = xTaskGetTickCount();
  xFrequency = pdMS_TO_TICKS(1000 / FPS);

  Log.trace("streamCB: Client connected\n");
  for (;;) {
    //  Only bother to send anything if there is someone watching
    if ( info->client->connected() ) {
      if ( myFrame ) {

        info->client->write(CTNTTYPE, cntLen);
        sprintf(buf, "%d\r\n\r\n", fstFrame->siz);
        info->client->write(buf, strlen(buf));
        info->client->write((char*) fstFrame->dat, (size_t)fstFrame->siz);
        info->client->write(BOUNDARY, bdrLen);

        // Log.verbose("streamCB: Served frame# %d\n", fstFrame->fnm);

        if ( myFrame->nxt ) {
          frameChunck_t* f;
          f = (frameChunck_t*) myFrame->nxt;

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
      Log.trace("streamCB: Client disconnected\n");
      Log.verbose("streamCB: Stream Task stack wtrmark  : %d\n", uxTaskGetStackHighWaterMark(info->task));
      info->client->stop();
      delete info->client;
      info->client = NULL;
      free( info );
      info = NULL;
      vTaskDelete(NULL);
    }
    //  Let other tasks run after serving every client
    if ( !xTaskDelayUntil(&xLastWakeTime, xFrequency) ) taskYIELD();
  }
}

#endif