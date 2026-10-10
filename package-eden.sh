#!/usr/bin/env bash
# build-eden/bin/eden-ps4 (ELF) -> out-eden/IV0000-EDPS00034_00-EDENPS4000000000.pkg
#
# Linux packaging path for the relocatable orbis-sdk-v1 bundle.
# Preserves the package layout proven on the original PS4 Pro test console:
# paid 0x3800000000000011, libc.prx + libSceFios2.prx, right.sprx, category gd.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
source "$ROOT/tools/env-build.sh"

TITLE="NX on Orbis - GD Test 34"
TITLE_ID="EDPS00034"
CONTENT_LABEL="EDENPS4000000000"
CID="IV0000-${TITLE_ID}_00-${CONTENT_LABEL}"
ELF="${ELF:-$ROOT/build-eden/bin/eden-ps4}"
ST="$ROOT/stage-eden"
OUT="$ROOT/out-eden"
CACHE="$ROOT/deps/openorbis-v0.5.4-piglet"
ICON="$ST/icon0.png"

FSELF="$ORBIS_PKG_TOOLS/create-fself"
PKG_SCRIPT="$ORBIS_COMPAT_DIR/scripts/ps4/make-pkg.sh"

die() { echo "package-eden: $*" >&2; exit 1; }

[ -f "$ELF" ] || die "missing ELF: $ELF"
[ -x "$FSELF" ] || chmod +x "$FSELF" 2>/dev/null || true
[ -x "$FSELF" ] || die "create-fself not executable at $FSELF"
[ -f "$PKG_SCRIPT" ] || die "missing package helper: $PKG_SCRIPT"
command -v curl >/dev/null 2>&1 || die "curl is required"
command -v git >/dev/null 2>&1 || die "git is required"
command -v python3 >/dev/null 2>&1 || die "python3 is required"

rm -rf "$ST"
mkdir -p "$ST" "$OUT" "$CACHE" "$ROOT/elf"

fetch_blob() {
  local rel="$1"
  local blob_sha="$2"
  local dst="$CACHE/$rel"
  local url="https://raw.githubusercontent.com/OpenOrbis/OpenOrbis-PS4-Toolchain/v0.5.4/samples/piglet/$rel"

  mkdir -p "$(dirname "$dst")"
  if [ ! -f "$dst" ]; then
    echo "fetching OpenOrbis v0.5.4 sample asset: $rel"
    curl -L --fail --retry 3 -o "$dst" "$url"
  fi

  local got
  got="$(git hash-object "$dst")"
  if [ "$got" != "$blob_sha" ]; then
    rm -f "$dst"
    die "integrity check failed for $rel (got $got, expected $blob_sha)"
  fi
}

# orbis-sdk-v1 intentionally prunes sample modules. Fetch the exact files from
# the OpenOrbis v0.5.4 tag used by this port and verify their Git blob IDs.
fetch_blob "sce_module/libc.prx"         "e49b25d011281d52131a34d5a18106dd49605919"
fetch_blob "sce_module/libSceFios2.prx"  "1f83bdc8f1fd0ddf6a56f0d5faa1d914e35095fa"
fetch_blob "sce_sys/about/right.sprx"     "aed25c03faae7aba45a2871b6fe443fa6a70f5d7"

if python3 -c 'from PIL import Image, ImageDraw, ImageFont' >/dev/null 2>&1; then
  python3 "$ROOT/probe/make-icon.py" "$ICON" "NX"
else
  echo "warning: python3 Pillow not installed; using OpenOrbis v0.5.4 Piglet icon"
  fetch_blob "sce_sys/icon0.png" "449b3a05f9cff5135c09af0d4524986c0663c749"
  cp "$CACHE/sce_sys/icon0.png" "$ICON"
fi

echo "creating fake-signed eboot..."
"$FSELF" -in="$ELF" -out="$ST/eden-ps4.oelf" \
  --eboot "$ST/eboot.bin" --paid 0x3800000000000011 >/dev/null
rm -f "$ST/eden-ps4.oelf"

PROBE_NRO="$ROOT/switch-probe/gd-probe.nro"
PROBE_EXTRA=()
if [ -f "$PROBE_NRO" ]; then
  # NRO0 magic appears at byte offset 0x10 of a Nintendo Switch NRO.
  python3 - "$PROBE_NRO" <<'PY'
import sys
with open(sys.argv[1], "rb") as f:
    f.seek(16)
    magic = f.read(4)
if magic != b"NRO0":
    raise SystemExit(f"Invalid NRO (expected NRO0 at offset 0x10): {sys.argv[1]}")
PY
  PROBE_EXTRA=(--extra "$PROBE_NRO:assets/misc/gd-probe.nro")
  echo "including built-in GD Probe: $PROBE_NRO"
else
  echo "warning: GD Probe not built; package will include only the hbmenu fallback"
  echo "         run: make -C switch-probe  (requires devkitPro Switch SDK)"
fi

echo "building $CID.pkg..."
bash "$PKG_SCRIPT" \
  --eboot "$ST/eboot.bin" \
  --out-dir "$OUT" \
  --title-id "$TITLE_ID" \
  --title "$TITLE" \
  --version "01.00" \
  --content-label "$CONTENT_LABEL" \
  --icon "$ICON" \
  --sdk "$OO_PS4_TOOLCHAIN" \
  --extra "$CACHE/sce_sys/about/right.sprx:sce_sys/about/right.sprx" \
  --extra "$CACHE/sce_module/libc.prx:sce_module/libc.prx" \
  --extra "$CACHE/sce_module/libSceFios2.prx:sce_module/libSceFios2.prx" \
  --extra "$ROOT/testroms/hbmenu.nro:assets/misc/hbmenu.nro" \
  --extra "$ROOT/testroms/hbmenu-LICENSE.txt:assets/misc/hbmenu-LICENSE.txt" \
  "${PROBE_EXTRA[@]}"

PKG="$OUT/$CID.pkg"
[ -f "$PKG" ] || die "package helper returned without producing $PKG"

STAMP="$(date +%Y%m%d-%H%M)"
ELF_COPY="$ROOT/elf/eden-ps4-gd-test34-$STAMP.elf"
cp "$ELF" "$ELF_COPY"

echo
echo "package ready:"
ls -lh "$PKG"
echo "ELF kept for symbolization:"
ls -lh "$ELF_COPY"
