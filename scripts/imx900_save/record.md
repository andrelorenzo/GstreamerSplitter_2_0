Guarda esto como record_imx900.sh. Usa la configuración que acabas de probar, con ganancia fija para evitar el tono rosa:
#!/usr/bin/env bash
set -euo pipefail

dir="/home/arq/videos/$(date +%Y%m%d_%H%M%S)"
mkdir -p "$dir"
rpicam-vid -n -t "${DURATION_MS:-300000}" --camera 0 --width 1600 --height 900 --mode 2048:1536:8 --framerate 100 --shutter 1000 --gain 1 --denoise cdn_off --codec libav --libav-video-codec libx264 --bitrate 10000000 --low-latency --libav-format h264 -o "$dir/cam0.h264"
El archivo .h264 es el formato que ha funcionado en la prueba: 1991 frames en 20 s. Cinco minutos a 10 Mbit/s ocuparán aproximadamente 375–400 MB. Para probar el script durante 20 segundos: DURATION_MS=20000 bash record_imx900.sh.

Para repetirlo en otra CM5:
1. Instala el driver VC MIPI 0.7.0 y la rama update-to-v0.7.0 de libcamera. Configura la IMX900 con 4 carriles y soporte libcamera.
2. En /usr/src/vc-mipi-driver-bcm2712-0.7.0/vc_mipi_core/vc_mipi_modules.c, cambia solo el modo IMX900 RAW8 de 4 carriles con HMAX 338 (MODE( 3, 4, FORMAT_RAW08, ...338...)) por MODE_HMAX( 3, 4, FORMAT_RAW08, 0, 338, 0xffff, 338, 99, 0xffffff, 1794, 255, 15, 413694). Hay otro modo RAW8 con HMAX 348: no lo cambies. Código del driver.
3. Reconstruye el módulo y reinicia:
   sudo dkms build vc-mipi-driver-bcm2712/0.7.0 -k "$(uname -r)" --force
   sudo dkms install vc-mipi-driver-bcm2712/0.7.0 -k "$(uname -r)" --force
   sudo reboot
4. Ejecuta tu instalador del timer de 30 segundos y comprueba que instala este script en /usr/local/bin/record_imx900.sh, con una sola cámara. El timer debe iniciar record-imx900.service