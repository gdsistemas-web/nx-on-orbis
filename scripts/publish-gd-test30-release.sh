#!/usr/bin/env bash
# Publish the frozen GD Test 30 PKG + exact ELF as a GitHub pre-release.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

REPO="gdsistemas-web/nx-on-orbis"
TAG="v0.2.0-test30"
TARGET="dev/fastmem-v1"
TITLE="NX on Orbis v0.2.0-test30 — GD Test 30"
NOTES="docs/releases/v0.2.0-test30.md"

PKG="out-eden/IV0000-EDPS00001_00-EDENPS4000000000.pkg"
ELF="elf/eden-ps4-gd-test30-20261007-2012.elf"

PKG_SHA="3437859813eca8aaeddfb2173f32c99e4d196a73f97c790ba49aff6b42bfc547"
ELF_SHA="87ba17b441cbf49af6506f8439ad31458e1c985b9a990ec15865d38e205ea3f9"

command -v gh >/dev/null 2>&1 || {
  echo "missing GitHub CLI (gh)" >&2
  exit 2
}

[ -f "$PKG" ] || { echo "missing $PKG" >&2; exit 3; }
[ -f "$ELF" ] || { echo "missing $ELF" >&2; exit 3; }
[ -f "$NOTES" ] || { echo "missing $NOTES" >&2; exit 3; }

echo "$PKG_SHA  $PKG" | sha256sum -c -
echo "$ELF_SHA  $ELF" | sha256sum -c -

gh auth status >/dev/null

if gh release view "$TAG" --repo "$REPO" >/dev/null 2>&1; then
  echo "release $TAG already exists"
  gh release view "$TAG" --repo "$REPO" --web
  exit 0
fi

gh release create "$TAG"   --repo "$REPO"   --target "$TARGET"   --prerelease   --title "$TITLE"   --notes-file "$NOTES"   "$PKG#PS4 PKG — GD Test 30"   "$ELF#Exact ELF — keep for crash symbolization"

echo
echo "published pre-release:"
gh release view "$TAG" --repo "$REPO"
