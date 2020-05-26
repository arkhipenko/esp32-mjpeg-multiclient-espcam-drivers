# MJPEG Multiclient Streaming Server 

## With latest ESP-CAM drivers

### Procedure:

1. Download latest ZIP file from https://github.com/espressif/esp32-camera.git into the esp32-cam subfolder

2. unzip using `unzip -j esp32-cam-master.zip` command. This will place all files in the same folder

**NOTE:** please observe the `-j` flag: the sketch assumes all files are in the same folder. 

3. In **esp32-cam.ino** sketch select your camera pin assignment. The choices are:

1. CAMERA_MODEL_WROVER_KIT
2. CAMERA_MODEL_ESP_EYE
3. CAMERA_MODEL_M5STACK_PSRAM
4. CAMERA_MODEL_M5STACK_WIDE
5. CAMERA_MODEL_AI_THINKER

4. Compile the **esp32-cam.ino** sketch using the following settings:

ESP32 Dev Module
CPU Freq: 240
Flash Freq: 80
Flash mode: QIO
Flash Size: 4Mb
Partition: Minimal SPIFFS (or any other that would fit the sketch)
PSRAM: Enabled

### Results:

I was able to run multiple browser windows, multiple VLC windows and connect multiple Blynk video widgets (max: 10) to ESP-EYE chip. The delay on the browser window was almost unnoticeable. In VLC you notice a 1 second delay probably due to buffering. Blynk performance all depends on the phone, so no comments there. 

This is incredible considering the size of this thing! The camera on ESP-EYE is actually quite good. 

### Enjoy!
