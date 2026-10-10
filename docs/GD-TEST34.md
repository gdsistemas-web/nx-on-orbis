# GD Test 34 — Standalone GD Probe NRO (experimental)

## Hardware baseline

PS4 Fat (GoldHEN): GD Test 33 menu and settings work. The native diagnostics page correctly reads `boot.old.log`; screenshots show previous `gd-test33-20261010T020246Z-0498b1e860c4`, fastmem OFF, and 59.9 FPS/100% emulation speed at 16.7 ms/frame in bundled hbmenu. The hbmenu prints `launchInit() failed` (requires hbloader), which is not proof of an emulator crash.

## New functionality

- Build ID `gd-test34-...` (reconfigure CMake before building).
- Separate PS4 Title ID `EDPS00034`.
- Native frontend updated to display GD Test 34.
- Independent `switch-probe/source/main.c` devkitA64/libnx homebrew:
  * Console rendering
  * 64-bit CPU checksum loop
  * 1 MiB allocation/fill/readback
  * Frame presentation counter (not FPS)
  * A/B/up/down input event tracking and + to exit.
- The NRO has no `launchInit()` call; however its libnx startup and services are still dependent on the emulator's HLE, which may not be implemented.
- `package-eden.sh` includes `switch-probe/gd-probe.nro` **only if built** and verifies its NRO0 header magic.
- When bundled, GD Probe appears as the first item in the game library; hbmenu remains a second option. User ROMs retain existing functionality. Without the probe binary, fallback hbmenu remains available.

## First verify the Switch toolchain on Linux Mint

```bash
command -v aarch64-none-elf-gcc || true
test -e /opt/devkitpro/libnx/switch_rules && echo "libnx installed" || echo "libnx missing"
echo "DEVKITPRO=${DEVKITPRO:-not-set}"
```

Install devkitPro's `switch-dev` group using their official package manager if required. Instructions: https://switchbrew.org/wiki/Setting_up_Development_Environment .

```bash
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export DEVKITA64="$DEVKITPRO/devkitA64"
export PATH="$DEVKITA64/bin:$PATH"
make -C switch-probe
ls -lh switch-probe/gd-probe.nro
```

## Compile emulator and package

```bash
git fetch origin
git switch --track origin/dev/gd-test34
source tools/env-build.sh
unset OO_PS4_TOOLCHAIN
bash configure-eden.sh
cmake --build build-eden --target eden-ps4 -j4
grep -aEom2 'gd-test34|GD TEST 34' build-eden/bin/eden-ps4
bash package-eden.sh
ls -lh out-eden/*EDPS00034*.pkg
```

Verify packaging prints `including built-in GD Probe` and not the fallback-only warning. Do not install or publish unless the build completes successfully.

## Console test

1. Install the EDPS00034 package independently.
2. Confirm both GD Probe and Homebrew Menu are listed. Select GD Probe.
3. Check whether the homebrew prints its test status and responds to A/B/up/down.
4. Recover `/data/edenps4/boot.log`, locate any HLE startup failures, and keep the exact ELF.
5. Compare existing hbmenu FPS with fastmem OFF. Do not assume probe frame counter measures FPS.

**Unvalidated:** NRO build and runtime on PS4 Fat must be tested. No commercial-game compatibility claim.
