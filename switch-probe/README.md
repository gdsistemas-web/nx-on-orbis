# GD Probe (Switch homebrew)

Experimental open-source NRO test program for NX on Orbis. Does **not** call `launchInit()` and therefore does not itself depend on nx-hbloader. It uses **libnx**, so Eden may still lack services required during startup, console rendering or controller polling.

Based on the standard libnx application Makefile template from [switchbrew/switch-examples](https://github.com/switchbrew/switch-examples/tree/master/templates/application).

## Build prerequisites

Install the devkitPro Switch toolchain (`devkitA64`, `libnx`, `switch-tools`) using the official devkitPro package manager. The `switch-dev` package group normally supplies these; refer to https://switchbrew.org/wiki/Setting_up_Development_Environment .

Verify:

```bash
echo "$DEVKITPRO"
command -v aarch64-none-elf-gcc
```

Build from the repo root:

```bash
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export DEVKITA64="$DEVKITPRO/devkitA64"
export PATH="$DEVKITA64/bin:$PATH"
make -C switch-probe
file switch-probe/gd-probe.nro
```

The PS4 package script includes `switch-probe/gd-probe.nro` **when present**; otherwise GD Test 34 can still be packaged with the bundled hbmenu fallback.

On the PS4, GD Probe should appear in the native library. It displays an arithmetic checksum, a 1 MiB allocation-and-verify result, the frame-present counter and basic input events. This measures functional pathways, **not FPS**. If launch fails, inspect `/data/edenps4/boot.log` to identify missing HLE services. Do not infer game compatibility from this test.
