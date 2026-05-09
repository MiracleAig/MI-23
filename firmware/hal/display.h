//
// Created by Miracle Aigbogun on 3/10/26.
//

#pragma once
#include <cstdint>

#include "graphics/font.h"

static const int DISPLAY_WIDTH = 320;
static const int DISPLAY_HEIGHT = 240;

struct Color {
    uint16_t value;

    constexpr Color() : value(0) {}
    constexpr Color(uint16_t rgb565) : value(rgb565) {}
    constexpr Color(uint8_t r, uint8_t g, uint8_t b)
        : value(static_cast<uint16_t>(((r >> 3) << 11) |
                                      ((g >> 2) << 5) |
                                      (b >> 3))) {}

    constexpr uint16_t rgb565() const { return value; }
    constexpr operator uint16_t() const { return value; }
};

class Display {
public:
    virtual ~Display() {};
    virtual void init() = 0;
    virtual void clear(Color color) = 0;
    virtual void drawPixel(int x, int y, Color color) = 0;
    virtual void fillRect(int x, int y, int w, int h, Color color) = 0;
    virtual void drawText(const char* text, int x, int y, Color color) = 0;
    virtual void present() = 0; // Push updates to the screen

    virtual void drawHorizontalLine(int x, int y, int w, Color color) {
        fillRect(x, y, w, 1, color);
    }

    virtual void drawVerticalLine(int x, int y, int h, Color color) {
        fillRect(x, y, 1, h, color);
    }

    virtual void drawRect(int x, int y, int w, int h, Color color) {
        if (w <= 0 || h <= 0) {
            return;
        }

        drawHorizontalLine(x, y, w, color);
        if (h > 1) {
            drawHorizontalLine(x, y + h - 1, w, color);
        }
        if (h > 2) {
            drawVerticalLine(x, y + 1, h - 2, color);
            if (w > 1) {
                drawVerticalLine(x + w - 1, y + 1, h - 2, color);
            }
        }
    }

    virtual void drawLine(int x0, int y0, int x1, int y1, Color color) {
        const int dx = absInt(x1 - x0);
        const int sx = x0 < x1 ? 1 : -1;
        const int dy = -absInt(y1 - y0);
        const int sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;

        while (true) {
            drawPixel(x0, y0, color);
            if (x0 == x1 && y0 == y1) {
                break;
            }

            const int error2 = 2 * error;
            if (error2 >= dy) {
                error += dy;
                x0 += sx;
            }
            if (error2 <= dx) {
                error += dx;
                y0 += sy;
            }
        }
    }

    static constexpr Color rgb(uint8_t r, uint8_t g, uint8_t b) {
        return Color(r, g, b);
    }

    static int textWidth(const char* text) {
        int len = 0;
        while (text[len] != '\0') {
            len++;
        }
        return len * FONT_CHAR_ADVANCE;
    }


    static constexpr Color WHITE = Color(0xFFFF);
    static constexpr Color BLACK = Color(0x0000);
    static constexpr Color RED = Color(0xF800);
    static constexpr Color GREEN = Color(0x07E0);
    static constexpr Color BLUE = Color(0x001F);

private:
    static constexpr int absInt(int value) {
        return value < 0 ? -value : value;
    }
};
