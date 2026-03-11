//
// Created by miracleaigbogun on 3/10/26.
//

#pragma once
#include <cstdint>

static const int DISPLAY_WIDTH = 320;
static const int DISPLAY_HEIGHT = 240;

class Display {
public:
    virtual ~Display() {};
    virtual void init() = 0;
    virtual void clear(uint16_t color) = 0;
    virtual void drawPixel(int x, int y, uint16_t color) = 0;
    virtual void drawRect(int x, int y, int w, int h, uint16_t color) = 0;
    virtual void drawText(const char* text, int x, int y, uint16_t color) = 0;
    virtual void present() = 0; // Push updates to the screen

    static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
        return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    static const  uint16_t WHITE = 0xFFFF;
    static const  uint16_t BLACK = 0x0000;
    static const  uint16_t RED = 0xF800;
    static const  uint16_t GREEN = 0x07E0;
    static const  uint16_t BLUE = 0x001F;

};