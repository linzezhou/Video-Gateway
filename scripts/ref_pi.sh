#!/usr/bin/env bash
# A2 reference pipeline: camera -> HW H.264 -> RTP/UDP -> Mac.
# Usage: scripts/ref_pi.sh <MAC_IP> [port]
# For A3 latency runs add a short shutter: SHUTTER=5000 scripts/ref_pi.sh <MAC_IP>
# Check option names against `rpicam-vid --help` for the installed version.
set -euo pipefail

MAC_IP=${1:?usage: $0 <MAC_IP> [port]}
PORT=${2:-5000}
EXTRA=()
if [[ -n "${SHUTTER:-}" ]]; then EXTRA+=(--shutter "$SHUTTER"); fi

rpicam-vid -t 0 -n --width 1280 --height 720 --framerate 30 \
  --codec h264 --profile baseline --inline --intra 30 ${EXTRA[@]+"${EXTRA[@]}"} -o - | \
gst-launch-1.0 fdsrc fd=0 ! h264parse ! rtph264pay config-interval=1 pt=96 ! \
  udpsink host="$MAC_IP" port="$PORT"
