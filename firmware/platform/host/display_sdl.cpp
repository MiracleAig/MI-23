//
// Created by Miracle Aigbogun on 3/10/26.
//

#include "platform/host/display_sdl.h"
#include "graphics/font.h"
#include <cstdio>

DisplaySDL::DisplaySDL() = default;

DisplaySDL::~DisplaySDL() {
    if (m_renderer) SDL_DestroyRenderer( m_renderer);
    if (m_window) SDL_DestroyWindow( m_window);
    SDL_Quit();
}

void DisplaySDL::init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL_Iinit error: %s\n", SDL_GetError());
        return;
    }

    // Scale window size by 2. Host gets extra simulator chrome below the
    // calculator display for app-specific onscreen controls.
    m_window = SDL_CreateWindow(
        "Calculator Simulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SIMULATOR_WINDOW_WIDTH * 2,
        SIMULATOR_WINDOW_HEIGHT * 2,
        SDL_WINDOW_SHOWN
    );

    if (!m_window) {
        printf("SDL_CreateWindow error: %s\n", SDL_GetError());
        return;
    }

    m_renderer = SDL_CreateRenderer( m_window, -1, SDL_RENDERER_ACCELERATED);

    if (!m_renderer) {
        printf("SDL_CreateRenderer error: %s\n", SDL_GetError());
        return;
    }

    SDL_RenderSetScale(m_renderer, 2.0f, 2.0f);

    printf("Display initialized: %dx%d\n", SIMULATOR_WINDOW_WIDTH, SIMULATOR_WINDOW_HEIGHT);
}

static void setDrawColor(SDL_Renderer* r, Color color) {
    const uint16_t rgb565 = color.rgb565();
    uint8_t red = ((rgb565 >> 11)    & 0x1F) << 3;
    uint8_t green = ((rgb565 >> 5)   & 0x3F) << 2;
    uint8_t blue = ((rgb565)         & 0x1F) << 3;

    SDL_SetRenderDrawColor(r, red, green, blue, 255);
}

void DisplaySDL::clear(Color color) {
    setDrawColor(m_renderer, color);
    SDL_RenderClear(m_renderer);
}

void DisplaySDL::drawPixel(int x, int y, Color color) {
    if (x < 0 || x >= SIMULATOR_WINDOW_WIDTH || y < 0 || y >= SIMULATOR_WINDOW_HEIGHT) {
        return;
    }

    setDrawColor(m_renderer, color);
    SDL_RenderDrawPoint(m_renderer, x, y);
}

void DisplaySDL::fillRect(int x, int y, int w, int h, Color color) {
    if (w <= 0 || h <= 0) {
        return;
    }

    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x >= SIMULATOR_WINDOW_WIDTH || y >= SIMULATOR_WINDOW_HEIGHT) {
        return;
    }
    if (x + w > SIMULATOR_WINDOW_WIDTH) {
        w = SIMULATOR_WINDOW_WIDTH - x;
    }
    if (y + h > SIMULATOR_WINDOW_HEIGHT) {
        h = SIMULATOR_WINDOW_HEIGHT - y;
    }
    if (w <= 0 || h <= 0) {
        return;
    }

    setDrawColor(m_renderer, color);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(m_renderer, &rect);
}

void DisplaySDL::drawChar(char c, int x, int y, Color color) {
    uint8_t ascii = static_cast<uint8_t>(c);
    const uint8_t* glyph = &FONT_DATA[ascii * FONT_CHAR_WIDTH];
    for (int col = 0; col < FONT_CHAR_WIDTH; col++) {
        uint8_t col_byte = glyph[col];
        for (int row = 0; row < FONT_CHAR_HEIGHT; row++) {
            if ((col_byte >> row) & 1) {
                drawPixel(x + col, y + row, color);
            }
        }
    }
}

void DisplaySDL::drawText(const char* text, int x, int y, Color color) {
    int cursor_x = x;
    for (int i = 0; text[i] != '\0'; i++) {
        drawChar(text[i], cursor_x, y, color);  // bgColor argument removed
        cursor_x += FONT_CHAR_ADVANCE;
    }
}

void DisplaySDL::present() {
    if (m_presentEnabled) {
        SDL_RenderPresent(m_renderer);
    }
}

void DisplaySDL::setPresentEnabled(bool enabled) {
    m_presentEnabled = enabled;
}

void DisplaySDL::forcePresent() {
    SDL_RenderPresent(m_renderer);
}

// Checks if anything happened (key press, window close, etc)
// void DisplaySDL::pollEvents() {
//     SDL_Event event;
//     while (SDL_PollEvent(&event)) {
//         if (event.type == SDL_QUIT) {
//             m_shouldQuit = true;
//         }
//         if (event.type == SDL_KEYDOWN) {
//             if (event.key.keysym.sym == SDLK_ESCAPE) {
//                 m_shouldQuit = true;
//             }
//         }
//     }
//}
