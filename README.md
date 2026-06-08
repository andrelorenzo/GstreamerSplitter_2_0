# Implementation for a fake IP camera
This implementation takes the image from the video0 interace of your PC (linux) and redirects the video via a RTSP server.
It comes with a RTSP client also

# DEPENDENCIES
* C11
* Gstreamer 1.0
* Cmake

sudo apt update && sudo apt upgrade -y
sudo apt install git -y
sudo apt install cmake -y
sudo apt install -y build-essential cmake pkg-config libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstrtspserver-1.0-dev gstreamer1.0-rtsp

# 1. HOW TO COMPILE

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)

```

# 2. HOW TO RUN
create or modify the config_xx.txt files, there you can modify the mount point, the codec (H.264 or H.265) and the port
```bash
./rtsp_server ../../params/config_camera.txt
./rtsp_viewer ../../params/config_debug.txt

```

## 3. Setup Static IP

```bash
sudo nmcli con show 
sudo nmcli con mod "Wired connection 1" ipv4.addresses 192.168.0.80/24 ipv4.gateway 192.168.0.1 ipv4.dns "192.168.0.1 8.8.8.8" ipv4.method manual
sudo nmcli con down "Wired connection 1"
sudo nmcli con up "Wired connection 1"
```

## 4. Install Daemon and Start


```bash
chmod +x scripts/launch_double_rtsp.sh
chmod +x systemd/install_rtsp_service.sh
./systemd/install_rtsp_service.sh

sudo systemctl enable usb-to-rtsp-double.service

sudo usermod -aG video,render $USER
sudo reboot

```