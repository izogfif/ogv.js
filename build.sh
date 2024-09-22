#!/bin/bash
set -euo pipefail
docker compose up --menu=false
cd ../ffmpeg.wasm
./build.sh
