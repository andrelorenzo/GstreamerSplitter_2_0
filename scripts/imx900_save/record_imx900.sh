#!/usr/bin/env bash
set -euo pipefail

duration=${DURATION_MS:-10000}
dir="/home/arq/videos/$(date +%Y-%m-%d_%H-%M-%S)"
mkdir -p "$dir"
rpicam-vid -n -t "$duration" --camera 0 --width 2048 --height 1536 --mode 2048:1536:12 --framerate 100 --denoise cdn_off --codec libav --libav-video-codec libx264 --bitrate 10000000 --low-latency -o "$dir/cam0.mkv"