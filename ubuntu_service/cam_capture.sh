#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

# === Config por variables de entorno (puedes sobreescribirlas en el servicio) ===
DEV="${DEV:-/dev/video0}"
OUTDIR="${OUTDIR:-/var/recordings}"
INPUT_FORMAT="${INPUT_FORMAT:-yuyv422}"
# Por defecto, comprime a H.264 4:2:2 (desde YUYV)
FFMPEG_OPTS="${FFMPEG_OPTS:- -c:v libx264 -pix_fmt yuv422p -preset veryfast -crf 18 }"

# === Espera a que aparezca el dispositivo (hasta ~5s) ===
for i in {1..10}; do
  [[ -e "$DEV" ]] && break
  sleep 0.5
done
if [[ ! -e "$DEV" ]]; then
  echo "ERROR: No existe $DEV" >&2
  exit 1
fi

SYS="$(basename "$DEV")"                    # p.ej. video0
NAME_PATH="/sys/class/video4linux/${SYS}/name"
if [[ ! -r "$NAME_PATH" ]]; then
  echo "ERROR: No puedo leer $NAME_PATH" >&2
  exit 1
fi

RAW_NAME="$(<"$NAME_PATH")"
# Reemplaza espacios y barras por dobles guiones bajos
CLEAN_STEP1="$(echo "$RAW_NAME" | tr ' /' '__')"
# Mantén solo alfanumérico, guion bajo, punto y guion
SAFE_NAME="$(printf "%s" "$CLEAN_STEP1" | tr -cd '[:alnum:]_.-')"
[[ -z "$SAFE_NAME" ]] && SAFE_NAME="UnknownCam"

DATE="$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUTDIR"
OUTFILE="${OUTDIR}/${SAFE_NAME}_${DATE}.mkv"

echo "Grabando desde ${DEV} (${RAW_NAME} -> ${SAFE_NAME}) a ${OUTFILE}"

# No fijamos framerate ni tamaño para permitir variación de FPS/resolución
exec ffmpeg -f v4l2 -input_format "$INPUT_FORMAT" -thread_queue_size 512 \
  -i "$DEV" -vsync vfr -fflags +genpts \
  $FFMPEG_OPTS "$OUTFILE"

# --- Alternativa SEGMENTADA (trozos de 10 min):
# exec ffmpeg -f v4l2 -input_format "$INPUT_FORMAT" -thread_queue_size 512 \
#   -i "$DEV" -vsync vfr -fflags +genpts \
#   $FFMPEG_OPTS -f segment -segment_time 600 -reset_timestamps 1 -strftime 1 \
#   "${OUTDIR}/${SAFE_NAME}_%Y%m%d_%H%M%S.mkv"
