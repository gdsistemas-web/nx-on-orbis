#!/usr/bin/env bash
# Report what is ready/missing before configuring Eden on Linux.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[ -x "$ROOT/tools/cmake/bin/cmake" ] && export PATH="$ROOT/tools/cmake/bin:$PATH"
fail=0

ok(){ printf '  [OK]   %s\n' "$1"; }
miss(){ printf '  [MISS] %s\n' "$1"; fail=1; }

for t in cmake ninja clang clang++ llvm-ar llvm-ranlib llvm-nm ld.lld python3 make perl git; do
  command -v "$t" >/dev/null 2>&1 && ok "$t -> $(command -v "$t")" || miss "$t"
done

cmake_version="$(cmake --version 2>/dev/null | sed -n '1s/cmake version //p')"
cmake_major="$(printf '%s' "$cmake_version" | cut -d. -f1)"
cmake_minor="$(printf '%s' "$cmake_version" | cut -d. -f2)"
if [ "${cmake_major:-0}" -gt 3 ] || { [ "${cmake_major:-0}" -eq 3 ] && [ "${cmake_minor:-0}" -ge 31 ]; }; then
  ok "cmake version $cmake_version"
else
  miss "CMake >= 3.31 required (current: ${cmake_version:-unknown}; run scripts/bootstrap-cmake-linux.sh)"
fi

clang_major="$(clang --version 2>/dev/null | sed -n '1s/.*version \([0-9][0-9]*\).*/\1/p')"
if [ "$clang_major" = "18" ]; then
  ok "clang major version 18"
else
  miss "clang 18 required/preferred (current: ${clang_major:-unknown})"
fi

[ -f "$ROOT/sdk-dl/orbis-sdk-v1/BUNDLE.txt" ] && ok "orbis-sdk-v1" || miss "orbis-sdk-v1 (run scripts/bootstrap-linux.sh)"
[ -d "$ROOT/deps/eden/.git" ] && ok "patched Eden tree" || miss "deps/eden (run scripts/fetch-eden.sh)"
[ -d "$ROOT/libcxx18/include/c++/v1" ] && [ -f "$ROOT/libcxx18/lib/libc++.a" ]   && ok "libc++18 PS4" || miss "libc++18 PS4"
[ -f "$ROOT/prefix/lib/libssl.a" ] && [ -f "$ROOT/prefix/lib/libcrypto.a" ]   && ok "OpenSSL PS4" || miss "OpenSSL PS4"
[ -f "$ROOT/prefix/lib/libavcodec.a" ] && [ -f "$ROOT/prefix/lib/libavutil.a" ]   && ok "FFmpeg PS4" || miss "FFmpeg PS4"

echo
if [ "$fail" -eq 0 ]; then
  echo "preflight: READY"
  echo "run: bash configure-eden.sh"
else
  echo "preflight: dependencies still missing"
fi
exit "$fail"
