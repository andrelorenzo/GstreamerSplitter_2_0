#!/bin/bash

# Script: launch_double_rtsp.sh
# Uso: ./launch_double_rtsp.sh

TUNING_FILE="/usr/share/libcamera/ipa/rpi/pisp/imx296_mono.json"

# Comprobar que existe el fichero de tuning
if [ ! -f "$TUNING_FILE" ]; then
    echo "ERROR: No existe el fichero de tuning: $TUNING_FILE" >&2
    exit 1
fi

echo "Usando tuning file para IMX296: $TUNING_FILE"

# Ir al directorio donde está este script
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR" || exit 1

########################################
# 1) Lanzar IMX296 (con tuning) en background
########################################
echo "Lanzando RTSP IMX296..."
LIBCAMERA_RPI_TUNING_FILE="$TUNING_FILE" \
  ./../build/usb_to_rtsp ../params/video_to_rtsp_imx296.txt "$@" &
PID_IMX=$!

########################################
# 2) Lanzar BERTIN (sin tuning especial) en background
########################################
echo "Lanzando RTSP BERTIN..."
./../build/usb_to_rtsp ../params/video_to_rtsp_bertin.txt "$@" &
PID_BERTIN=$!

echo "Procesos lanzados:"
echo "  IMX296  PID = $PID_IMX"
echo "  BERTIN  PID = $PID_BERTIN"

# Esperar a que terminen (si matas el script, se caen los dos)
wait
