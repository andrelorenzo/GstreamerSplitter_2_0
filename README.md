# Implementation for a fake IP camera
This implementation takes the image from the video0 interace of your PC (linux) and redirects the video via a RTSP server.
It comes with a RTSP client also

# DEPENDENCIES
* C11
* Gstreamer 1.0

# HOW TO COMPILE

´´´bash
mkdir build
cd build
cmake ..
make -j$(nproc)
´´´

# HOW TO RUN
create or modify the config_xx.txt files, there you can modify the mount point, the codec (H.264 or H.265) and the port
```bash
./rtsp_server ../../params/config_camera.txt
./rtsp_viewer ../../params/config_debug.txt
```

> If the configuration (port, mount and codec) doesn't work, may be that the chossen port needs root privilige, in that case try to run it as ```sudo```.