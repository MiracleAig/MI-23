//
// Created by Miracle Aigbogun on 3/10/26.
//
#pragma once
#include "hal/display.h"
#include <SDL2/SDL.h>

class DisplaySDL : public Display {
public:
    DisplaySDL();
    ~DisplaySDL();

    void init() override;
    void clear(uint16_t color) override;
    void drawPixel(int x, int y, uint16_t color) override;
    void drawRect(int x, int y, int w, int h, uint16_t color) override;
    void drawText(const char* text, int x, int y, uint16_t color) override;
    void present() override;

    bool shouldQuit() const { return m_shouldQuit; }
    void setQuit() {m_shouldQuit = true;}
    //void pollEvents();
private:
    void drawChar(char c, int x, int y, uint16_t color, uint16_t bgColor);

    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    bool m_shouldQuit = false;

};