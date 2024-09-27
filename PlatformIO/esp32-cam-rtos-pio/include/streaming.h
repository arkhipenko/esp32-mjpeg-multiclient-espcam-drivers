#pragma once
#include "definitions.h"
#include "references.h"

#include "esp_camera.h"

typedef struct {
  uint32_t        frame;
  WiFiClient      *client;
  TaskHandle_t    task;
  char*           buffer;
  size_t          len;
} streamInfo_t;

typedef struct {
  uint8_t   cnt;  // served to clients counter. when equal to number of active clients, could be deleted
  uint32_t* nxt;  // next chunck
  uint32_t  fnm;  // frame number
  uint32_t  siz;  // frame size
 uint8_t*  dat;  // frame pointer
} frameChunck_t;


void camCB(void* pvParameters);
void handleJPGSstream(void);
void streamCB(void * pvParameters);
void mjpegCB(void * pvParameters);
char* allocateMemory(char* aPtr, size_t aSize, bool psramOnly = false);

extern const char* HEADER;
extern const char* BOUNDARY;
extern const char* CTNTTYPE;
extern const int hdrLen;
extern const int bdrLen;
extern const int cntLen;
extern volatile uint32_t frameNumber;

extern frameChunck_t* fstFrame;
extern frameChunck_t* curFrame; 