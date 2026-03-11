//
// Created by miracleaigbogun on 3/10/26.
//

#include "display_sdl.h"
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

    // Scale window size by 2 (640px X 480px)
    m_window = SDL_CreateWindow(
        "Calculator Simulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        DISPLAY_WIDTH * 2,
        DISPLAY_HEIGHT * 2,
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

    printf("Display initialized: %dx%d\n", DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

void DisplaySDL::clear(uint16_t color) {
    uint8_t r = ((color >> 11)  & 0x1F) << 3;
    uint8_t g = ((color >> 5)   & 0x3F) << 2;
    uint8_t b = ((color)        & 0x1F) << 3;

    SDL_SetRenderDrawColor(m_renderer, r, g, b, 255);
    SDL_RenderClear(m_renderer);
}

void DisplaySDL::drawPixel(int x, int y, uint16_t color) {
    uint8_t r = ((color >> 11)  & 0x1F) << 3;
    uint8_t g = ((color >> 5)   & 0x3F) << 2;
    uint8_t b = ((color)        & 0x1F) << 3;

    SDL_SetRenderDrawColor(m_renderer, r, g, b, 255);
    SDL_RenderDrawPoint(m_renderer, x, y);
}

void DisplaySDL::drawRect(int x, int y, int w, int h, uint16_t color) {
    uint8_t r = ((color >> 11)  & 0x1F) << 3;
    uint8_t g = ((color >> 5)   & 0x3F) << 2;
    uint8_t b = ((color)        & 0x1F) << 3;

    SDL_SetRenderDrawColor(m_renderer, r, g, b, 255);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(m_renderer, &rect);
}

void DisplaySDL::drawText(const char* text, int x, int y, uint16_t color) {
    int len = 0;
    while (text[len]) len++;
    drawRect(x, y, len * 8, 16, color);
}

void DisplaySDL::present() {
    SDL_RenderPresent(m_renderer);
}

// Checks if anything happened (key press, window close, etc)
void DisplaySDL::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            m_shouldQuit = true;
        }
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                m_shouldQuit = true;
            }
        }
    }

}
