// SPDX-License-Identifier: MIT
// GD Probe: tiny independent Switch NRO for exercising nx-on-orbis HLE.
// Unlike hbmenu, this program does not call launchInit() or require nx-hbloader.
#include <switch.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// Keep the arithmetic visible to the compiler to exercise AArch64 instructions.
static volatile uint64_t g_cpu_checksum;

static void run_cpu_probe(void) {
    uint64_t n = 0x123456789abcdef0ULL;
    for (unsigned i = 0; i < 100000; ++i) {
        n ^= n >> 13;
        n *= 0xff51afd7ed558ccdULL;
        n ^= n >> 27;
        n += i;
    }
    g_cpu_checksum = n;
}

static bool run_memory_probe(void) {
    const size_t bytes = 1024 * 1024;
    uint8_t* memory = (uint8_t*)malloc(bytes);
    if (!memory) return false;
    for (size_t i = 0; i < bytes; ++i) {
        memory[i] = (uint8_t)((i * 37u + 19u) & 255u);
    }
    bool ok = true;
    for (size_t i = 0; i < bytes; ++i) {
        if (memory[i] != (uint8_t)((i * 37u + 19u) & 255u)) {
            ok = false;
            break;
        }
    }
    free(memory);
    return ok;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    consoleInit(NULL);

    run_cpu_probe();
    const bool memory_ok = run_memory_probe();
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    uint64_t frames = 0;
    unsigned button_events = 0;
    const char* last_button = "none";
    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 pressed = padGetButtonsDown(&pad);
        if (pressed & HidNpadButton_Plus) break;
        if (pressed & HidNpadButton_A) { last_button = "A"; ++button_events; }
        if (pressed & HidNpadButton_B) { last_button = "B"; ++button_events; }
        if (pressed & HidNpadButton_Up) { last_button = "UP"; ++button_events; }
        if (pressed & HidNpadButton_Down) { last_button = "DOWN"; ++button_events; }

        // Avoid repainting text at full rate; consoleUpdate still presents frames.
        if (frames % 30 == 0) {
            consoleClear();
            printf("========================================\n");
            printf(" NX ON ORBIS  -  GD PROBE 0.1\n");
            printf(" Independent Nintendo Switch homebrew\n");
            printf("========================================\n\n");
            printf("CPU arithmetic : %016llx\n", (unsigned long long)g_cpu_checksum);
            printf("Memory 1 MiB   : %s\n", memory_ok ? "PASS" : "FAIL");
            printf("Present cycles : %llu\n", (unsigned long long)frames);
            printf("Pad events     : %u\n", button_events);
            printf("Last input     : %s\n\n", last_button);
            printf("Test A/B/UP/DOWN.  PLUS to exit.\n");
            printf("No hbloader APIs, games or keys needed.\n");
            printf("Experimental: libnx may require HLE services.\n");
        }
        consoleUpdate(NULL);
        ++frames;
        svcSleepThread(16666666LL);
    }

    consoleExit(NULL);
    return 0;
}
