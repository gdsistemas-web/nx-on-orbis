#!/usr/bin/env bash
# Configures Eden (deps/eden) for the PS4 into build-eden/.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
source "$ROOT/tools/env-build.sh"

CMAKE_VERSION="$(cmake --version | sed -n '1s/cmake version //p')"
CMAKE_MAJOR="$(printf '%s' "$CMAKE_VERSION" | cut -d. -f1)"
CMAKE_MINOR="$(printf '%s' "$CMAKE_VERSION" | cut -d. -f2)"
if ! { [ "${CMAKE_MAJOR:-0}" -gt 3 ] || { [ "${CMAKE_MAJOR:-0}" -eq 3 ] && [ "${CMAKE_MINOR:-0}" -ge 31 ]; }; }; then
  echo "Eden requires CMake >= 3.31; found $CMAKE_VERSION" >&2
  echo "run: bash scripts/bootstrap-cmake-linux.sh" >&2
  exit 2
fi
echo "using CMake $CMAKE_VERSION ($(command -v cmake))"

cmake -S "$ROOT/deps/eden" -B "$ROOT/build-eden" -G Ninja \
 -DCMAKE_TOOLCHAIN_FILE="$ROOT/toolchain/eden-ps4.cmake" -DPS4_BUILD_PKG=OFF \
 -DCMAKE_BUILD_TYPE=Release -DENABLE_LTO=OFF -DENABLE_QT=OFF -DYUZU_CMD=OFF -DYUZU_ROOM=OFF \
 -DYUZU_ROOM_STANDALONE=OFF -DYUZU_TESTS=OFF -DBUILD_TESTING=OFF -DENABLE_OPENGL=OFF -DENABLE_CUBEB=OFF \
 -DENABLE_WEB_SERVICE=OFF -DENABLE_LIBUSB=OFF -DYUZU_CRASH_DUMPS=OFF -DENABLE_WERROR=OFF \
 -Dzstd_FORCE_BUNDLED=ON -DBoost_FORCE_BUNDLED=ON -Dfmt_FORCE_BUNDLED=ON -DDYNARMIC_ENABLE_NO_EXECUTE_SUPPORT=OFF \
 -DYUZU_USE_EXTERNAL_FFMPEG=OFF -DYUZU_DOWNLOAD_TIME_ZONE_DATA=ON \
 -DYUZU_USE_BUNDLED_OPENSSL=OFF -DOpenSSL_FORCE_SYSTEM=ON -DOPENSSL_ROOT_DIR="$PS4_PREFIX" \
 -DOPENSSL_INCLUDE_DIR="$PS4_PREFIX/include" -DOPENSSL_SSL_LIBRARY="$PS4_PREFIX/lib/libssl.a" \
 -DOPENSSL_CRYPTO_LIBRARY="$PS4_PREFIX/lib/libcrypto.a" \
 -U "FFmpeg_*" -DFFMPEG_DIR="$PS4_PREFIX" \
 -DCMAKE_PROJECT_yuzu_INCLUDE="$ROOT/frontend/inject.cmake" "$@"
