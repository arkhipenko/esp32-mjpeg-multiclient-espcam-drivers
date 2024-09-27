# MJPEG Multiclient Streaming Server 

## With latest ESP-CAM drivers
**Full story:** https://www.hackster.io/anatoli-arkhipenko/multi-client-mjpeg-streaming-from-esp32-47768f

**Video:** https://youtu.be/bsAVJSZeSmc



**Updated on 2021-07-01:**

- OP updated to account for recent repo changes by Espressif

- Recompiled with ESP32 Arduino Core 1.0.6

- Updated with latest ESP CAM drivers

  

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



### DISCLAIMER:

The repository should compile and work **AS-IS** as of the date stated above.  Espressif is actively working on the camera drivers, and their **future updates may break the procedure below**. Please report the broken process providing as much information as possible, definitely the make and model of your camera device, version of the Arduino Core and IDE used. 

Remember, this is a hack, a POC and a test. This is **NOT GUARANTEED** to work on all ESP32-based devices. The performance could be different depending on the make, brand and manufacturer of your camera.

Please do not demand fixes and updates - you are welcome to take this repo as a baseline and improve upon it. 

*Have fun!*



### Procedure:

Use this process **ONLY** if you want to update to the very latest drivers. 

**Remember**: updating to the latest drivers may break the code dependencies and require investigation / code changes. 

1. Clone or pull this repo locally using GIT

2. **Use AS-IS (in Arduino IDE) for guaranteed results.** If you feel adventurous and brave - proceed to step 3. 

3. Download latest ZIP file from https://github.com/espressif/esp32-camera.git into the esp32-cam subfolder

4. In the archive: delete `examples` and `test` folders

5. Delete **ALL FILES** in the sketch folder (from step 1) except `esp32-cam*.ino` and `camera_pins.h`

6. In the archive:  switch to subfolder `esp32-camera-master/target` and delete subfolders for `esp32s2` and `esp32s3` - I have not tested with those ones

7. unzip using `unzip -jo esp32-camera-master.zip` command. This will place all files in the same folder in a flat file structure

   

   **NOTE:** please observe the `-jo` flag: the sketch assumes all files are in the same folder and will overwrite the existing old files without asking for confirmation. 



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

