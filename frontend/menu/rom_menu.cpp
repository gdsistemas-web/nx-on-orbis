// SPDX-License-Identifier: GPL-3.0-or-later
// eden-ps4: the game list shown before the emulator starts. See rom_menu.h.
//
// It draws with the CPU into two linear framebuffers in direct memory and flips them with
// sceVideoOut, then closes the video out so that RADV's WSI can open the display for Eden. The
// framebuffers' 16 MiB are never given back: the display may still be scanning the last one out
// after the close, and freeing memory under it is a risk this console answers with a reboot.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <sys/mman.h>
#include <sys/types.h>

#include "menu/menu_font.h"
#include "menu/rom_menu.h"
#include "ps4_platform.h"

extern "C" {
int32_t sceVideoOutOpen(int32_t user, int32_t bus, int32_t index, const void* param);
int32_t sceVideoOutClose(int32_t handle);
void sceVideoOutSetBufferAttribute(void* attribute, uint32_t pixel_format, uint32_t tiling,
                                   uint32_t aspect, uint32_t width, uint32_t height,
                                   uint32_t pitch);
int32_t sceVideoOutRegisterBuffers(int32_t handle, int32_t start, void* const* addresses,
                                   int32_t count, const void* attribute);
int32_t sceVideoOutSubmitFlip(int32_t handle, int32_t index, uint32_t mode, int64_t arg);
int32_t sceVideoOutSetFlipRate(int32_t handle, int32_t rate);
int32_t sceVideoOutIsFlipPending(int32_t handle);
int32_t sceKernelAllocateDirectMemory(off_t start, off_t end, size_t len, size_t align, int type,
                                      off_t* phys);
int32_t sceKernelMapDirectMemory(void** addr, size_t len, int prot, int flags, off_t phys,
                                 size_t align);
int32_t sceKernelReleaseDirectMemory(off_t start, size_t len);
size_t sceKernelGetDirectMemorySize(void);
}

namespace {

constexpr int Width = 1920;
constexpr int Height = 1080;
constexpr size_t BufferBytes = size_t(Width) * Height * 4;
constexpr size_t Align = 2u << 20;
constexpr size_t TotalBytes = (BufferBytes * 2 + Align - 1) & ~(Align - 1);
constexpr uint32_t PixelFormatA8R8G8B8Srgb = 0x80000000u; // pixels are 0xAARRGGBB
constexpr uint32_t TilingLinear = 1;
constexpr int MemoryWcGarlic = 3;
constexpr int ProtCpuRwGpuRw = 0x33;
constexpr uint32_t FlipVsync = 1;

constexpr uint32_t Background = 0xFF141821;
constexpr uint32_t Text = 0xFFE8E8E8;
constexpr uint32_t Dim = 0xFF8A93A6;
constexpr uint32_t Highlight = 0xFF2F5FD0;

struct Canvas {
    uint32_t* pixels;

    void Fill(int x, int y, int w, int h, uint32_t color) {
        for (int j = std::max(y, 0); j < std::min(y + h, Height); ++j) {
            std::fill(pixels + size_t(j) * Width + std::max(x, 0),
                      pixels + size_t(j) * Width + std::min(x + w, Width), color);
        }
    }

