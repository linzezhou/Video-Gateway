#!/usr/bin/env bash
# A2 receiver on the Mac (GStreamer). Always use the SAME receiver for all
# latency measurements; the ffplay alternative is: scripts/recv_mac.sh ffplay
# Usage: scripts/recv_mac.sh [gst|ffplay] [port]
set -euo pipefail

MODE=${1:-gst}
PORT=${2:-5000}
HERE=$(cd "$(dirname "$0")" && pwd)

if [[ "$MODE" == "ffplay" ]]; then
  # stream.sdp hardcodes port 5000
  exec ffplay -protocol_whitelist file,udp,rtp -fflags nobuffer -flags low_delay \
    -framedrop "$HERE/stream.sdp"
fi

exec gst-launch-1.0 udpsrc port="$PORT" \
  caps="application/x-rtp,media=video,encoding-name=H264,payload=96,clock-rate=90000" ! \
  rtpjitterbuffer latency=0 ! rtph264depay ! h264parse ! avdec_h264 ! \
  videoconvert ! autovideosink sync=false
