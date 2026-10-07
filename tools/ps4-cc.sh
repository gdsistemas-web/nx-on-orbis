#!/usr/bin/env bash
# clang wrapper for PS4 autoconf-style dependency builds (FFmpeg etc.).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck disable=SC1091
source "$ROOT/tools/env-build.sh"

args=()
link=true
for a in "$@"; do
    case "$a" in
        -c|-E|-S|-M|-MM|--version) link=false; args+=("$a") ;;
        -lm|-lpthread|-pthread) ;;
        *) args+=("$a") ;;
    esac
done

if $link; then
    args+=(-nostdlib -fuse-ld=lld -pie -Wl,-m,elf_x86_64
           -Wl,--script="$PS4_COMPAT/cmake/orbis-tls.ld"
           -Wl,--eh-frame-hdr -Wl,--no-rosegment
           -L "$ROOT/libcxx18/lib" -L "$PS4_SDK/lib"
           -Wl,--whole-archive "$PS4_COMPAT/build/liborbis-compat.a" -Wl,--no-whole-archive
           -lc++ -lc++abi -lunwind -lc -lkernel "$PS4_SDK/lib/crt1.o")
fi

exec "$PS4_CC" $PS4_CFLAGS -ffunction-sections -fdata-sections "${args[@]}"
