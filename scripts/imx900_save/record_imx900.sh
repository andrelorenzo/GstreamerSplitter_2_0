#!/usr/bin/env bash
set -euo pipefail

dir="/home/arq/videos/$(date +%Y%m%d_%H%M%S)"
mkdir -p "$dir"
rpicam-vid -n -t "${DURATION_MS:-10000}" --camera 0 --width 1600 --height 900 --mode 2048:1536:8 --framerate 100 --shutter 1000 --gain 1 --denoise cdn_off --codec libav --libav-video-codec libx264 --bitrate 10000000 --low-latency --libav-format h264 -o "$dir/cam0.h264"