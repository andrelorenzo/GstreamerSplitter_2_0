#!/usr/bin/env bash

set -euo pipefail

SERVICE_NAME="${SERVICE_NAME:-usb-to-rtsp-double.service}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LAUNCHER="$REPO_DIR/scripts/launch_double_rtsp.sh"
SERVICE_PATH="/etc/systemd/system/$SERVICE_NAME"
RUN_USER="${SERVICE_USER:-${SUDO_USER:-$(id -un)}}"

if [ ! -f "$LAUNCHER" ]; then
    echo "ERROR: no existe el lanzador: $LAUNCHER" >&2
    exit 1
fi

chmod +x "$LAUNCHER"

SUPPLEMENTARY_GROUPS=""
GROUP_LIST=""
for group in video render; do
    if getent group "$group" >/dev/null; then
        GROUP_LIST="$GROUP_LIST $group"
    fi
done

if [ -n "$GROUP_LIST" ]; then
    SUPPLEMENTARY_GROUPS="SupplementaryGroups=${GROUP_LIST# }"
fi

sudo tee "$SERVICE_PATH" >/dev/null <<EOF_SERVICE
[Unit]
Description=Dual RTSP camera server
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=$RUN_USER
$SUPPLEMENTARY_GROUPS
WorkingDirectory=$REPO_DIR
ExecStart=$LAUNCHER
Restart=on-failure
RestartSec=3
KillMode=control-group
TimeoutStopSec=10
StandardOutput=journal
StandardError=journal
Environment=GST_DEBUG=2

[Install]
WantedBy=multi-user.target
EOF_SERVICE

sudo systemctl daemon-reload
sudo systemctl enable "$SERVICE_NAME"

echo "Servicio instalado: $SERVICE_NAME"
echo "Usuario del servicio: $RUN_USER"
echo "Ruta del servicio: $SERVICE_PATH"
echo "Arrancar: sudo systemctl start $SERVICE_NAME"
echo "Logs: journalctl -u $SERVICE_NAME -f"
