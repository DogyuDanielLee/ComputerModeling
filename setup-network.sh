#!/usr/bin/env bash
# Configure this machine's wired network interface so it can reach the TORCS
# Windows PC directly over an Ethernet cable (see README.md: connect the two
# machines, disable Wi-Fi and IPv6 on both, give the Windows PC a static IP).
#
# This changes real network settings. Review with --dry-run first.
#
# Usage:
#   deploy/setup-network.sh [--dry-run] [local-ip] [torcs-ip] [interface]
#
#   local-ip   static IP to give THIS machine (default: 10.44.72.11)
#              (must be in the same /24 as torcs-ip, and not equal to it)
#   torcs-ip   the Windows/TORCS machine's static IP (default: 10.44.72.10)
#              (used only to sanity-check the subnet below; this script never
#              pings or connects to it - that machine is Windows, out of scope here)
#   interface  wired interface name; auto-detected if omitted
#
# Every lab PC pair is just this one machine cabled point-to-point to one
# TORCS PC, so the defaults above are normally all you need - just run
# deploy/setup-network.sh with no arguments. Pass explicit IPs only when a
# specific machine needs something other than the default pair.
#
# Notes:
#   - Needs NetworkManager (nmcli); most Ubuntu Desktop installs have it.
#   - The engine's TORCS port (13380) is fixed at build time; this script
#     only sets up the network path, not the port.
#   - Run this directly on each lab machine, one at a time - not over a
#     connection that itself depends on the interface being reconfigured.
set -euo pipefail

log() { echo "[setup-network] $*"; }
die() { echo "[setup-network] Error: $*" >&2; exit 1; }

DRY_RUN=false
if [ "${1:-}" = "--dry-run" ]; then
	DRY_RUN=true
	shift
fi
LOCAL_IP="${1:-10.44.72.11}"
TORCS_IP="${2:-10.44.72.10}"
IFACE="${3:-}"

is_ipv4() { [[ "$1" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]]; }
octet3() { cut -d. -f1-3 <<<"$1"; }

is_ipv4 "$LOCAL_IP" || die "not a valid IPv4 address: $LOCAL_IP"
is_ipv4 "$TORCS_IP" || die "not a valid IPv4 address: $TORCS_IP"
[ "$LOCAL_IP" != "$TORCS_IP" ] || die "local-ip and torcs-ip must be different addresses"
[ "$(octet3 "$LOCAL_IP")" = "$(octet3 "$TORCS_IP")" ] \
	|| die "local-ip ($LOCAL_IP) and torcs-ip ($TORCS_IP) are not on the same /24 subnet." \
	"(exactly this kind of mismatch, e.g. 192.174.x.x vs 192.168.x.x, is what breaks the TORCS connection)"

command -v nmcli >/dev/null 2>&1 || die "NetworkManager (nmcli) not found; configure the static IP manually."

if [ -z "$IFACE" ]; then
	# Prefer a wired (non-wifi, non-loopback) device NetworkManager already knows about
	IFACE="$(nmcli -t -f DEVICE,TYPE device status | awk -F: '$2 == "ethernet" {print $1; exit}')"
	[ -n "$IFACE" ] || die "no wired interface found. Plug in the Ethernet cable, or pass the interface name explicitly."
fi
nmcli -t -f DEVICE device status | grep -qx "$IFACE" || die "no such network interface: $IFACE"

CON_NAME="cpsim-torcs-link"

log "Interface:        $IFACE"
log "This machine:      $LOCAL_IP/24"
log "TORCS machine:     $TORCS_IP"
log "Connection name:   $CON_NAME"

run() {
	if [ "$DRY_RUN" = true ]; then
		echo "  [dry-run] $*"
	else
		"$@"
	fi
}

log "Disabling Wi-Fi (README: Wi-Fi must be off on both machines)..."
run nmcli radio wifi off

log "Setting up a static-IP profile on $IFACE (IPv6 off, as README recommends)..."
if nmcli -t -f NAME connection show | grep -qx "$CON_NAME"; then
	run nmcli connection delete "$CON_NAME"
fi
run nmcli connection add type ethernet ifname "$IFACE" con-name "$CON_NAME" \
	ipv4.method manual ipv4.addresses "$LOCAL_IP/24" \
	ipv6.method disabled

log "Bringing the connection up..."
run nmcli connection up "$CON_NAME"

if [ "$DRY_RUN" = true ]; then
	log "Dry run only; nothing was changed. Drop --dry-run to apply."
else
	log "Done. $IFACE is now at $LOCAL_IP/24, Wi-Fi off, IPv6 off."
fi
