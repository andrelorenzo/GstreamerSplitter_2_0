#!/usr/bin/env bash
set -Eeuo pipefail

IFACE="eth0"
CON_NAME="Wired connection 1"
IP_CIDR="192.168.0.80/24"
GATEWAY="192.168.0.1"
DNS="192.168.0.1 8.8.8.8"
REBOOT_DELAY_SECONDS=5

if [[ "$EUID" -ne 0 ]]; then echo "Ejecuta este script con sudo: sudo bash $0"; exit 1; fi
if ! command -v nmcli >/dev/null 2>&1; then echo "ERROR: nmcli no está instalado."; exit 1; fi

echo "Conexiones disponibles:"
nmcli con show

if ! nmcli -t -f NAME con show | grep -Fxq "$CON_NAME"; then
    DETECTED_CON="$(nmcli -t -f NAME,DEVICE con show --active | awk -F: -v iface="$IFACE" '$2==iface {print $1; exit}')"
    if [[ -n "${DETECTED_CON:-}" ]]; then CON_NAME="$DETECTED_CON"; else echo "ERROR: no encuentro la conexión '$CON_NAME' ni una conexión activa en $IFACE."; exit 1; fi
fi

echo "Usando conexión: $CON_NAME"
echo "Configurando IP estática $IP_CIDR en $IFACE..."

nmcli con mod "$CON_NAME" connection.interface-name "$IFACE" connection.autoconnect yes ipv4.addresses "$IP_CIDR" ipv4.gateway "$GATEWAY" ipv4.dns "$DNS" ipv4.method manual

echo "Configuración aplicada:"
nmcli con show "$CON_NAME" | grep -E "connection.interface-name|connection.autoconnect|ipv4.addresses|ipv4.gateway|ipv4.dns|ipv4.method"

echo "Sincronizando cambios..."
sync

echo "La Raspberry se reiniciará en ${REBOOT_DELAY_SECONDS} segundos."
echo "Después intenta conectarte con:"
echo "ssh usuario@${IP_CIDR%/*}"

sleep "$REBOOT_DELAY_SECONDS"
systemctl reboot