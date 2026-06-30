//
// Created by Miracle Aigbogun on 3/10/26.
//
#pragma once
#include "hal/display.h"
#include "platform/host/simulator_layout.h"
#include <SDL2/SDL.h>
#include <vector>

class DisplaySDL : public Display {
public:
    DisplaySDL();
    ~DisplaySDL();

    void init() override;
    void clear(Color color) override;
    void drawPixel(int x, int y, Color color) override;
    void fillRect(int x, int y, int w, int h, Color color) override;
    void drawText(const char* text, int x, int y, Color color) override;
    void present() override;
    void setPresentEnabled(bool enabled);
    void forcePresent();

    bool shouldQuit() const { return m_shouldQuit; }
    void setQuit() {m_shouldQuit = true;}
    bool isReady() const { return m_renderer != nullptr; }
    //void pollEvents();
private:
    void drawChar(char c, int x, int y, Color color);
    void markDirty(const DisplayRect& rect);
    void uploadAndPresent();
    int canvasWidth() const override { return SIMULATOR_WINDOW_WIDTH; }
    int canvasHeight() const override { return SIMULATOR_WINDOW_HEIGHT; }

    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture* m_texture = nullptr;
    std::vector<uint16_t> m_framebuffer;
    DirtyRegionList m_presentRegions;
    bool m_shouldQuit = false;
    bool m_presentEnabled = true;
};
