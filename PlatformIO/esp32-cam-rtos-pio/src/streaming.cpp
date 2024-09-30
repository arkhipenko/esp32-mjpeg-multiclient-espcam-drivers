#include "streaming.h"

const char* HEADER = "HTTP/1.1 200 OK\r\n" \
                      "Access-Control-Allow-Origin: *\r\n" \
                      "Content-Type: multipart/x-mixed-replace; boundary=+++===123454321===+++\r\n";
const char* BOUNDARY = "\r\n--+++===123454321===+++\r\n";
const char* CTNTTYPE = "Content-Type: image/jpeg\r\nContent-Length: ";
const int hdrLen = strlen(HEADER);
const int bdrLen = strlen(BOUNDARY);
const int cntLen = strlen(CTNTTYPE);
volatile uint32_t frameNumber;

frameChunck_t* fstFrame = NULL;  // first frame
frameChunck_t* curFrame = NULL;  // current frame being captured by the camera

void mjpegCB(void* pvParameters) {
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = pdMS_TO_TICKS(WSINTERVAL);

  // Creating frame synchronization semaphore and initializing it
  frameSync = xSemaphoreCreateBinary();
  xSemaphoreGive( frameSync );


  //  Creating RTOS task for grabbing frames from the camera
  xTaskCreatePinnedToCore(
    camCB,        // callback
    "cam",        // name
    4096,         // stacj size
    NULL,         // parameters
    2,            // priority
    &tCam,        // RTOS task handle
    APP_CPU);     // core

  //  Registering webserver handling routines
  server.on("/mjpeg/1", HTTP_GET, handleJPGSstream);
  server.onNotFound(handleNotFound);

  //  Starting webserver
  server.begin();

  Log.trace("mjpegCB: Starting streaming service\n");
  Log.verbose ("mjpegCB: free heap (start)  : %d\n", ESP.getFreeHeap());

  //=== loop() section  ===================
  xLastWakeTime = xTaskGetTickCount();
  for (;;) {
    server.handleClient();

    //  After every server client handling request, we let other tasks run and then pause
    if ( xTaskDelayUntil(&xLastWakeTime, xFrequency) != pdTRUE ) taskYIELD();
  }
}


// ==== Memory allocator that takes advantage of PSRAM if present =======================
char* allocateMemory(char* aPtr, size_t aSize, bool fail, bool psramOnly) {

  //  Since current buffer is too smal, free it
  if (aPtr != NULL) {
    free(aPtr);
    aPtr = NULL;
  }

  char* ptr = NULL;

  if ( psramOnly ) {
    if ( psramFound() && ESP.getFreePsram() > aSize ) {
      ptr = (char*) ps_malloc(aSize);
    }
  }
  else {
    // If memory requested is more than 2/3 of the currently free heap, try PSRAM immediately
    if ( aSize > ESP.getFreeHeap() * 2 / 3 ) {
      if ( psramFound() && ESP.getFreePsram() > aSize ) {
        ptr = (char*) ps_malloc(aSize);
      }
    }
    else {
      //  Enough free heap - let's try allocating fast RAM as a buffer
      ptr = (char*) malloc(aSize);

      //  If allocation on the heap failed, let's give PSRAM one more chance:
      if ( ptr == NULL && psramFound() && ESP.getFreePsram() > aSize) {
        ptr = (char*) ps_malloc(aSize);
      }
    }
  }
  // Finally, if the memory pointer is NULL, we were not able to allocate any memory, and that is a terminal condition.
  if (fail && ptr == NULL) {
    Log.fatal("allocateMemory: Out of memory!");
    delay(5000);
    ESP.restart();
  }
  return ptr;
}

