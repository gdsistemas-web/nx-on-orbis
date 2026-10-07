# GD Test 30 — Linux build, fastmem diagnostics and PS4 test plan

Status: **built and packaged on Linux; console validation pending**  
Branch: `dev/fastmem-v1`  
Base: `experimental-fastmem`  
Build ID: `gd-test30`  
Build date: 2026-10-07  
Planned first console test: 2026-10-09 evening (PS4 Pro / GoldHEN), subject to availability.

## Goal

Test 30 continues the experimental fastmem work from tests 25–29. The immediate goal is not another
blind performance tweak: it is to identify which guest pages cause the very large Dynarmic slow-path
redirect count seen with the fastmem view active.

The branch adds fixed-size, exception-handler-safe redirect hotspot diagnostics and a complete Linux
build/package path so development can continue without the original author's Windows-only machine.

## Baseline inherited from experimental-fastmem

The fastmem branch already had:

- an 8 GiB capped guest-address-space view;
- 16 KiB host view pages over 4 KiB guest subpages;
- lazy alias mapping into direct memory;
- fault-handler redirect into Dynarmic's slow path;
- redirect cause counters;
- startup fastmem self-tests;
- direct-memory diagnostics;
- reduced GPU arena experiments.

Tests 25–29 proved that the fastmem view can become active, but MK8D race loading ran into direct
memory pressure and the redirect counter remained extremely high. Test 30 instruments the hot pages
so the next change can be based on measured fault locality.

## Test 30 diagnostics

The Eden patch series now contains the Test 30 hotspot tracker.

Key properties:

- fixed table of 1024 slots;
- at most 8 probes per record operation;
- no allocation, logging or locks inside the exception/fault path;
- 4 KiB guest page key;
- atomic page key, count and last redirect reason;
- collision-drop counter;
- frontend prints the top redirect pages approximately every 30 seconds.

Expected log shape:

```text
redirect hotspot table: <tracked> tracked pages, <drops> collision drops; top <n>
#1 page 0x........: <count> redirects (cause <reason>)
#2 page 0x........: <count> redirects (cause <reason>)
...
```

Redirect reasons inherited from the previous diagnostic patch:

- `0`: outside fastmem view;
- `1`: no entry or the 16 KiB entry is not fully valid;
- `2`: not fully readable;
- `3`: mapped read-only / inferred write fault;
- `4`: other.

The main question for Test 30 is whether the redirect storm is concentrated in a small number of
4 KiB guest pages, especially with cause 1, or spread across a large working set.

## Linux build environment

The continuation was brought up on Linux Mint with the relocatable `orbis-sdk-v1` bundle.

Pinned/used components:

| Component | Version / revision |
|---|---|
| Eden | `5f142c7926d0c7fcbbd0ce30794d72f638a43b2a` + local patch series |
| OpenOrbis SDK | 0.5.4 via `orbis-sdk-v1` |
| LLVM libc++ / libc++abi | `llvmorg-18.1.8` |
| Host compiler | Clang 18 |
| CMake | repo-local 3.31.12 |
| OpenSSL | 3.6.2 |
| FFmpeg | `c7b5f1537d9c52efa50fd10d106ca015ddde1818` |
| Generator | Ninja |

The system CMake (3.28.3) was intentionally left untouched. The project bootstraps CMake 3.31.12
under `tools/cmake/` and prepends that copy to PATH.

## Linux bootstrap and dependency build

From a clean checkout of `dev/fastmem-v1`:

```bash
bash scripts/bootstrap-linux.sh
bash scripts/bootstrap-cmake-linux.sh
bash scripts/fetch-eden.sh
bash scripts/build-deps-linux.sh
```

The dependency orchestrator is resumable: completed libc++18, OpenSSL and FFmpeg outputs are detected
and skipped on subsequent runs.

A successful preflight ends with:

