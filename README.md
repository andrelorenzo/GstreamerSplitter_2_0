# Implementation for a fake IP camera
This implementation takes the image from the video0 interace of your PC (linux) and redirects the video via a RTSP server.
It comes with a RTSP client also

# DEPENDENCIES
* C11
* Gstreamer 1.0 = sudo apt-get install libgstrtspserver-1.0-dev gstreamer1.0-rtsp

# HOW TO COMPILE

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)

```

# HOW TO RUN
create or modify the config_xx.txt files, there you can modify the mount point, the codec (H.264 or H.265) and the port
```bash
./rtsp_server ../../params/config_camera.txt
./rtsp_viewer ../../params/config_debug.txt

```

## 3. Install Daemon and Start


```bash
chmod +x scripts/launch_double_rtsp.sh
chmod +x systemd/install_rtsp_service.sh
./systemd/install_rtsp_service.sh

sudo systemctl enable usb-to-rtsp-double.service

```


```bash
sudo usermod -aG video,render $USER
sudo reboot
```