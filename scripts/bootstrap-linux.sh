#!/usr/bin/env bash
# Bootstrap the proven orbis-sdk-v1 bundle on a Linux host.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DST="$ROOT/sdk-dl/orbis-sdk-v1"
TAR="$ROOT/sdk-dl/orbis-sdk-v1.tar.gz"
URL="https://github.com/orbis-ports/orbis-porting-kit/releases/download/orbis-sdk-v1/orbis-sdk-v1.tar.gz"
SHA="f4f079a4df334b46a76996f895283904283d6a006f97102aafaa53d4ecfc9622"

need() { command -v "$1" >/dev/null 2>&1 || { echo "missing host tool: $1" >&2; exit 2; }; }
for t in curl tar sha256sum cmake ninja clang clang++ llvm-ar llvm-ranlib ld.lld python3 make perl git; do
  need "$t"
done

mkdir -p "$ROOT/sdk-dl"
if [ ! -f "$TAR" ]; then
  echo "downloading orbis-sdk-v1..."
  curl -L --fail --retry 3 -o "$TAR" "$URL"
fi
echo "$SHA  $TAR" | sha256sum -c -

if [ ! -f "$DST/BUNDLE.txt" ]; then
  rm -rf "$DST"
  tar -xzf "$TAR" -C "$ROOT/sdk-dl"
fi

[ -f "$DST/BUNDLE.txt" ] || { echo "SDK extraction did not create $DST" >&2; exit 3; }
chmod +x "$DST"/env.sh "$DST"/verify.sh 2>/dev/null || true
"$DST/verify.sh" "$DST"

echo
echo "SDK ready: $DST"
echo "next: bash scripts/preflight-linux.sh"