```text
[OK]   orbis-sdk-v1
[OK]   patched Eden tree
[OK]   libc++18 PS4
[OK]   OpenSSL PS4
[OK]   FFmpeg PS4

preflight: READY
run: bash configure-eden.sh
```

## Problems found while bringing up Linux

### LLVM sparse checkout missed llvm-lit

The runtimes CMake configuration references `llvm/utils/llvm-lit` even with runtime tests disabled.
The sparse checkout now includes that directory and repairs an already-existing checkout.

### libc++ reported "No thread API"

The Orbis target deliberately undefines `__FreeBSD__`, so libc++ could not auto-select pthreads.
The runtime build now explicitly sets:

```text
LIBCXX_HAS_PTHREAD_API=ON
LIBCXXABI_HAS_PTHREAD_API=ON
```

### Host Linux random header leaked into the PS4 cross-build

`libcxx/src/random.cpp` uses `__has_include(<linux/random.h>)`. On the Linux host that test could
see `/usr/include/linux/random.h`, which then pulled host Linux headers into the PS4 build.

A PS4 include-overlay guard prevents that host header from participating in the cross-build.

### libc++ selected the wrong sendfile implementation

OpenOrbis exposes the BSD/FreeBSD `sendfile` signature, while libc++18's detected sendfile path uses
the Linux file-to-file form. This produced an argument-count error in
`libcxx/src/filesystem/operations.cpp`.

The local LLVM patch disables libc++'s sendfile copy path on `__ORBIS__` and uses the fstream
fallback instead. The patch is applied automatically by `scripts/build-libcxx18-linux.sh`.

### FFmpeg compiler wrapper was not executable

FFmpeg requires the configured `--cc` / `--ld` wrapper to be executable. The Linux FFmpeg builder
now ensures `tools/ps4-cc.sh` is executable before configure.

### Eden requires newer CMake

The Eden tree requires CMake >= 3.31. Linux Mint provided 3.28.3. A repo-local CMake 3.31.12 bootstrap
was added rather than replacing the host package.

## Build Eden

```bash
rm -rf build-eden
bash configure-eden.sh
cmake --build build-eden --target eden-ps4 -j"$(nproc)"
```

First successful Linux Test 30 build completed with:

```text
[1550/1550] Linking CXX executable bin/eden-ps4
```

Resulting ELF before packaging:

```text
build-eden/bin/eden-ps4
```

## Linux packaging

`package-eden.sh` was ported from the original Windows-only flow to the Linux tools shipped in
`orbis-sdk-v1`.

Run:

```bash
bash package-eden.sh
```

The script:

1. converts the Eden ELF to a fake-signed PS4 eboot using Linux `create-fself`;
2. preserves paid `0x3800000000000011`;
3. uses the bundle's Linux `PkgTool.Core` / `create-gp4` packaging path;
4. fetches the Piglet package modules from the exact OpenOrbis `v0.5.4` tag because the SDK bundle
   intentionally prunes sample modules;
5. verifies those downloaded files by their Git blob IDs;
6. packages the bundled nx-hbmenu test program;
7. keeps an exact ELF copy for later crash symbolization.

Pinned Piglet assets:

| File | OpenOrbis v0.5.4 Git blob |
|---|---|
| `sce_module/libc.prx` | `e49b25d011281d52131a34d5a18106dd49605919` |
| `sce_module/libSceFios2.prx` | `1f83bdc8f1fd0ddf6a56f0d5faa1d914e35095fa` |
| `sce_sys/about/right.sprx` | `aed25c03faae7aba45a2871b6fe443fa6a70f5d7` |

No Switch keys, firmware or commercial games are downloaded or included.

## First successful Linux package

Generated on 2026-10-07:

```text
out-eden/IV0000-EDPS00001_00-EDENPS4000000000.pkg
size reported by ls: 58M

elf/eden-ps4-gd-test30-20261007-2012.elf
size reported by ls: 70M
```

The exact ELF must be kept with the installed package. Any `eboot+0x...` crash offset from this
build must be symbolized against `eden-ps4-gd-test30-20261007-2012.elf`, not against a later build.

