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
    std::string build = "Nenhuma sessao anterior";
    std::string status = "Sem FPS anterior";
    std::string fastmem = "Sem dado anterior";
};
BootSummary ReadBootSummary() {
    BootSummary summary;
    // OpenBootLog() already rotates boot.log to boot.old.log at launch.
    // The current boot has no game FPS until AFTER this native menu exits.
    std::ifstream in{"/data/edenps4/boot.old.log"};
    std::string line;
    std::size_t count = 0;
    // Bounded historical read; no live polling and no changes to log rotation.
    while (count++ < 20000 && std::getline(in, line)) {
        if (line.size() > 1024) continue;
        if (const auto pos = line.find("PS4 build:"); pos != std::string::npos) {
            const auto first = pos + std::strlen("PS4 build:");
            const auto last = line.find(';', first);
            summary.build = line.substr(first, std::min(last == std::string::npos ? line.size() - first : last - first, size_t(64)));
        }
        if (const auto pos = line.find("status: game "); pos != std::string::npos) {
            const auto last = line.find(", guest+tables", pos);
            summary.status = line.substr(pos + 8, std::min(last == std::string::npos ? line.size() - pos - 8 : last - pos - 8, size_t(90)));
        }
        for (const char* marker : {"fastmem: view ", "fastmem: off "}) {
            if (const auto pos = line.find(marker); pos != std::string::npos) {
                summary.fastmem = line.substr(pos, 90);
            }
        }
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

// GD Test 35: dashboard drawn entirely on the existing two video-out buffers.
// Solid surfaces keep text alpha blending deterministic; no GPU or network dependencies.
enum class MenuView { Home, Library, Settings, Diagnostics };
constexpr uint32_t Ink = 0xFF091427;
constexpr uint32_t Surface = 0xFF142943;
constexpr uint32_t Surface2 = 0xFF1D3652;
constexpr uint32_t Orange = 0xFFFF6B00;
constexpr uint32_t White = 0xFFF6F8FF;
constexpr uint32_t Soft = 0xFFBBCBDE;
constexpr uint32_t Green = 0xFF6FD6AA;
constexpr int L = 96;

std::string Fitted(const std::string& str, size_t max) {
    return str.size() <= max ? str : str.substr(0, max > 3 ? max - 3 : 0) + "...";
}
std::string GameLabel(const std::string& filename) {
    if (filename.find("gd-probe") != std::string::npos || filename.find("GD Probe") != std::string::npos)
        return "GD PROBE  /  DIAGNOSTICO";
    if (filename.find("hbmenu") != std::string::npos || filename.find("Homebrew Menu") != std::string::npos)
        return "HOMEBREW MENU  /  EXPERIMENTAL";
    return DisplayName(filename);
}
void Frame(Canvas& c, MenuView view, bool fastmemOn) {
    c.Fill(0, 0, Width, Height, Ink);
    c.Fill(0, 0, Width, 12, Orange);
    c.Fill(L, 58, 76, 76, Orange);
    c.Print(L + 23, 86, "GD", White, Orange);
    c.Print(L + 108, 54, "NX ON ORBIS", White, Ink);
    c.Print(L + 108, 94, "GD EDITION  /  TEST 36", Soft, Ink);
    c.Fill(Width - 470, 71, 374, 52, Surface);
    c.Fill(Width - 470, 71, 7, 52, fastmemOn ? Orange : Green);
    c.Print(Width - 442, 88, fastmemOn ? "FASTMEM EXPERIMENTAL" : "FASTMEM MODO SEGURO", White, Surface, 23);
    c.Fill(L, 160, Width - L * 2, 2, 0xFF304660);
    const char* tabs[] = {"HOME", "BIBLIOTECA", "CONFIGURACOES", "DIAGNOSTICOS"};
    for (int i = 0; i < 4; ++i) {
        const bool active = int(view) == i;
        const int x = L + i * 302;
        c.Fill(x, 189, 278, 55, active ? Surface2 : Ink);
        if (active) c.Fill(x, 239, 278, 5, Orange);
        c.Print(x + 18, 207, tabs[i], active ? White : Soft, active ? Surface2 : Ink);
    }
}
void Draw(Canvas& c, const std::vector<std::string>& names, int selected, bool fastmemOn,
          MenuView view, bool saveError, const BootSummary& summary) {
    Frame(c, view, fastmemOn);
    const int count = int(names.size());
    if (view == MenuView::Home) {
        c.Print(L, 290, "BEM-VINDO A SUA BIBLIOTECA", White, Ink);
        c.Print(L, 333, "Emulacao experimental de Nintendo Switch no PlayStation 4", Soft, Ink, 66);
        // Large hero panel: selected app can be launched directly with Cross.
        c.Fill(L, 385, 1120, 410, Surface);
        c.Fill(L, 385, 13, 410, Orange);
        c.Fill(L + 40, 422, 214, 208, Surface2);
        c.Fill(L + 62, 444, 170, 165, Orange);
        c.Print(L + 91, 507, "NX", White, Orange);
        c.Print(L + 292, 417, "DESTAQUE", Orange, Surface);
        c.Print(L + 292, 469, Fitted(GameLabel(names[selected]), 36), White, Surface, 38);
        c.Print(L + 292, 527, "Pronto para iniciar", Green, Surface);
        c.Print(L + 292, 590, "X  INICIAR APLICATIVO", White, Surface);
        c.Print(L + 292, 654, "Direcional: escolher aplicativo", Soft, Surface);
        c.Print(L + 292, 696, "Direita: biblioteca completa", Soft, Surface);
        c.Fill(L + 32, 748, 1055, 2, 0xFF36516D);

        c.Fill(1250, 385, 574, 410, Surface);
        c.Print(1284, 417, "ULTIMA SESSAO", Orange, Surface);
        c.Print(1284, 475, "DESEMPENHO REGISTRADO", Soft, Surface);
        c.Print(1284, 518, Fitted(summary.status, 28), White, Surface, 30);
        c.Print(1284, 594, "MEMORIA RAPIDA", Soft, Surface);
        c.Print(1284, 637, fastmemOn ? "ON - TESTE" : "OFF - SEGURO", fastmemOn ? Orange : Green, Surface);
        c.Print(1284, 705, "Triangulo: detalhes", Soft, Surface);

        c.Fill(L, 825, 548, 100, Surface2);
        c.Print(L + 24, 843, "BIBLIOTECA", White, Surface2);
        char apps[64]; std::snprintf(apps, sizeof(apps), "%d aplicativos disponiveis", count);
        c.Print(L + 24, 882, apps, Soft, Surface2);
        c.Fill(L + 574, 825, 546, 100, Surface2);
        c.Print(L + 600, 843, "CONFIGURACOES", White, Surface2);
        c.Print(L + 600, 882, "Quadrado: ajustes de CPU", Soft, Surface2);
        c.Fill(1250, 825, 574, 100, Surface2);
        c.Print(1278, 843, "DIAGNOSTICOS", White, Surface2);
        c.Print(1278, 882, "Historico de execucao", Soft, Surface2);
    } else if (view == MenuView::Library) {
        c.Print(L, 288, "SEUS APLICATIVOS", White, Ink);
        c.Print(L, 330, "Escolha um item para iniciar no emulador", Soft, Ink);
        c.Fill(L, 380, 1210, 553, Surface);
        constexpr int rows = 7, itemH = 71;
        const int first = std::clamp(selected - rows / 2, 0, std::max(count - rows, 0));
        for (int i = 0; i < rows && i + first < count; ++i) {
            const int y = 400 + i * itemH;
            const bool chosen = selected == i + first;
            const uint32_t bg = chosen ? Surface2 : Surface;
            c.Fill(L + 16, y, 1170, 63, bg);
            if (chosen) c.Fill(L + 16, y, 8, 63, Orange);
            c.Print(L + 45, y + 22, Fitted(GameLabel(names[first + i]), 49),
                    chosen ? White : Soft, bg, 50);
        }
        c.Fill(1340, 380, 484, 553, Surface);
        c.Print(1370, 414, "APLICATIVO", Orange, Surface);
        c.Print(1370, 468, Fitted(GameLabel(names[selected]), 23), White, Surface, 23);
        c.Print(1370, 540, "X  INICIAR", Green, Surface);
        c.Print(1370, 625, "Selecione com cima/baixo", Soft, Surface);
        c.Print(1370, 700, "Voltar: Circulo", Soft, Surface);
    } else if (view == MenuView::Settings) {
        c.Print(L, 287, "CONFIGURACOES DO EMULADOR", White, Ink);
        c.Print(L, 335, "As alteracoes passam a valer no proximo boot", Soft, Ink);
        c.Fill(L, 393, 1160, 520, Surface);
        c.Fill(L + 30, 427, 8, 92, Orange);
        c.Print(L + 66, 429, "FASTMEM", White, Surface);
        c.Print(L + 66, 483, fastmemOn ? "ATIVADA / EXPERIMENTAL" : "DESATIVADA / RECOMENDADA",
                fastmemOn ? Orange : Green, Surface);
        c.Print(L + 64, 579, "X  Alternar   |   Circulo  Voltar", Soft, Surface);
        c.Print(L + 64, 653, "GD 30: ON ~23 FPS, OFF ~60 FPS (hbmenu)", Soft, Surface);
        if (saveError) c.Print(L + 64, 724, "ERRO AO SALVAR settings.txt", Orange, Surface);
        c.Fill(1290, 393, 534, 520, Surface);
        c.Print(1320, 431, "PERFIL DE EXECUCAO", Orange, Surface);
        c.Print(1320, 507, "Fastmem OFF por padrao", White, Surface);
        c.Print(1320, 576, "PS4 Fat - modo seguro", Soft, Surface);
        c.Print(1320, 660, "Config: settings.txt", Soft, Surface);
    } else {
        c.Print(L, 289, "CENTRAL DE DIAGNOSTICOS", White, Ink);
        c.Print(L, 332, "Resultados da sessao anterior - nao sao dados ao vivo", Soft, Ink, 64);
        c.Fill(L, 390, 1728, 532, Surface);
        c.Fill(L + 32, 430, 8, 81, Orange);
        c.Print(L + 69, 427, "BUILD ANTERIOR", Orange, Surface);
        c.Print(L + 69, 487, Fitted(summary.build, 63), White, Surface, 65);
        c.Fill(L + 44, 565, 1610, 2, 0xFF304660);
        c.Print(L + 69, 600, "DESEMPENHO REPORTADO", Orange, Surface);
        c.Print(L + 69, 653, Fitted(summary.status, 65), White, Surface, 65);
        c.Print(L + 69, 735, "FASTMEM / SESSAO ANTERIOR", Orange, Surface);
        c.Print(L + 69, 788, Fitted(summary.fastmem, 65), White, Surface, 65);
        c.Print(L + 69, 856, "Fonte: boot.old.log", Soft, Surface);
    }
    c.Fill(L, 971, Width - L * 2, 2, 0xFF304660);
    const char* footer = view == MenuView::Settings ? "X ALTERNAR     O VOLTAR     TRI DIAGNOSTICOS" :
                         view == MenuView::Diagnostics ? "O VOLTAR     QUADRADO CONFIGURACOES" :
                         "X INICIAR     SETAS NAVEGAR     QUADRADO AJUSTES     TRI LOGS";
    c.Print(L + 4, 1003, footer, Soft, Ink, 74);
    c.Print(1595, 1003, "GD 36  /  BETA", Orange, Ink, 17);
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
    MenuView view = MenuView::Home;
    BootSummary summary = ReadBootSummary();
    bool saveError = false;
    int back = 0;
    const auto present = [&] {
        while (sceVideoOutIsFlipPending(handle) > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Canvas canvas{static_cast<uint32_t*>(buffers[back])};
        Draw(canvas, names, selected, fastmemOn, view, saveError, summary);
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
        if (pressed & Ps4::Button::Triangle) {
            view = view == MenuView::Diagnostics ? MenuView::Home : MenuView::Diagnostics;
            if (view == MenuView::Diagnostics) summary = ReadBootSummary();
            present();
            continue;
        }
        if (pressed & Ps4::Button::Square) {
            view = view == MenuView::Settings ? MenuView::Home : MenuView::Settings;
            saveError = false;
            present();
            continue;
        }
        if (pressed & Ps4::Button::Circle) {
            if (view != MenuView::Home) {
                view = MenuView::Home;
                present();
            }
            continue;
        }
        if (pressed & Ps4::Button::Cross) {
            if (view == MenuView::Settings) {
                const bool desired = !fastmemOn;
                saveError = !WriteFastmemSetting(desired);
                if (!saveError) fastmemOn = desired;
                present();
                continue;
            }
            if (view == MenuView::Diagnostics) continue;
            break;
        }
        if (view == MenuView::Settings || view == MenuView::Diagnostics) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }
        if (pressed & Ps4::Button::Right) {
            view = MenuView::Library;
            present();
            continue;
        }
        if (pressed & Ps4::Button::Left) {
            view = MenuView::Home;
            present();
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
