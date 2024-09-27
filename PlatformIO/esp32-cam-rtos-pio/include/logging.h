#pragma once
// ==== includes =================================
#include "references.h"

#define MILLIS_FUNCTION xTaskGetTickCount()
// #define MILLIS_FUNCTION millis()

// ==== prototypes ===============================
void    setupLogging();

#ifndef DISABLE_LOGGING
void    printTimestampMillis(Print* logOutput);
void    printBuffer(const char* aBuf, size_t aSize);
#endif  //   #ifndef DISABLE_LOGGING
