#!/usr/bin/env bash
# Build every locally-generated dependency required by Eden on PS4.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

bash "$ROOT/scripts/build-libcxx18-linux.sh"
bash "$ROOT/scripts/build-openssl-linux.sh"
bash "$ROOT/scripts/build-ffmpeg-linux.sh"

echo
bash "$ROOT/scripts/preflight-linux.sh"
