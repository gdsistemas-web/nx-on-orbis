# Source this file. Sets paths used to cross-build Eden dependencies for the PS4.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK_BUNDLE="${ORBIS_SDK_BUNDLE:-$ROOT/sdk-dl/orbis-sdk-v1}"

if [ ! -f "$SDK_BUNDLE/BUNDLE.txt" ]; then
  echo "missing PS4 SDK bundle at $SDK_BUNDLE" >&2
  echo "run: bash $ROOT/scripts/bootstrap-linux.sh" >&2
  return 1 2>/dev/null || exit 1
fi

# The bundle is relocatable and carries the matched SDK/orbis-compat/Mesa set.
# shellcheck disable=SC1090
source "$SDK_BUNDLE/env.sh"

export PS4_SDK="$SDK_BUNDLE/sdk"
export PS4_COMPAT="$SDK_BUNDLE/orbis-compat"
export PS4_OVERLAY="$ROOT/toolchain/include-overlay"
export PS4_PREFIX="$ROOT/prefix"

# Prefer repo-local helper tools, but use the Linux host LLVM/CMake/Ninja from PATH.
export PATH="$ROOT/tools/bin:$PATH"

export PS4_CFLAGS="--target=x86_64-pc-freebsd12-elf -O2 -fPIC -funwind-tables -D__PS4__ -DPS4 -DORBIS -D__ORBIS__ -D_BSD_SOURCE=1 -U__FreeBSD__ -isysroot $PS4_SDK -isystem $PS4_OVERLAY -isystem $PS4_COMPAT/include -isystem $PS4_SDK/include -include orbis_prefix.h"
