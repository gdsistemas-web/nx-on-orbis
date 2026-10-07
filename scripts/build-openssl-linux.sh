#!/usr/bin/env bash
# Build OpenSSL 3.6.2 static libraries for the PS4.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/deps/openssl-3.6.2"
TAG="openssl-3.6.2"

source "$ROOT/tools/env-build.sh"

if [ ! -d "$SRC/.git" ]; then
  rm -rf "$SRC"
  git clone --depth 1 --branch "$TAG" https://github.com/openssl/openssl.git "$SRC"
fi

cd "$SRC"
make distclean >/dev/null 2>&1 || true

CFLAGS="$PS4_CFLAGS" CC="$PS4_CC" AR="$PS4_AR" RANLIB="$PS4_RANLIB" perl Configure linux-x86_64-clang   no-shared no-tests no-apps no-docs no-dso no-async no-afalgeng no-engine   no-ui-console no-secure-memory no-module   --with-rand-seed=devrandom   --prefix="$PS4_PREFIX" --libdir=lib --openssldir=/app0/ssl

make -j"$(nproc)" build_libs
make install_dev

test -f "$PS4_PREFIX/lib/libssl.a"
test -f "$PS4_PREFIX/lib/libcrypto.a"
echo "OpenSSL PS4 ready: $PS4_PREFIX"
