#include "streaming.h"
#include "lwip/sockets.h"

#if defined(CAMERA_MULTICLIENT_QUEUE)

#if defined(BENCHMARK)
#include <AverageFilter.h>
#define BENCHMARK_PRINT_INT 1000
averageFilter<uint32_t> captureAvg(10);
uint32_t lastPrintCam = millis();
#endif

QueueHandle_t streamingClients;

volatile size_t camSize;    // size of the current frame, byte
volatile char* camBuf;      // pointer to the current frame

// ==== RTOS task to grab frames from the camera =========================
void camCB(void* pvParameters) {
  TickType_t xLastWakeTime;

  //  A running interval associated with currently desired frame rate
  const TickType_t xFrequency = pdMS_TO_TICKS(1000 / FPS);

  // Creating a queue to track all connected clients
  streamingClients = xQueueCreate( 10, sizeof(WiFiClient*) );


  //  Creating task to push the stream to all connected clients
  xTaskCreatePinnedToCore(
    streamCB,
    "strmCB",
    4096,
    NULL, //(void*) handler,
    2,
    &tStream,
    APP_CPU);
    // PRO_CPU);

  //  Pointers to the 2 frames, their respective sizes and index of the current frame
  char* fbs[2] = { NULL, NULL };
  size_t fSize[2] = { 0, 0 };
  int ifb = 0;

  //=== loop() section  ===================
  xLastWakeTime = xTaskGetTickCount();

#if defined(BENCHMARK)
  captureAvg.initialize();
#endif

  camera_fb_t* fb = NULL;

  for (;;) {
    //  Grab a frame from the camera and query its size

#if defined(BENCHMARK)
    uint32_t captureStart = micros();
#endif

    fb = esp_camera_fb_get();
    size_t s = fb->len;

    //  If frame size is more that we have previously allocated - request  125% of the current frame space
    if (s > fSize[ifb]) {
      fSize[ifb] = s * 4 / 3;
      fbs[ifb] = allocateMemory(fbs[ifb], fSize[ifb], FAIL_IF_OOM, ANY_MEMORY);
    }

    //  Copy current frame into local buffer
    char* b = (char *)fb->buf;
    memcpy(fbs[ifb], b, s);
    esp_camera_fb_return(fb);
  
#if defined(BENCHMARK)
    captureAvg.value(micros()-captureStart);
#endif

    //  Only switch frames around if no frame is currently being streamed to a client
    //  Wait on a semaphore until client operation completes
    xSemaphoreTake( frameSync, portMAX_DELAY );

    //  Do not allow interrupts while switching the current frame
    // taskENTER_CRITICAL(&xSemaphore);
    camBuf = fbs[ifb];
    camSize = s;
    ++ifb;
    ifb = ifb & 1;  // this should produce 1, 0, 1, 0, 1 ... sequence
    // taskEXIT_CRITICAL(&xSemaphore);

    //  Let anyone waiting for a frame know that the frame is ready
    xSemaphoreGive( frameSync );

    //  Technically only needed once: let the streaming task know that we have at least one frame
    //  and it could start sending frames to the clients, if any
    xTaskNotifyGive( tStream );


    //  Let other tasks run and wait until the end of the current frame rate interval (if any time left)
    if ( xTaskDelayUntil(&xLastWakeTime, xFrequency) != pdTRUE ) taskYIELD();

    //  If streaming task has suspended itself (no active clients to stream to)
    //  there is no need to grab frames from the camera. We can save some juice
    //  by suspedning the tasks
    if ( eTaskGetState( tStream ) == eSuspended ) {
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
  //  Can only acommodate 10 clients. The limit is a default for WiFi connections
  if ( !uxQueueSpacesAvailable(streamingClients) ) {
    Log.error("handleJPGSstream: Max number of WiFi clients reached\n");
    return;
  }

  //  Create a new WiFi Client object to keep track of this one
  WiFiClient* client = new WiFiClient();
  if ( client == NULL ) {
    Log.error("handleJPGSstream: Can not create new WiFi client - OOM\n");
    return;
  }
  *client = server.client();

  //  Immediately send this client a header
  client->setTimeout(1);
  client->write(HEADER, hdrLen);
  client->write(BOUNDARY, bdrLen);

  // Push the client to the streaming queue
  xQueueSend(streamingClients, (void *) &client, 0);

  // Wake up streaming tasks, if they were previously suspended:
  if ( eTaskGetState( tCam ) == eSuspended ) vTaskResume( tCam );
  if ( eTaskGetState( tStream ) == eSuspended ) vTaskResume( tStream );

  Log.trace("handleJPGSstream: Client connected\n");
}


// ==== Actually stream content to all connected clients ========================
void streamCB(void * pvParameters) {
  char buf[16];
  TickType_t xLastWakeTime;
  TickType_t xFrequency = pdMS_TO_TICKS(1000 / FPS);

  //  Wait until the first frame is captured and there is something to send
  //  to clients
  ulTaskNotifyTake( pdTRUE,          /* Clear the notification value before exiting. */
                    portMAX_DELAY ); /* Block indefinitely. */

#if defined(BENCHMARK)
  averageFilter<int32_t> streamAvg(10);
  averageFilter<int32_t> waitAvg(10);
  averageFilter<uint32_t> frameAvg(10);
  averageFilter<float> fpsAvg(10);
  uint32_t streamStart = 0;
  streamAvg.initialize();
  waitAvg.initialize();
  frameAvg.initialize();
  uint32_t lastPrint = millis();
  uint32_t lastFrame = millis();
#endif

  xLastWakeTime = xTaskGetTickCount();
  for (;;) {
    // Default assumption we are running according to the FPS


    //  Only bother to send anything if there is someone watching
    UBaseType_t activeClients = uxQueueMessagesWaiting(streamingClients);
    if ( activeClients ) {
      WiFiClient *client;

      for (int i = 0; i<activeClients; i++) {
        //  Since we are sending the same frame to everyone,
        //  pop a client from the the front of the queue

        xQueueReceive (streamingClients, (void*) &client, 0);

      //  Check if this client is still connected.
        if (!client->connected()) {
          //  delete this client reference if s/he has disconnected
          //  and don't put it back on the queue anymore. Bye!
          Log.trace("streamCB: Client disconnected\n");
          delete client;
        }
        else {

          //  Ok. This is an actively connected client.
          //  Let's grab a semaphore to prevent frame changes while we
          //  are serving this frame
#if defined (BENCHMARK)
          streamStart = micros();
#endif

          xSemaphoreTake( frameSync, portMAX_DELAY );

#if defined (BENCHMARK)
          waitAvg.value(micros()-streamStart);
          frameAvg.value(camSize);
          streamStart = micros();
#endif

          sprintf(buf, "%d\r\n\r\n", camSize);
          client->flush();
          client->write(CTNTTYPE, cntLen);
          client->write(buf, strlen(buf));
          client->write((char*) camBuf, (size_t)camSize);
          client->write(BOUNDARY, bdrLen);

#if defined (BENCHMARK)
          streamAvg.value(micros()-streamStart);
#endif

          //  The frame has been served. Release the semaphore and let other tasks run.
          //  If there is a frame switch ready, it will happen now in between frames
          xSemaphoreGive( frameSync );

          // Since this client is still connected, push it to the end
          // of the queue for further processing
          xQueueSend(streamingClients, (void *) &client, 0);

        }
      }
    }
    else {
      //  Since there are no connected clients, there is no reason to waste battery running
      vTaskSuspend(NULL);
    }
    //  Let other tasks run after serving every client
    if ( xTaskDelayUntil(&xLastWakeTime, xFrequency) != pdTRUE ) taskYIELD();

#if defined (BENCHMARK)
    fpsAvg.value(1000.0 / (float) (millis()-lastFrame) );
    lastFrame = millis();
#endif

#if defined (BENCHMARK)
    if ( millis() - lastPrint > BENCHMARK_PRINT_INT ) {
      lastPrint = millis();
      Log.verbose("streamCB: wait avg=%d, stream avg=%d us, frame avg size=%d bytes, fps=%S\n", waitAvg.currentValue(), streamAvg.currentValue(), frameAvg.currentValue(), String(fpsAvg.currentValue()));
    }
#endif

  }
}

#endif
