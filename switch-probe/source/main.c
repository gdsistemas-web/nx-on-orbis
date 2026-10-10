// SPDX-License-Identifier: MIT
// Independent libnx probe for NX on Orbis. Console mode is the stable fallback.
#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define FB_W 1280
#define FB_H 720
#define TEST_BYTES (4u * 1024u * 1024u)
static volatile uint64_t cpu_checksum;

static void test_cpu(void) {
    uint64_t n = 0x123456789abcdef0ULL;
    for (unsigned i = 0; i < 100000; ++i) {
        n ^= n >> 13;
        n *= 0xff51afd7ed558ccdULL;
        n ^= n >> 27;
        n += i;
    }
    cpu_checksum = n;
}
static bool test_memory(size_t bytes) {
    uint8_t *mem = (uint8_t*)malloc(bytes);
    if (!mem) return false;
    for (size_t i = 0; i < bytes; ++i)
        mem[i] = (uint8_t)((i * 37u + 19u) & 255u);
    bool ok = true;
    for (size_t i = 0; i < bytes; ++i) {
        if (mem[i] != (uint8_t)((i * 37u + 19u) & 255u)) {
            ok = false; break;
        }
    }
    free(mem);
    return ok;
}
static double seconds(void) {
    struct timespec t = {0};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}
static void rect(uint32_t* pixels, uint32_t stride, int x, int y, int w, int h, uint32_t color) {
    for (int j = y; j < y + h && j < FB_H; ++j)
        for (int i = x; i < x + w && i < FB_W; ++i)
            pixels[(size_t)j * stride + i] = color;
}
static void graphic_frame(Framebuffer* fb, uint64_t frames, int speed) {
    u32 strideBytes;
    uint32_t* pixels = (uint32_t*)framebufferBegin(fb, &strideBytes);
    if (!pixels) return;
    const u32 pitch = strideBytes / 4;
    // Pixel format follows the official libnx simplegfx example (RGBA8888).
    const uint32_t bg = RGBA8_MAXALPHA(9, 18, 34);
    const uint32_t panel = RGBA8_MAXALPHA(23, 44, 68);
    const uint32_t orange = RGBA8_MAXALPHA(255, 107, 0);
    const uint32_t green = RGBA8_MAXALPHA(87, 212, 152);
    const uint32_t blue = RGBA8_MAXALPHA(36, 154, 245);
    for (int y = 0; y < FB_H; ++y)
        for (int x = 0; x < FB_W; ++x)
            pixels[(size_t)y * pitch + x] = bg;
    rect(pixels, pitch, 0, 0, FB_W, 15, orange);
    rect(pixels, pitch, 70, 76, 1140, 570, panel);
    const uint32_t colors[] = { orange, green, blue, RGBA8_MAXALPHA(255, 255, 255) };
    for (int i = 0; i < 4; ++i)
        rect(pixels, pitch, 115 + i * 265, 140, 205, 125, colors[i]);
    // Moving square: speed may be adjusted by left/right.
    const int x = 110 + (int)((frames * (uint64_t)speed) % 980);
    rect(pixels, pitch, x, 350, 94, 94, orange);
    rect(pixels, pitch, 110, 530, 1040, 12, blue);
    rect(pixels, pitch, 110, 530, (int)(frames % 1040), 12, green);
    framebufferEnd(fb);
}
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    test_cpu();
    const bool mem1 = test_memory(1024u * 1024u);
    const bool mem4 = test_memory(TEST_BYTES);
    unsigned events = 0;
    const char* last = "none";
    bool graphics = false, graphics_failed = false;
    Framebuffer fb;
    uint64_t frames = 0, window_frames = 0;
    double window_start = seconds(), fps = 0;
    int speed = 3;

    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 keys = padGetButtonsDown(&pad);
        if (keys & HidNpadButton_Plus) break;
        if (keys & HidNpadButton_Up) { last = "UP"; ++events; }
        if (keys & HidNpadButton_Down) { last = "DOWN"; ++events; }
        if (keys & HidNpadButton_Left) { last = "LEFT"; ++events; if (speed > 1) --speed; }
        if (keys & HidNpadButton_Right) { last = "RIGHT"; ++events; if (speed < 12) ++speed; }
        if (keys & HidNpadButton_A) {
            last = "A"; ++events;
            if (!graphics) {
                // Do not hold console and raw framebuffer concurrently.
                consoleExit(NULL);
                const Result rc = framebufferCreate(&fb, nwindowGetDefault(), FB_W, FB_H,
                                                     PIXEL_FORMAT_RGBA_8888, 2);
                if (R_SUCCEEDED(rc)) {
                    framebufferMakeLinear(&fb);
                    graphics = true;
                } else {
                    graphics_failed = true;
                    consoleInit(NULL);
                }
            }
        }
        if (keys & HidNpadButton_B) {
            last = "B"; ++events;
            if (graphics) {
                framebufferClose(&fb);
                graphics = false;
                consoleInit(NULL);
            }
        }
        const double now = seconds();
        ++window_frames;
        if (now - window_start >= 1.0) {
            fps = window_frames / (now - window_start);
            window_frames = 0;
            window_start = now;
        }
        if (graphics) {
            graphic_frame(&fb, frames, speed);
        } else {
            if (frames % 30 == 0 || keys) {
                consoleClear();
                printf("=============================================\n");
                printf(" NX ON ORBIS  -  GD PROBE 0.2\n");
                printf(" CPU / MEMORY / INPUT / GRAPHICS\n");
                printf("=============================================\n\n");
                printf("CPU checksum      : %016llx\n", (unsigned long long)cpu_checksum);
                printf("Memory 1 MiB      : %s\n", mem1 ? "PASS" : "FAIL");
                printf("Memory 4 MiB      : %s\n", mem4 ? "PASS" : "FAIL");
                printf("Frame loops       : %llu\n", (unsigned long long)frames);
                printf("Loop rate (1 s)   : %.1f /s (NOT emulator FPS)\n", fps);
                printf("Controller events : %u\n", events);
                printf("Last button       : %s\n", last);
                printf("Movement speed    : %d\n\n", speed);
                printf("A: graphics   B: text   LEFT/RIGHT: speed\n");
                printf("PLUS: exit to PS4 (close with PS button)\n");
                if (graphics_failed) printf("Graphic framebuffer creation FAILED\n");
            }
            consoleUpdate(NULL);
        }
        ++frames;
        svcSleepThread(16666666LL);
    }
    if (graphics) framebufferClose(&fb);
    else consoleExit(NULL);
    return 0;
}
