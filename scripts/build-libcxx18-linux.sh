#!/usr/bin/env bash
# Build LLVM 18.1.8 libc++/libc++abi for the PS4.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/deps/llvm-project"
BUILD="$ROOT/build-libcxx18"
PREFIX="$ROOT/libcxx18"
TAG="llvmorg-18.1.8"

source "$ROOT/tools/env-build.sh"

if [ ! -d "$SRC/.git" ]; then
  rm -rf "$SRC"
  git clone --depth 1 --filter=blob:none --sparse --branch "$TAG"     https://github.com/llvm/llvm-project.git "$SRC"
  git -C "$SRC" sparse-checkout set runtimes libcxx libcxxabi libunwind cmake llvm/cmake
fi

rm -rf "$BUILD"
cmake -S "$SRC/runtimes" -B "$BUILD" -G Ninja   -DCMAKE_TOOLCHAIN_FILE="$ROOT/toolchain/runtimes-ps4.cmake"   -DCMAKE_BUILD_TYPE=Release   -DCMAKE_INSTALL_PREFIX="$PREFIX"   -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi"   -DLIBCXX_ENABLE_SHARED=OFF   -DLIBCXXABI_ENABLE_SHARED=OFF   -DLIBCXX_HAS_MUSL_LIBC=ON   -DLIBCXX_CXX_ABI=libcxxabi   -DLIBCXX_ENABLE_NEW_DELETE_DEFINITIONS=OFF   -DLIBCXXABI_USE_LLVM_UNWINDER=OFF   -DLIBCXX_ENABLE_TIME_ZONE_DATABASE=OFF   -DLIBCXX_INCLUDE_BENCHMARKS=OFF   -DLIBCXX_INCLUDE_TESTS=OFF   -DLIBCXXABI_INCLUDE_TESTS=OFF

cmake --build "$BUILD" -j"$(nproc)"
cmake --install "$BUILD"

CFG="$PREFIX/include/c++/v1/__config_site"
if [ -f "$CFG" ] && ! grep -q '^#undef __FreeBSD__$' "$CFG"; then
  printf '\n#undef __FreeBSD__\n' >> "$CFG"
fi

test -f "$PREFIX/lib/libc++.a"
test -f "$PREFIX/lib/libc++abi.a"
echo "libc++18 PS4 ready: $PREFIX"
