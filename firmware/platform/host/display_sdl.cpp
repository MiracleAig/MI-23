//
// Created by Miracle Aigbogun on 3/10/26.
//

#include "platform/host/display_sdl.h"

#include "graphics/font.h"

#include <algorithm>
#include <cstdio>

namespace {

constexpr uint32_t SDL_PIXEL_FORMAT_RGB565 = SDL_PIXELFORMAT_RGB565;

} // namespace

DisplaySDL::DisplaySDL()
    : m_framebuffer(static_cast<std::size_t>(SIMULATOR_WINDOW_WIDTH) *
                    static_cast<std::size_t>(SIMULATOR_WINDOW_HEIGHT),
                    Display::BLACK.rgb565()) {}

DisplaySDL::~DisplaySDL() {
    if (m_texture) {
        SDL_DestroyTexture(m_texture);
    }
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
    }
    SDL_Quit();
}

void DisplaySDL::init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::printf("SDL_Iinit error: %s\n", SDL_GetError());
        return;
    }

    m_window = SDL_CreateWindow("Calculator Simulator",
                                SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED,
                                SIMULATOR_WINDOW_WIDTH * 2,
                                SIMULATOR_WINDOW_HEIGHT * 2,
                                SDL_WINDOW_SHOWN);
    if (!m_window) {
        std::printf("SDL_CreateWindow error: %s\n", SDL_GetError());
        return;
    }

    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_renderer) {
        std::printf("SDL_CreateRenderer error: %s\n", SDL_GetError());
        return;
    }

    m_texture = SDL_CreateTexture(m_renderer,
                                  SDL_PIXEL_FORMAT_RGB565,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  SIMULATOR_WINDOW_WIDTH,
                                  SIMULATOR_WINDOW_HEIGHT);
    if (!m_texture) {
        std::printf("SDL_CreateTexture error: %s\n", SDL_GetError());
        return;
    }

    SDL_RenderSetScale(m_renderer, 2.0f, 2.0f);
    markDirty({0, 0, SIMULATOR_WINDOW_WIDTH, SIMULATOR_WINDOW_HEIGHT});
    uploadAndPresent();
    std::printf("Display initialized: %dx%d\n",
                SIMULATOR_WINDOW_WIDTH,
                SIMULATOR_WINDOW_HEIGHT);
}

void DisplaySDL::markDirty(const DisplayRect& rect) {
    m_presentRegions.add(rect);
}

void DisplaySDL::clear(Color color) {
    DisplayRect rect{0, 0, SIMULATOR_WINDOW_WIDTH, SIMULATOR_WINDOW_HEIGHT};
    if (!clipRect(rect)) {
        return;
    }

    const uint16_t value = color.rgb565();
    for (int row = 0; row < rect.h; ++row) {
        uint16_t* line = &m_framebuffer[static_cast<std::size_t>((rect.y + row) *
                                                                 SIMULATOR_WINDOW_WIDTH +
                                                                 rect.x)];
        std::fill(line, line + rect.w, value);
    }
    markDirty(rect);
}

void DisplaySDL::drawPixel(int x, int y, Color color) {
    if (!clipPoint(x, y) ||
        x < 0 || x >= SIMULATOR_WINDOW_WIDTH ||
        y < 0 || y >= SIMULATOR_WINDOW_HEIGHT) {
        return;
    }

    m_framebuffer[static_cast<std::size_t>(y * SIMULATOR_WINDOW_WIDTH + x)] = color.rgb565();
    markDirty({x, y, 1, 1});
}

void DisplaySDL::fillRect(int x, int y, int w, int h, Color color) {
    DisplayRect rect{x, y, w, h};
    if (!clipRect(rect)) {
        return;
    }
    rect = DirtyRegionList::intersect(rect,
                                      {0, 0, SIMULATOR_WINDOW_WIDTH, SIMULATOR_WINDOW_HEIGHT});
    if (rect.isEmpty()) {
        return;
    }

    const uint16_t value = color.rgb565();
    for (int row = 0; row < rect.h; ++row) {
        uint16_t* line = &m_framebuffer[static_cast<std::size_t>((rect.y + row) *
                                                                 SIMULATOR_WINDOW_WIDTH +
                                                                 rect.x)];
        std::fill(line, line + rect.w, value);
    }
    markDirty(rect);
}

void DisplaySDL::drawChar(char c, int x, int y, Color color) {
    const uint8_t ascii = static_cast<uint8_t>(c);
    const uint8_t* glyph = &FONT_DATA[ascii * FONT_CHAR_WIDTH];
    const uint16_t value = color.rgb565();

    for (int col = 0; col < FONT_CHAR_WIDTH; ++col) {
        const uint8_t colByte = glyph[col];
        for (int row = 0; row < FONT_CHAR_HEIGHT; ++row) {
            if (((colByte >> row) & 1U) == 0) {
                continue;
            }

            int px = x + col;
            int py = y + row;
            if (!clipPoint(px, py) ||
                px < 0 || px >= SIMULATOR_WINDOW_WIDTH ||
                py < 0 || py >= SIMULATOR_WINDOW_HEIGHT) {
                continue;
            }

            m_framebuffer[static_cast<std::size_t>(py * SIMULATOR_WINDOW_WIDTH + px)] = value;
        }
    }
}

void DisplaySDL::drawText(const char* text, int x, int y, Color color) {
    if (!text) {
        return;
    }

    int cursorX = x;
    for (int i = 0; text[i] != '\0'; ++i) {
        drawChar(text[i], cursorX, y, color);
        cursorX += FONT_CHAR_ADVANCE;
    }

    markDirty({x, y, Display::textWidth(text), FONT_CHAR_HEIGHT});
}

void DisplaySDL::uploadAndPresent() {
    if (!m_renderer || !m_texture || m_presentRegions.empty()) {
        return;
    }

    for (int i = 0; i < m_presentRegions.count(); ++i) {
        const DisplayRect rect = m_presentRegions.rect(i);
        SDL_Rect sdlRect{rect.x, rect.y, rect.w, rect.h};
        const void* pixels = &m_framebuffer[static_cast<std::size_t>(rect.y *
                                                                     SIMULATOR_WINDOW_WIDTH +
                                                                     rect.x)];
        SDL_UpdateTexture(m_texture,
                          &sdlRect,
                          pixels,
                          SIMULATOR_WINDOW_WIDTH * static_cast<int>(sizeof(uint16_t)));
    }

    SDL_RenderClear(m_renderer);
    SDL_RenderCopy(m_renderer, m_texture, nullptr, nullptr);
    SDL_RenderPresent(m_renderer);
    m_presentRegions.clear();
}

void DisplaySDL::present() {
    if (!m_presentEnabled || !m_renderer) {
        return;
    }
    uploadAndPresent();
}

void DisplaySDL::setPresentEnabled(bool enabled) {
    m_presentEnabled = enabled;
}

void DisplaySDL::forcePresent() {
    if (!m_renderer) {
        return;
    }
    uploadAndPresent();
}
