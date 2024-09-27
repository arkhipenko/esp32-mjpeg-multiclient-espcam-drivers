#include "streaming.h"

#if defined(CAMERA_MULTICLIENT_QUEUE)

// frameChunck_t frameChunck;
// streamInfo_t streamInfo;
QueueHandle_t streamingClients;

volatile size_t camSize;    // size of the current frame, byte
volatile char* camBuf;      // pointer to the current frame

// ==== RTOS task to grab frames from the camera =========================
void camCB(void* pvParameters) {
  TickType_t xLastWakeTime;

  //  A running interval associated with currently desired frame rate
  const TickType_t xFrequency = pdMS_TO_TICKS(1000 / FPS);

  // Mutex for the critical section of swithing the active frames around
  portMUX_TYPE xSemaphore = portMUX_INITIALIZER_UNLOCKED;

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
    //    APP_CPU);
    PRO_CPU);

  //  Pointers to the 2 frames, their respective sizes and index of the current frame
  char* fbs[2] = { NULL, NULL };
  size_t fSize[2] = { 0, 0 };
  int ifb = 0;

  //=== loop() section  ===================
  xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    //  Grab a frame from the camera and query its size
    camera_fb_t* fb = NULL;

    fb = esp_camera_fb_get();
    size_t s = fb->len;

    //  If frame size is more that we have previously allocated - request  125% of the current frame space
    if (s > fSize[ifb]) {
      fSize[ifb] = s * 4 / 3;
      fbs[ifb] = allocateMemory(fbs[ifb], fSize[ifb]);
    }

    //  Copy current frame into local buffer
    char* b = (char *)fb->buf;
    memcpy(fbs[ifb], b, s);
    esp_camera_fb_return(fb);

    //  Let other tasks run and wait until the end of the current frame rate interval (if any time left)
    if ( !xTaskDelayUntil(&xLastWakeTime, xFrequency) ) taskYIELD();

    //  Only switch frames around if no frame is currently being streamed to a client
    //  Wait on a semaphore until client operation completes
    xSemaphoreTake( frameSync, portMAX_DELAY );

    //  Do not allow interrupts while switching the current frame
    taskENTER_CRITICAL(&xSemaphore);
    camBuf = fbs[ifb];
    camSize = s;
    ++ifb;
    ifb = ifb & 1;  // this should produce 1, 0, 1, 0, 1 ... sequence
    taskEXIT_CRITICAL(&xSemaphore);

    //  Let anyone waiting for a frame know that the frame is ready
    xSemaphoreGive( frameSync );

    //  Technically only needed once: let the streaming task know that we have at least one frame
    //  and it could start sending frames to the clients, if any
    xTaskNotifyGive( tStream );

    //  Immediately let other (streaming) tasks run
    taskYIELD();

    //  If streaming task has suspended itself (no active clients to stream to)
    //  there is no need to grab frames from the camera. We can save some juice
    //  by suspedning the tasks
    if ( eTaskGetState( tStream ) == eSuspended ) {
      vTaskSuspend(NULL);  // passing NULL means "suspend yourself"
    }
  }
}


// ==== Handle connection request from clients ===============================
void handleJPGSstream(void)
{
  //  Can only acommodate 10 clients. The limit is a default for WiFi connections
  if ( !uxQueueSpacesAvailable(streamingClients) ) return;

  //  Create a new WiFi Client object to keep track of this one
  WiFiClient* client = new WiFiClient();
  *client = server.client();

  //  Immediately send this client a header
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
          xSemaphoreTake( frameSync, portMAX_DELAY );

          client->write(CTNTTYPE, cntLen);
          sprintf(buf, "%d\r\n\r\n", camSize);
          client->write(buf, strlen(buf));
          client->write((char*) camBuf, (size_t)camSize);
          client->write(BOUNDARY, bdrLen);

          // Since this client is still connected, push it to the end
          // of the queue for further processing
          xQueueSend(streamingClients, (void *) &client, 0);

          //  The frame has been served. Release the semaphore and let other tasks run.
          //  If there is a frame switch ready, it will happen now in between frames
          xSemaphoreGive( frameSync );
        }
      }
    }
    else {
      //  Since there are no connected clients, there is no reason to waste battery running
      vTaskSuspend(NULL);
    }
    //  Let other tasks run after serving every client
    if ( !xTaskDelayUntil(&xLastWakeTime, xFrequency) ) taskYIELD();
  }
}

#endif
