//
// Digital implementation of an average filter
// with variable number of samples
//

#include <Arduino.h>

#ifndef _AVERAGEFILTER_H
#define _AVERAGEFILTER_H

template<typename T>
class averageFilter {
    public:
        averageFilter(int aSamples);
        averageFilter(int aSamples, T* aStorage);
        ~averageFilter();
        
        void    initialize();
        T       value(T aSample);
        void    setSamples( int aSamples );

        inline  T         currentValue() { return iCV; }
        inline  int32_t   samples() { return iCount; }
        inline  bool      memoryError() { return iReadings == NULL; }
        
    private:
        T         *iReadings;
        T         iTotal;
        T         iCV;
        int32_t   iIndex;
        int32_t   iCount;
        int32_t   iSamples;
};



template<typename T>
averageFilter<T>::averageFilter(int aSamples) {
    iSamples = constrain(aSamples, 1, aSamples); 
    iReadings = (T *) malloc (sizeof (T) * aSamples);
// if there is a memory allocation error.
   if (iReadings == NULL) {
    iReadings = &iCV;
    iSamples = 1;
   }
  initialize();
}

template<typename T>
averageFilter<T>::averageFilter(int aSamples, T* aStorage) {
    iSamples = constrain(aSamples, 1, aSamples); 
    iReadings = aStorage;
// if storage provided is NULL
   if (iReadings == NULL) {
    iReadings = &iCV;
    iSamples = 1;
   }
  initialize();
}

template<typename T>
void averageFilter<T>::setSamples(int aSamples) {
    iSamples = constrain(aSamples, 1, aSamples); 
    if ( iCount ) initialize();
}

template<typename T>
averageFilter<T>::~averageFilter() {
    if ( iReadings && iReadings != &iCV ) free (iReadings);
    iReadings = NULL;
    iIndex = 0;
    iTotal = 0;
    iCount = 0;
}

template<typename T>
void averageFilter<T>::initialize() {
    iIndex = 0;
    iTotal = 0;
    iCount = 0;
    iCV = 0;
    for (int i=0; i<iSamples; i++) iReadings[i]=0;
}

template<typename T>
T   averageFilter<T>::value(T aSample) {
    // if ( !iReadings ) return 0;
    iTotal -= iReadings[iIndex];
    iReadings[iIndex] = aSample;
    iTotal += aSample;
    if (++iIndex >= iSamples) iIndex = 0;
    if (++iCount > iSamples) iCount = iSamples;
    iCV = (T) iTotal/iCount;
    return iCV;
}

#endif  // _AVERAGEFILTER_H