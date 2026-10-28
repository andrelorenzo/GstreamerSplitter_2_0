# Implementation for a fake IP camera
This implementation takes the image from the video0 interace of your PC (linux) and redirects the video via a RTSP server.
It comes with a RTSP client also

# DEPENDENCIES
* C11
* Gstreamer 1.0
* Cmake 3.21

``` bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y build-essential git cmake pkg-config libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstrtspserver-1.0-dev gstreamer1.0-rtsp
```

# 0. INSTALL DRIVER (RPI4/5)
https://github.com/VC-MIPI-modules/vc_mipi_raspi

https://github.com/VC-MIPI-modules/vc_mipi_raspi/blob/main/docs/libcamera.md
## RASPBERRY PI 4
```bash
mkdir vccam
cd vccam
wget https://github.com/VC-MIPI-modules/vc_mipi_raspi/releases/download/v0.6.10/vc-mipi-driver-bcm2711_0.6.10_arm64.deb
sudo apt install ./vc-mipi-driver-bcm2711_0.6.10_arm64.deb
sudo nano /boot/firmware/config_vc-mipi-driver-bcm2712.txt
change to 1 lane, sony cameras, libcameraon
ctrl+x
y
wget https://raw.githubusercontent.com/VC-MIPI-modules/vc_mipi_raspi/main/Makefile
make all-rpi4
```
## RASPBERRY PI 5
```bash
wget https://github.com/VC-MIPI-modules/vc_mipi_raspi/releases/download/v0.6.10/vc-mipi-driver-bcm2712_0.6.10_arm64.deb
sudo apt install ./vc-mipi-driver-bcm2712_0.6.10_arm64.deb
sudo nano /boot/firmware/config_vc-mipi-driver-bcm2712.txt
change to 1 lane, sony cameras, libcameraon
ctrl+x
y
wget https://raw.githubusercontent.com/VC-MIPI-modules/vc_mipi_raspi/main/Makefile
make all
```

## CHECK INSTALLAION
```bash
sudo reboot -n
sudo dmesg # check for camera detection
vc-config # check for camera detection
```

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
