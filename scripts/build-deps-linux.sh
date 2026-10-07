#!/usr/bin/env bash
# Build every locally-generated dependency required by Eden on PS4.
# Existing completed dependencies are skipped so failed runs can resume cheaply.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if [ -f "$ROOT/libcxx18/lib/libc++.a" ] && [ -f "$ROOT/libcxx18/lib/libc++abi.a" ]; then
  echo "== libc++18 PS4 already ready; skipping"
else
  bash "$ROOT/scripts/build-libcxx18-linux.sh"
fi

CFG="$ROOT/libcxx18/include/c++/v1/__config_site"
if [ -f "$CFG" ] && ! grep -q '^#undef __FreeBSD__$' "$CFG"; then
  printf '\n#undef __FreeBSD__\n' >> "$CFG"
  echo "== normalized libc++ target macros for Orbis"
fi

if [ -f "$ROOT/prefix/lib/libssl.a" ] && [ -f "$ROOT/prefix/lib/libcrypto.a" ]; then
  echo "== OpenSSL PS4 already ready; skipping"
else
  bash "$ROOT/scripts/build-openssl-linux.sh"
fi

if [ -f "$ROOT/prefix/lib/libavcodec.a" ] && [ -f "$ROOT/prefix/lib/libavutil.a" ]; then
  echo "== FFmpeg PS4 already ready; skipping"
else
  bash "$ROOT/scripts/build-ffmpeg-linux.sh"
fi

echo
bash "$ROOT/scripts/preflight-linux.sh"
