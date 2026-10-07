#!/usr/bin/env bash
# Build the exact FFmpeg revision used by nx-on-orbis for the PS4.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/deps/ffmpeg"
BUILD="$ROOT/deps/build-ffmpeg"
TMP="$ROOT/deps/ffmpeg-tmp"
COMMIT="c7b5f1537d9c52efa50fd10d106ca015ddde1818"

source "$ROOT/tools/env-build.sh"

if [ ! -d "$SRC/.git" ]; then
  rm -rf "$SRC"
  git clone --filter=blob:none https://github.com/FFmpeg/FFmpeg.git "$SRC"
fi
git -C "$SRC" fetch --depth 1 origin "$COMMIT"
git -C "$SRC" checkout --detach "$COMMIT"

rm -rf "$BUILD" "$TMP"
mkdir -p "$BUILD" "$TMP"
cd "$BUILD"

TMPDIR="$TMP" "$SRC/configure"   --prefix="$PS4_PREFIX"   --enable-cross-compile   --arch=x86_64   --target-os=freebsd   --host-cc=cc   --host-ld=cc   --cc="$ROOT/tools/ps4-cc.sh"   --ld="$ROOT/tools/ps4-cc.sh"   --ar="$PS4_AR"   --ranlib="$PS4_RANLIB"   --nm="$(command -v llvm-nm-18 || command -v llvm-nm)"   --disable-x86asm   --disable-autodetect   --disable-everything   --disable-programs   --disable-doc   --disable-avdevice   --disable-avformat   --disable-network   --disable-swresample   --enable-decoder=h264,vp8,vp9   --enable-filter=yadif,scale   --enable-pic   --enable-pthreads   --disable-shared   --enable-static

make -j"$(nproc)"
make install

test -f "$PS4_PREFIX/lib/libavcodec.a"
test -f "$PS4_PREFIX/lib/libavutil.a"
echo "FFmpeg PS4 ready: $PS4_PREFIX"