    /// Draws ASCII text (anything else as '?'), clipped at max_chars, blending the glyph
    /// coverage over the background color behind it.
    void Print(int x, int y, const std::string& text, uint32_t color, uint32_t behind,
               size_t max_chars = 200) {
        for (size_t i = 0; i < text.size() && i < max_chars; ++i) {
            unsigned char c = static_cast<unsigned char>(text[i]);
            if (c < 32 || c > 126) {
                c = '?';
            }
            const std::uint8_t* glyph = MenuFont::Glyphs[c - 32];
            const int gx = x + int(i) * MenuFont::Width;
            for (int row = 0; row < MenuFont::Height; ++row) {
                const int py = y + row;
                if (py < 0 || py >= Height) {
                    continue;
                }
                for (int col = 0; col < MenuFont::Width; ++col) {
                    const int px = gx + col;
                    const unsigned a = glyph[row * MenuFont::Width + col];
                    if (a == 0 || px < 0 || px >= Width) {
                        continue;
                    }
                    const auto mix = [a](uint32_t fg, uint32_t bg, int shift) {
                        const unsigned f = (fg >> shift) & 0xFF;
                        const unsigned b = (bg >> shift) & 0xFF;
                        return ((f * a + b * (255 - a)) / 255) << shift;
                    };
                    pixels[size_t(py) * Width + px] = 0xFF000000u | mix(color, behind, 16) |
                                                      mix(color, behind, 8) |
                                                      mix(color, behind, 0);
                }
            }
        }
    }
};

// Read the persisted fastmem mode (default OFF) without initializing Eden.
bool ReadFastmemSetting() {
    std::ifstream in{"/data/edenps4/settings.txt"};
    std::string line;
    bool on = false;
    while (std::getline(in, line)) {
        const auto comment = line.find('#');
        if (comment != std::string::npos) line.erase(comment);
        line.erase(std::remove_if(line.begin(), line.end(),
            [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r'; }), line.end());
        if (line == "fastmem=on") on = true;
        if (line == "fastmem=off") on = false;
    }
    return on;
}

// Save atomically, retaining unrelated settings (including custom driver options).
bool WriteFastmemSetting(bool on) {
    namespace fs = std::filesystem;
    const fs::path path{"/data/edenps4/settings.txt"};
    const fs::path temporary{"/data/edenps4/settings.txt.tmp"};
    std::vector<std::string> lines;
    {
        std::ifstream input{path};
        for (std::string line; std::getline(input, line);) {
            std::string check = line;
            const auto comment = check.find('#');
            if (comment != std::string::npos) check.erase(comment);
            check.erase(std::remove_if(check.begin(), check.end(),
                [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r'; }), check.end());
            if (check.rfind("fastmem=", 0) != 0) lines.push_back(line);
        }
    }
    std::ofstream out{temporary, std::ios::trunc};
    if (!out) return false;
    for (const auto& line : lines) out << line << '\n';
    out << "fastmem=" << (on ? "on" : "off") << '\n';
    out.flush();
    if (!out.good()) { out.close(); std::error_code ec; fs::remove(temporary, ec); return false; }
    out.close();
    std::error_code ec;
    fs::rename(temporary, path, ec);
    if (ec) { fs::remove(temporary, ec); return false; }
    Ps4::Log("menu: persisted fastmem=%s (effective next launch)", on ? "on" : "off");
    return true;
}

struct BootSummary {
    std::string build = "Sem registro";
    std::string status = "Sem medicao";
    std::string fastmem = "Sem medicao";
};
BootSummary ReadBootSummary() {
    BootSummary summary;
    std::ifstream in{"/data/edenps4/boot.log"};
    std::string line;
    std::size_t count = 0;
    // Only parse bounded text; never allow a large log to stall the menu.
    while (count++ < 20000 && std::getline(in, line)) {
        if (line.size() > 1024) continue;
        const auto take = [&](const char* marker, std::string& dest) {
            const auto pos = line.find(marker);
            if (pos != std::string::npos) dest = line.substr(pos, 110);
        };
        take("PS4 build:", summary.build);
        take("status: game ", summary.status);
        take("fastmem: view ", summary.fastmem);
        take("fastmem: off ", summary.fastmem);
    }
    return summary;
}

std::string DisplayName(const std::string& file) {
    std::string name = file;
    if (const auto dot = name.find_last_of('.'); dot != std::string::npos && dot > 0) {
        name.erase(dot);
    }
    return name;
}

// GD Edition palette: lightweight native graphics, no extra video allocations.
void Draw(Canvas& c, const std::vector<std::string>& names, int selected, bool fastmemOn, bool settingsOpen, bool diagnosticsOpen, bool saveError, const BootSummary& summary) {
    constexpr uint32_t Base = 0xFF0B1120;
    constexpr uint32_t Panel = 0xFF182238;
    constexpr uint32_t Selected = 0xFF283B58;
    constexpr uint32_t Accent = 0xFFFF6B00;
    constexpr uint32_t White = 0xFFF7F9FD;
    constexpr uint32_t Muted = 0xFFB1BDD2;
    constexpr int Left = 112;
    constexpr int ListTop = 302;
    constexpr int RowHeight = 66;
    constexpr int Rows = 9;
    constexpr int ListWidth = 1140;
    constexpr size_t MaxChars = (ListWidth - 90) / MenuFont::Width;

    c.Fill(0, 0, Width, Height, Base);
    // Brand rail and heading.
    c.Fill(0, 0, 12, Height, Accent);
    c.Fill(Left, 78, 70, 70, Accent);
    c.Print(Left + 20, 102, "GD", White, Accent);
    c.Print(Left + 100, 84, "NX ON ORBIS", White, Base);
    c.Print(Left + 100, 124, "GD EDITION  /  TEST 31", Muted, Base);
    c.Fill(Left, 183, Width - Left * 2, 2, 0xFF2B3750);

    c.Print(Left, 216, settingsOpen ? "CONFIGURACOES" : diagnosticsOpen ? "DIAGNOSTICOS" : "BIBLIOTECA", White, Base);
    c.Print(Left, 253, settingsOpen ? "Ajustes experimentais do emulador" : diagnosticsOpen ? "Dados registrados no ultimo boot" : "Selecione um aplicativo para iniciar", Muted, Base);

    // Single-column game list; reserve the right-hand pane for diagnostics.
    c.Fill(Left, ListTop - 12, ListWidth, 650, Panel);
    const int count = int(names.size());
    const int first = std::clamp(selected - Rows / 2, 0, std::max(count - Rows, 0));
    if (settingsOpen) {
        c.Print(Left + 48, ListTop + 45, "FASTMEM", White, Panel);
        c.Print(Left + 48, ListTop + 100, fastmemOn ? "ATIVADA (EXPERIMENTAL)" : "DESATIVADA (RECOMENDADA)", Accent, Panel);
        c.Print(Left + 48, ListTop + 165, "X  Alternar fastmem", Muted, Panel);
        c.Print(Left + 48, ListTop + 220, "O ajuste passa a valer no proximo inicio.", Muted, Panel);
        c.Print(Left + 48, ListTop + 290, "Teste anterior no PS4 Fat: ON ~23 FPS", Muted, Panel);
        c.Print(Left + 48, ListTop + 340, "OFF ~60 FPS no Homebrew Menu.", Muted, Panel);
        if (saveError) c.Print(Left + 48, ListTop + 420, "ERRO: nao foi possivel salvar settings.txt", Accent, Panel);
    } else if (diagnosticsOpen) {
        c.Print(Left + 48, ListTop + 42, "ULTIMA BUILD REGISTRADA", Accent, Panel);
        c.Print(Left + 48, ListTop + 90, summary.build, White, Panel, 63);
        c.Print(Left + 48, ListTop + 176, "ULTIMO STATUS", Accent, Panel);
        c.Print(Left + 48, ListTop + 224, summary.status, White, Panel, 63);
        c.Print(Left + 48, ListTop + 310, "FASTMEM", Accent, Panel);
        c.Print(Left + 48, ListTop + 358, summary.fastmem, White, Panel, 63);
        c.Print(Left + 48, ListTop + 470, "Dados historicos, nao atualizados ao vivo.", Muted, Panel);
        c.Print(Left + 48, ListTop + 526, "Circulo: voltar para biblioteca", Muted, Panel);
    } else {
        for (int row = 0; row < Rows && first + row < count; ++row) {
            const int index = first + row;
            const int y = ListTop + row * RowHeight;
            const bool on = index == selected;
            const uint32_t surface = on ? Selected : Panel;
            c.Fill(Left + 16, y + 2, ListWidth - 32, RowHeight - 7, surface);
            if (on) {
                c.Fill(Left + 16, y + 2, 7, RowHeight - 7, Accent);
            }
            c.Print(Left + 48, y + 23, DisplayName(names[index]), on ? White : Muted,
                    surface, MaxChars);
        }
    }
    if (!settingsOpen && !diagnosticsOpen && first > 0) {
        c.Print(Left + ListWidth - 52, ListTop - 4, "^", Accent, Panel);
    }
    if (!settingsOpen && !diagnosticsOpen && first + Rows < count) {
        c.Print(Left + ListWidth - 52, ListTop + Rows * RowHeight, "v", Accent, Panel);
    }

    // Side information is intentionally static: no fake performance readings.
    constexpr int SideX = 1290;
    c.Fill(SideX, ListTop - 12, 510, 650, Panel);
    c.Fill(SideX + 24, ListTop + 20, 6, 54, Accent);
    c.Print(SideX + 52, ListTop + 24, "GD TEST 31", White, Panel);
    c.Print(SideX + 26, ListTop + 110, "MODO SEGURO", Accent, Panel);
    c.Print(SideX + 26, ListTop + 160, fastmemOn ? "Fastmem: ON (experimental)" : "Fastmem: OFF (padrao)", White, Panel);
    c.Print(SideX + 26, ListTop + 212, "Quadrado: configuracoes", Muted, Panel);
    c.Fill(SideX + 24, ListTop + 266, 462, 2, 0xFF2B3750);
    c.Print(SideX + 26, ListTop + 310, "Sem jogos instalados?", White, Panel);
    c.Print(SideX + 26, ListTop + 356, "Teste o Homebrew Menu", Muted, Panel);
    c.Print(SideX + 26, ListTop + 402, "ou adicione ROMs via FTP.", Muted, Panel);
    c.Print(SideX + 26, ListTop + 534, "Build experimental", Accent, Panel);

    c.Fill(Left, Height - 100, Width - Left * 2, 2, 0xFF2B3750);
    c.Print(Left, Height - 72, settingsOpen ? "X  Alternar     O  Voltar" : diagnosticsOpen ? "O  Voltar" : "UP/DOWN  Navegar   X  Iniciar   []  Ajustes   TRI  Logs", Muted, Base);
    char position[32];
    std::snprintf(position, sizeof(position), "%d / %d", selected + 1, count);
    c.Print(Width - Left - int(std::strlen(position)) * MenuFont::Width,
            Height - 72, position, Accent, Base);
}

} // Anonymous namespace

int RunRomMenu(const std::vector<std::string>& names, int initial) {
    if (names.empty()) {
        return -1;
    }
    off_t phys = 0;
    if (int rc = sceKernelAllocateDirectMemory(0, off_t(sceKernelGetDirectMemorySize()),
                                               TotalBytes, Align, MemoryWcGarlic, &phys);
        rc != 0) {
        Ps4::Log("menu: direct memory for the framebuffers refused (0x%x)", unsigned(rc));
        return -1;
    }
    void* memory = nullptr;
    if (int rc = sceKernelMapDirectMemory(&memory, TotalBytes, ProtCpuRwGpuRw, 0, phys, Align);
        rc != 0) {
        Ps4::Log("menu: mapping the framebuffers failed (0x%x)", unsigned(rc));
        sceKernelReleaseDirectMemory(phys, TotalBytes);
        return -1;
    }
    const int handle = sceVideoOutOpen(255, 0, 0, nullptr);
    if (handle < 0) {
        Ps4::Log("menu: sceVideoOutOpen failed (0x%x)", unsigned(handle));
        munmap(memory, TotalBytes);
        sceKernelReleaseDirectMemory(phys, TotalBytes);
        return -1;
    }
    void* buffers[2] = {memory, static_cast<std::uint8_t*>(memory) + BufferBytes};
    alignas(16) std::uint8_t attribute[64] = {};
    sceVideoOutSetBufferAttribute(attribute, PixelFormatA8R8G8B8Srgb, TilingLinear, 0, Width,
                                  Height, Width);
    sceVideoOutSetFlipRate(handle, 0);
    if (int rc = sceVideoOutRegisterBuffers(handle, 0, buffers, 2, attribute); rc < 0) {
        Ps4::Log("menu: sceVideoOutRegisterBuffers failed (0x%x)", unsigned(rc));
        sceVideoOutClose(handle);
        munmap(memory, TotalBytes);
        sceKernelReleaseDirectMemory(phys, TotalBytes);
        return -1;
    }
    Ps4::Log("menu: %zu games listed", names.size());

    int selected = std::clamp(initial, 0, int(names.size()) - 1);
    bool fastmemOn = ReadFastmemSetting();
    bool settingsOpen = false;
    bool diagnosticsOpen = false;
    BootSummary summary{};
    bool saveError = false;
    int back = 0;
    const auto present = [&] {
        while (sceVideoOutIsFlipPending(handle) > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Canvas canvas{static_cast<uint32_t*>(buffers[back])};
        Draw(canvas, names, selected, fastmemOn, settingsOpen, diagnosticsOpen, saveError, summary);
        sceVideoOutSubmitFlip(handle, back, FlipVsync, 0);
        back ^= 1;
    };
    present();

    constexpr std::uint32_t Intercepted = 0x80000000u;
    std::uint32_t previous = Ps4::ReadPad().buttons;
    std::uint64_t repeat_at = 0;
    for (;;) {
        const std::uint32_t now = Ps4::ReadPad().buttons;
        if (now & Intercepted) {
            previous = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }
        const std::uint32_t pressed = now & ~previous;
        previous = now;
        if ((pressed & Ps4::Button::Triangle) && !settingsOpen) {
            diagnosticsOpen = !diagnosticsOpen;
            if (diagnosticsOpen) summary = ReadBootSummary();
            present();
            continue;
        }
        if ((pressed & Ps4::Button::Circle) && diagnosticsOpen) {
            diagnosticsOpen = false;
            present();
            continue;
        }
        if ((pressed & Ps4::Button::Square) && !diagnosticsOpen) {
            settingsOpen = !settingsOpen;
            saveError = false;
            present();
            continue;
        }
        if (settingsOpen && (pressed & Ps4::Button::Circle)) {
            settingsOpen = false;
            present();
            continue;
        }
        if (pressed & Ps4::Button::Cross) {
            if (diagnosticsOpen) continue;
            if (!settingsOpen) break;
            const bool desired = !fastmemOn;
            saveError = !WriteFastmemSetting(desired);
            if (!saveError) fastmemOn = desired;
            present();
            continue;
        }
        if (settingsOpen || diagnosticsOpen) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }
        int step = 0;
        const std::uint32_t vertical = now & (Ps4::Button::Up | Ps4::Button::Down);
        if (pressed & (Ps4::Button::Up | Ps4::Button::Down)) {
            step = (pressed & Ps4::Button::Up) ? -1 : 1;
            repeat_at = Ps4::NowUs() + 400000; // held: repeat after 0.4 s
        } else if (vertical != 0 && Ps4::NowUs() >= repeat_at) {
            step = (vertical & Ps4::Button::Up) ? -1 : 1;
            repeat_at = Ps4::NowUs() + 110000;
        }
        if (step != 0) {
            const int count = int(names.size());
            selected = (selected + step + count) % count;
            present();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Leave the display as Eden's WSI expects to find it: closed.
    {
        Canvas canvas{static_cast<uint32_t*>(buffers[back])};
        canvas.Fill(0, 0, Width, Height, 0xFF000000);
        while (sceVideoOutIsFlipPending(handle) > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        sceVideoOutSubmitFlip(handle, back, FlipVsync, 0);
        while (sceVideoOutIsFlipPending(handle) > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    const int closed = sceVideoOutClose(handle);
    Ps4::Log("menu: game %d of %zu chosen, display closed (0x%x); framebuffers kept",
             selected + 1, names.size(), unsigned(closed));
    return selected;
}
