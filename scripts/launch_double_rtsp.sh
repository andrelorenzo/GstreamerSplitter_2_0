#!/usr/bin/env bash

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$SCRIPT_DIR/../build/usb_to_rtsp"
CFG_IMX="$SCRIPT_DIR/../params/config_imx296.txt"
CFG_BERTIN="$SCRIPT_DIR/../params/config_bertin.txt"

PID_IMX=""
PID_BERTIN=""

stop_children() {
    trap - INT TERM EXIT

    if [ -n "${PID_IMX:-}" ] && kill -0 "$PID_IMX" 2>/dev/null; then
        kill "$PID_IMX" 2>/dev/null || true
    fi

    if [ -n "${PID_BERTIN:-}" ] && kill -0 "$PID_BERTIN" 2>/dev/null; then
        kill "$PID_BERTIN" 2>/dev/null || true
    fi

    wait "$PID_IMX" "$PID_BERTIN" 2>/dev/null || true
}

on_signal() {
    stop_children
    exit 143
}

trap on_signal INT TERM
trap stop_children EXIT

if [ ! -x "$BIN" ]; then
    echo "ERROR: no existe o no es ejecutable: $BIN" >&2
    exit 1
fi

if [ ! -f "$CFG_IMX" ]; then
    echo "ERROR: no existe: $CFG_IMX" >&2
    exit 1
fi

if [ ! -f "$CFG_BERTIN" ]; then
    echo "ERROR: no existe: $CFG_BERTIN" >&2
    exit 1
fi

echo "Lanzando RTSP IMX296..."
"$BIN" "$CFG_IMX" "$@" &
PID_IMX=$!

echo "Lanzando RTSP BERTIN..."
"$BIN" "$CFG_BERTIN" "$@" &
PID_BERTIN=$!

echo "Procesos lanzados:"
echo "  IMX296 PID = $PID_IMX"
echo "  BERTIN PID = $PID_BERTIN"

wait -n "$PID_IMX" "$PID_BERTIN"
STATUS=$?

echo "Uno de los procesos RTSP ha terminado. Parando el otro..."
stop_children
exit "$STATUS"