SHA-256 values were not recorded at documentation time and should be captured before moving the
artifacts to the console:

```bash
sha256sum \
  out-eden/IV0000-EDPS00001_00-EDENPS4000000000.pkg \
  elf/eden-ps4-gd-test30-20261007-2012.elf
```

## Console test plan

Target for the first validation is the same class of environment used by the upstream experiment:
PS4 Pro with GoldHEN. Test 30 has not yet been run on a console.

Recommended order:

1. Install the generated PKG.
2. Launch NX on Orbis and confirm the Test 30 build banner.
3. Confirm the game picker and DualShock 4 input.
4. Run the bundled Homebrew Menu first.
5. Confirm fastmem startup/self-test state in the logs.
6. If stable, launch a user-dumped game.
7. For MK8D, reach a race if possible and let the session run long enough to print multiple hotspot
   samples.
8. Preserve all logs before replacing the build.

Collect:

```text
/data/edenps4/boot.log
/data/edenps4/mesa.log
/data/edenps4/log/eden_log.txt
```

## How to interpret Test 30

### A few pages dominate, mostly cause 1

This would support the hypothesis that the 4 KiB guest mapping model is fighting the PS4's 16 KiB
host page granularity. The next diagnostic should break cause 1 down into null-entry versus partial
valid-mask cases and print valid/read/write masks for the hot entries.

### Thousands of pages are hot

The redirect storm is broad rather than localized. A hybrid fastmem/memory strategy is more likely
to help than special-casing a handful of pages.

### Race load fails from direct-memory pressure

Memory work remains the priority before performance work. First candidates inherited from the
previous investigation:

- reduce the GPU stream buffer from 256 MiB to 128 MiB;
- lower texture-cache budgets;
- inspect the current `OnGuestRamFreed` fastmem path, which returns early while the view is active
  and may retain backing memory;
- use `ORBIS_VRAM_GARLIC=0` only as a diagnostic because it is expected to hurt GPU performance
  substantially.

### Fastmem is stable and races load

Compare the Test 30 redirect distribution against the earlier ~368k–370k redirects/session and use
the hotspot data to choose Test 31.

## Useful symbolization command

For an offset reported as `eboot+0xOFFSET`:

```bash
llvm-symbolizer -obj=elf/eden-ps4-gd-test30-20261007-2012.elf -C -f 0xOFFSET
```

Do not symbolize Test 30 crashes with an ELF produced by a later rebuild.

## Key continuation commits

The branch contains, among others:

- `57d584c4` / `094e0a78` — initial fastmem redirect hotspot diagnostic + hunk fix;
- `d64a1163` — frontend periodic top-hotspot logging;
- `9cf586ef` — `gd-test30` build ID;
- `de762a40` — repair LLVM sparse checkout with `llvm-lit`;
- `ca397d92` — resumable Linux dependency build;
- `a270fcc1` — force pthread API for PS4 libc++ / libc++abi;
- `171fd372` — prevent Linux random header leakage;
- `0f637d56` / `600ccb17` — permanent Orbis libc++ filesystem patch integration;
- `09747d69` — repair dependency orchestrator;
- `338cb93f` — make FFmpeg PS4 compiler wrapper executable;
- `45e68990` through `15775859` — repo-local CMake 3.31 bootstrap and guards;
- `3ebb4e19` — Linux package flow;
- `65310375` — Linux package documentation.

## Current state

As of 2026-10-07:

- Linux dependency preflight: **PASS**
- Eden configure: **PASS**
- Eden compile/link: **PASS (1550/1550)**
- Linux fake-PKG packaging: **PASS**
- Test 30 console boot: **PENDING**
- Fastmem hotspot data from real PS4 workload: **PENDING**
- MK8D race validation: **PENDING**

Do not treat Test 30 as validated until the generated PKG has been run on hardware and the logs above
have been collected.
