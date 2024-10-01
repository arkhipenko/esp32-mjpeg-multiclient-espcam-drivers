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

const char*  STREAMING_URL = "/mjpeg/1";

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
      4 * KILOBYTE, // stack size
      NULL,         // parameters
      tskIDLE_PRIORITY + 2, // priority
      &tCam,        // RTOS task handle
      APP_CPU);     // core

  //  Registering webserver handling routines
  server.on(STREAMING_URL, HTTP_GET, handleJPGSstream);
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
char* allocatePSRAM(size_t aSize) {
  if ( psramFound() && ESP.getFreePsram() > aSize ) {
    return (char*) ps_malloc(aSize);
  }
  return NULL;
}

char* allocateMemory(char* aPtr, size_t aSize, bool fail, bool psramOnly) {

  //  Since current buffer is too smal, free it
  if (aPtr != NULL) {
    free(aPtr);
    aPtr = NULL;
  }

  char* ptr = NULL;

  if ( psramOnly ) {
    ptr = allocatePSRAM(aSize);
  }
  else {
    // If memory requested is more than 2/3 of the currently free heap, try PSRAM immediately
    if ( aSize > ESP.getFreeHeap() * 2 / 3 ) {
      ptr = allocatePSRAM(aSize);
    }
    else {
      //  Enough free heap - let's try allocating fast RAM as a buffer
      ptr = (char*) malloc(aSize);

      //  If allocation on the heap failed, let's give PSRAM one more chance:
      if ( ptr == NULL ) ptr = allocatePSRAM(aSize);
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


// ==== Handle invalid URL requests ============================================
void handleNotFound() {
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

