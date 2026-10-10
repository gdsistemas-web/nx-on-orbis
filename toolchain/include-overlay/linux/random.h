#pragma once
/*
 * PS4 cross-build guard.
 *
 * libc++ random.cpp conditionally includes <linux/random.h> when
 * __has_include() finds it. On a Linux build host Clang can otherwise see the
 * host kernel header even while targeting Orbis, which then drags in
 * /usr/include/linux/types.h and host asm headers.
 *
 * Orbis uses libc++'s /dev/random path here. Deliberately leave Linux-only
 * RNDGETENTCNT undefined so random_device::entropy() falls back to 0 instead
 * of compiling the Linux ioctl path.
 */
