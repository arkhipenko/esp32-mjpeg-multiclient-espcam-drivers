//  === Arduino-Log implementation  =================================================================
#include "logging.h"

// Setup default logging system
void setupLogging() {
#ifndef DISABLE_LOGGING
  Log.begin(LOG_LEVEL, &Serial);
  Log.setPrefix(printTimestampMillis);
  Log.trace("setupLogging()" CR);
#endif  //  #ifndef DISABLE_LOGGING
}


// === millis() - based timestamp ==
void printTimestamp(Print* logOutput) {
  char c[24];
  sprintf(c, "%10lu ", (long unsigned int) xTaskGetTickCount()/*millis()*/);
  logOutput->print(c);
}


// start-time-based timestamp ========
void printTimestampMillis(Print* logOutput) {
  char c[64];
  unsigned long mm = xTaskGetTickCount();//millis();
  int ms = mm % 1000;
  int s = mm / 1000;
  int m = s / 60;
  int h = m / 60;
  int d = h / 24;
  sprintf(c, "%02d:%02d:%02d:%02d.%03d ", d, h % 24, m % 60, s % 60, ms);
  logOutput->print(c);
}


#ifndef DISABLE_LOGGING
void printBuffer(const char* aBuf, size_t aSize) {
    Serial.println("Buffer contents:");

    char c;
    String s;
    size_t cnt = 0;

    for (int j = 0; j < aSize / 16 + 1; j++) {
      Serial.printf("%04x : ", j * 16);
      for (int i = 0; i < 16 && cnt < aSize; i++) {
        c = aBuf[cnt++];
        Serial.printf("%02x ", c);
        if (c < 32) {
          s += '.';
        }
        else {
          s += c;
        }
      }
      Serial.print(" : ");
      Serial.println(s);
      s = "";
    }
    Serial.println();
}
#else
void printBuffer(const char* aBuf, size_t aSize) {}
#endif
