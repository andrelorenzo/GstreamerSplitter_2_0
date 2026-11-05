#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

# Stop old schedules before replacing their unit files.
sudo systemctl disable --now record-imx900.timer 2>/dev/null || true
sudo systemctl disable --now grabar-imx900.timer 2>/dev/null || true

sudo install -m 755 "$script_dir/record_imx900.sh" /usr/local/bin/record_imx900.sh
sudo install -m 644 "$script_dir/record_imx900.service" /etc/systemd/system/record-imx900.service
sudo install -m 644 "$script_dir/record_imx900.timer" /etc/systemd/system/record-imx900.timer

# Remove the previous unit names so repeated installs cannot leave an old timer active.
sudo rm -f /etc/systemd/system/grabar-imx900.service /etc/systemd/system/grabar-imx900.timer
sudo systemctl daemon-reload
sudo systemctl enable record-imx900.timer

echo "Instalación completada. El timer record-imx900.timer quedó habilitado para el siguiente arranque."
