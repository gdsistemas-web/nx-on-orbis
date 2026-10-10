#!/usr/bin/env bash
# Install a repo-local CMake new enough for Eden without replacing the host CMake.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="3.31.12"
ARCHIVE="cmake-${VERSION}-linux-x86_64.tar.gz"
URL="https://cmake.org/files/v3.31/${ARCHIVE}"
SHA256="0dc2e9a6860f06bf10bd8fadc03e35d9eeb4df46e33763a7e480e987758f385c"
CACHE="$ROOT/sdk-dl/$ARCHIVE"
DST="$ROOT/tools/cmake"

for t in curl tar sha256sum; do
  command -v "$t" >/dev/null 2>&1 || { echo "missing host tool: $t" >&2; exit 2; }
done

mkdir -p "$ROOT/sdk-dl" "$ROOT/tools"

if [ ! -f "$CACHE" ]; then
  echo "downloading CMake $VERSION..."
  curl -L --fail --retry 3 -o "$CACHE" "$URL"
fi

echo "$SHA256  $CACHE" | sha256sum -c -

rm -rf "$DST"
mkdir -p "$DST"
tar -xzf "$CACHE" --strip-components=1 -C "$DST"

"$DST/bin/cmake" --version
echo "CMake $VERSION ready: $DST"
