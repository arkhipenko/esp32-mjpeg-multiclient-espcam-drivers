# MJPEG Multiclient Streaming Server 

## THIS DOCUMENT IS WORK IN PROGRESS



## With latest ESP-CAM drivers
**Full story:** https://www.hackster.io/anatoli-arkhipenko/multi-client-mjpeg-streaming-from-esp32-47768f

**Video:** https://youtu.be/bsAVJSZeSmc



**Updated on 2024-09-26:**

- moved to Platform IO

- code includes all three RTOS frame delivery options:

     - Single RTOS task serving the same frame to all  clients (compile CAMERA_MULTICLIENT_QUEUE)

     - Separate dedicated RTOS tasks serving the same frame to clients independently (compile CAMERA_MULTICLIENT_TASK)

     - Serarate dedicated RTOS tasks serving individual frames to clients - no frames are "dropped" (compile CAMERA_ALL_FRAMES)


### Quick start

Pull the repo recursively in order to include the latest esp32-camera drivers. 

`git clone --recurse-submodules https://github.com/arkhipenko/esp32-mjpeg-multiclient-espcam-drivers.git`

Open workspace file `esp32-cam-rtos-pio\esp32-cam-rtos-pio.code-workspace` with MS VSCode

Inspect the platformio.ini file:

- Select frame service:
     - -D CAMERA_MULTICLIENT_QUEUE
     - -D CAMERA_MULTICLIENT_TASK
     - -D CAMERA_ALL_FRAMES
- Select appropriate frame size: -D FRAME_SIZE=FRAMESIZE_HVGA <== example
- Choose FPS (10 recommended)
- Choose JPEG quality: JPEG_QUALITY  ==> 0-63 lower means higher quality (0, 1, and 2 did not work for me)

Switch to Platform IO menu then build and upload appropriate camera options

If your camera model is not listed - read on or try contacting me to create appropriate build option, device file and partition scheme. 

When firmware is running, connect to the WIFI AP it creates:

- SSID: mjpeg-ap
- Password: MJPEG123

Navigate to: http://192.168.4.1 to the WiFiManager main page.

Provide WIFI credentials for your network

Once connected, the MJPEG stream is available on: Stream Link: http://your.local.network.IP.address/mjpeg/1

e.g.: Stream Link: http://192.168.122.132/mjpeg/1



### Notes:

#### What is this?

MJPEG Multiclient Streaming Server using RTOS tasks and queues to serve video frames to clients from the onboard camera. 

Several video frame delivery options are available:

- Single RTOS task serving the same frame to all  clients (compile CAMERA_MULTICLIENT_QUEUE)

- Separate dedicated RTOS tasks serving the same frame to clients independently (compile CAMERA_MULTICLIENT_TASK)

- Serarate dedicated RTOS tasks serving individual frames to clients - no frames are "dropped" (compile CAMERA_ALL_FRAMES)


#### Compile options - Camera types

The following camearas are supported.  (configurable in the `platformio.ini` file)
Only ESP32-CAM and ESP-EYE were actually tested. 

Pinouts for some/all cameras are described here: 
https://randomnerdtutorials.com/esp32-cam-camera-pin-gpios/

- ESP-EYE (CAMERA_MODEL_ESP_EYE)

- ESP32-CAM AI-Thinker (CAMERA_MODEL_AI_THINKER)

These cameras could be supported, but require additional work (creating board and partition files, and editing of platformio.ini file)

- Freenove ESP32-Wrover (CAMERA_MODEL_WROVER_KIT)

- M5 Cameras (models A and B) (CAMERA_MODEL_M5STACK_PSRAM)

- M5-stack ESP32 camera without PSRAM (CAMERA_MODEL_M5STACK_WIDE)

- TTGO T-Journal (CAMERA_TTGO_T_JOURNAL)

- TTGO T-Camera Plus (CAMERA_TTGO_T_PLUS)

- TTGO T-Camera Plus (CAMERA_TTGO_PIR)


#### Compile options - Frame size

The following frame sizes are currently supported by the camera: (configurable in the `platformio.ini` file)

- FRAMESIZE_96X96,    // 96x96

- FRAMESIZE_QQVGA,    // 160x120

- FRAMESIZE_QCIF,     // 176x144

- FRAMESIZE_HQVGA,    // 240x176

- FRAMESIZE_240X240,  // 240x240

- FRAMESIZE_QVGA,     // 320x240

- FRAMESIZE_CIF,      // 400x296

- FRAMESIZE_HVGA,     // 480x320

- FRAMESIZE_VGA,      // 640x480

- FRAMESIZE_SVGA,     // 800x600

- FRAMESIZE_XGA,      // 1024x768

- FRAMESIZE_HD,       // 1280x720

- FRAMESIZE_SXGA,     // 1280x1024

- FRAMESIZE_UXGA,     // 1600x1200

Larger frames are served slower


#### Latest camera drivers

This repo references Espressif's latest camera drivers' git repo directly as a component. 
The drivers re located at: https://github.com/espressif/esp32-camera

To update to the latest - run `git pull origin` command from the `esp32-camera` folder.



### DISCLAIMER:

The repository should compile and work **AS-IS** as of the date stated above.  Espressif is actively working on the camera drivers, and their **future updates may break the procedure below**. Please report the broken process providing as much information as possible, definitely the make and model of your camera device, version of the Arduino Core and IDE used. 

Remember, this is a hack, a POC and a test. This is **NOT GUARANTEED** to work on all ESP32-based devices. The performance could be different depending on the make, brand and manufacturer of your camera.

Please do not demand fixes and updates - you are welcome to take this repo as a baseline and improve upon it. 

*Have fun!*



### Procedure:

#### WORK IN PROGRESS

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

