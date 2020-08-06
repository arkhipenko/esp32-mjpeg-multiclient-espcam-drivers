# MJPEG Multiclient Streaming Server 

## With latest ESP-CAM drivers
Full story: https://www.hackster.io/anatoli-arkhipenko/multi-client-mjpeg-streaming-from-esp32-47768f

### Contents:

#### esp32-cam folder:

MJPEG Multiclient Streaming Server using RTOS queue to serve video to clients. 

The problem with this approach is that the slowest connected client slows it down for everyone. 

#### esp32-cam-rtos folder:

MJPEG Multiclient Streaming Server using dedicated RTOS tasks to serve video to clients. 

This solves the problem of slowest client as every client is served independently based on their bandwidth. Slow clients  are just not getting all the frames. 

#### esp32-cam-allframes folder:

MJPEG Multiclient Streaming Server using dedicated RTOS tasks to serve video to clients. 

All captured frames are stored in PSRAM (until you run out of memory) and served to individual clients one after another, so every client is guaranteed to get all frames in order, at their own pace (good for recording without frame drops)



### Procedure:

1. Download latest ZIP file from https://github.com/espressif/esp32-camera.git into the esp32-cam subfolder

2. Delete `examples` folder from the archive

3. unzip using `unzip -j esp32-cam-master.zip` command. This will place all files in the same folder

   

   **NOTE:** please observe the `-j` flag: the sketch assumes all files are in the same folder. 



In **esp32-cam.ino** sketch select your camera pin assignment. 

The choices are:

- CAMERA_MODEL_WROVER_KIT

- CAMERA_MODEL_ESP_EYE

- CAMERA_MODEL_M5STACK_PSRAM

- CAMERA_MODEL_M5STACK_WIDE

- CAMERA_MODEL_AI_THINKER

  

Compile the **esp32-cam.ino** sketch using the following settings:

- ESP32 Dev Module
- CPU Freq: 240
- Flash Freq: 80
- Flash mode: QIO
- Flash Size: 4Mb
- Partition: Default, Minimal SPIFFS (or any other that would fit the sketch)
- PSRAM: **Enabled**



### Results:

I was able to run multiple browser windows, multiple VLC windows and connect multiple Blynk video widgets (max: 10) to ESP-EYE chip. The delay on the browser window was almost unnoticeable. In VLC you notice a 1 second delay due to buffering. Blynk performance all depends on the phone, so no comments there. 

This is incredible considering the size of this thing! The camera on ESP-EYE is actually quite good. 

### Enjoy!



------

##### Other repositories that may be of interest

###### ESP32 MJPEG streaming server servicing a single client:

https://github.com/arkhipenko/esp32-cam-mjpeg



###### ESP32 MJPEG streaming server servicing multiple clients (FreeRTOS based):

https://github.com/arkhipenko/esp32-cam-mjpeg-multiclient



###### ESP32 MJPEG streaming server servicing multiple clients (FreeRTOS based) with the latest camera drivers from espressif.

https://github.com/arkhipenko/esp32-mjpeg-multiclient-espcam-drivers



###### Cooperative multitasking library:

https://github.com/arkhipenko/TaskScheduler

