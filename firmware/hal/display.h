//
// Created by Miracle Aigbogun on 3/10/26.
//

#pragma once
#include <array>
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

struct DisplayRect {
    int x;
    int y;
    int w;
    int h;

    constexpr bool isEmpty() const { return w <= 0 || h <= 0; }
};

class DirtyRegionList {
public:
    static constexpr int MAX_RECTS = 8;

    void clear() {
        m_count = 0;
    }

    bool empty() const {
        return m_count == 0;
    }

    int count() const {
        return m_count;
    }

    const DisplayRect& rect(int index) const {
        return m_rects[index];
    }

    void add(DisplayRect rect) {
        normalize(rect);
        if (rect.isEmpty()) {
            return;
        }

        for (int i = 0; i < m_count; ++i) {
            if (intersectsOrTouches(m_rects[i], rect)) {
                m_rects[i] = merge(m_rects[i], rect);
                coalesceFrom(i);
                return;
            }
        }

        if (m_count < MAX_RECTS) {
            m_rects[m_count++] = rect;
            return;
        }

        m_rects[0] = merge(m_rects[0], rect);
        coalesceFrom(0);
    }

    static DisplayRect intersect(DisplayRect a, DisplayRect b) {
        normalize(a);
        normalize(b);
        const int left = maxInt(a.x, b.x);
        const int top = maxInt(a.y, b.y);
        const int right = minInt(a.x + a.w, b.x + b.w);
        const int bottom = minInt(a.y + a.h, b.y + b.h);
        return {left, top, right - left, bottom - top};
    }

    static DisplayRect merge(DisplayRect a, DisplayRect b) {
        normalize(a);
        normalize(b);
        const int left = minInt(a.x, b.x);
        const int top = minInt(a.y, b.y);
        const int right = maxInt(a.x + a.w, b.x + b.w);
        const int bottom = maxInt(a.y + a.h, b.y + b.h);
        return {left, top, right - left, bottom - top};
    }

private:
    std::array<DisplayRect, MAX_RECTS> m_rects{};
    int m_count = 0;

    static constexpr int minInt(int a, int b) {
        return a < b ? a : b;
    }

    static constexpr int maxInt(int a, int b) {
        return a > b ? a : b;
    }

    static void normalize(DisplayRect& rect) {
        if (rect.w < 0) {
            rect.x += rect.w;
            rect.w = -rect.w;
        }
        if (rect.h < 0) {
            rect.y += rect.h;
            rect.h = -rect.h;
        }
    }

    static bool intersectsOrTouches(const DisplayRect& a, const DisplayRect& b) {
        return a.x <= b.x + b.w &&
               b.x <= a.x + a.w &&
               a.y <= b.y + b.h &&
               b.y <= a.y + a.h;
    }

    void coalesceFrom(int index) {
        for (int i = 0; i < m_count; ++i) {
            if (i == index) {
                continue;
            }
            if (intersectsOrTouches(m_rects[index], m_rects[i])) {
                m_rects[index] = merge(m_rects[index], m_rects[i]);
                removeAt(i);
                if (i < index) {
                    index--;
                }
                i = -1;
            }
        }
    }

    void removeAt(int index) {
        for (int i = index; i + 1 < m_count; ++i) {
            m_rects[i] = m_rects[i + 1];
        }
        m_count--;
    }
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

    void setClipRect(DisplayRect rect) {
        if (!clipRect(rect)) {
            m_hasClipRect = true;
            m_clipRect = {0, 0, 0, 0};
            return;
        }
        m_hasClipRect = true;
        m_clipRect = rect;
    }

    void clearClipRect() {
        m_hasClipRect = false;
    }

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

protected:
    bool clipRect(DisplayRect& rect) const {
        if (rect.w <= 0 || rect.h <= 0) {
            return false;
        }

        const DisplayRect bounds = m_hasClipRect
            ? m_clipRect
            : DisplayRect{0, 0, canvasWidth(), canvasHeight()};
        rect = DirtyRegionList::intersect(rect, bounds);
        return !rect.isEmpty();
    }

    bool clipPoint(int& x, int& y) const {
        DisplayRect rect{x, y, 1, 1};
        if (!clipRect(rect)) {
            return false;
        }
        x = rect.x;
        y = rect.y;
        return true;
    }

    virtual int canvasWidth() const {
        return DISPLAY_WIDTH;
    }

    virtual int canvasHeight() const {
        return DISPLAY_HEIGHT;
    }

private:
    bool m_hasClipRect = false;
    DisplayRect m_clipRect{0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT};

    static constexpr int absInt(int value) {
        return value < 0 ? -value : value;
    }
};
