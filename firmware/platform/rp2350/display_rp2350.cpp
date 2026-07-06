//
// Created by Miracle Aigbogun on 3/22/26.
//

#include "platform/rp2350/display_rp2350.h"

#include "graphics/font.h"
#include "platform/rp2350/config/pin_config.h"

#include <algorithm>
#include <cstdio>

#include "pico/time.h"

namespace {

constexpr int FULL_SCREEN_PIXELS = DISPLAY_WIDTH * DISPLAY_HEIGHT;

uint32_t elapsedUs(uint32_t startUs) {
    return time_us_32() - startUs;
}

} // namespace

DisplayRP2350::DisplayRP2350()
    : m_display(spi1,
                PIN_DISPLAY_CS,
                PIN_DISPLAY_DC,
                PIN_DISPLAY_RST,
                PIN_DISPLAY_SCK,
                PIN_DISPLAY_MOSI,
                PIN_DISPLAY_BL) {
    m_framebuffer.fill(Display::BLACK.rgb565());
}

void DisplayRP2350::init() {
    m_display.init();
    markDirty({0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT});
    present();
}

void DisplayRP2350::markDirty(const DisplayRect& rect) {
    m_presentRegions.add(rect);
}

void DisplayRP2350::clear(Color color) {
    DisplayRect rect{0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT};
    if (!clipRect(rect)) {
        return;
    }

    const uint32_t startUs = time_us_32();
    const uint16_t value = color.rgb565();
    for (int row = 0; row < rect.h; ++row) {
        uint16_t* line = &m_framebuffer[static_cast<std::size_t>((rect.y + row) *
                                                                 DISPLAY_WIDTH +
                                                                 rect.x)];
        std::fill(line, line + rect.w, value);
    }
    if (m_timingLogsEnabled && rect.w * rect.h == FULL_SCREEN_PIXELS) {
        const uint32_t us = elapsedUs(startUs);
        std::printf("[display] framebuffer clear full-screen pixels=%d time=%lu us (%lu ms)\n",
                    FULL_SCREEN_PIXELS,
                    static_cast<unsigned long>(us),
                    static_cast<unsigned long>((us + 500) / 1000));
    }
    markDirty(rect);
}

void DisplayRP2350::drawPixel(int x, int y, Color color) {
    if (!clipPoint(x, y) ||
        x < 0 || x >= DISPLAY_WIDTH ||
        y < 0 || y >= DISPLAY_HEIGHT) {
        return;
    }

    m_framebuffer[static_cast<std::size_t>(y * DISPLAY_WIDTH + x)] = color.rgb565();
    markDirty({x, y, 1, 1});
}

void DisplayRP2350::fillRect(int x, int y, int w, int h, Color color) {
    DisplayRect rect{x, y, w, h};
    if (!clipRect(rect)) {
        return;
    }
    rect = DirtyRegionList::intersect(rect, {0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT});
    if (rect.isEmpty()) {
        return;
    }

    const uint16_t value = color.rgb565();
    for (int row = 0; row < rect.h; ++row) {
        uint16_t* line = &m_framebuffer[static_cast<std::size_t>((rect.y + row) *
                                                                 DISPLAY_WIDTH +
                                                                 rect.x)];
        std::fill(line, line + rect.w, value);
    }
    markDirty(rect);
}

void DisplayRP2350::drawChar(char c, int x, int y, Color color, int scale) {
    const uint8_t idx = static_cast<uint8_t>(c);
    const uint8_t* bitmap = &FONT_DATA[idx * FONT_CHAR_WIDTH];
    const uint16_t value = color.rgb565();

    for (int col = 0; col < FONT_CHAR_WIDTH; ++col) {
        const uint8_t colData = bitmap[col];
        for (int row = 0; row < FONT_CHAR_HEIGHT; ++row) {
            if ((colData & (1U << row)) == 0) {
                continue;
            }

            for (int sy = 0; sy < scale; ++sy) {
                for (int sx = 0; sx < scale; ++sx) {
                    int px = x + col * scale + sx;
                    int py = y + row * scale + sy;
                    if (!clipPoint(px, py) ||
                        px < 0 || px >= DISPLAY_WIDTH ||
                        py < 0 || py >= DISPLAY_HEIGHT) {
                        continue;
                    }
                    m_framebuffer[static_cast<std::size_t>(py * DISPLAY_WIDTH + px)] = value;
                }
            }
        }
    }
}

void DisplayRP2350::drawText(const char* text, int x, int y, Color color) {
    drawText(text, x, y, color, 1);
}

void DisplayRP2350::drawText(const char* text, int x, int y, Color color, int scale) {
    if (!text) {
        return;
    }

    int cursorX = x;
    while (*text != '\0') {
        drawChar(*text, cursorX, y, color, scale);
        cursorX += FONT_CHAR_ADVANCE * scale;
        ++text;
    }

    markDirty({x, y, cursorX - x, FONT_CHAR_HEIGHT * std::max(1, scale)});
}

void DisplayRP2350::present() {
    if (m_presentRegions.empty()) {
        return;
    }

    const uint32_t startUs = time_us_32();
    const int regionCount = m_presentRegions.count();
    int totalPixels = 0;

    for (int i = 0; i < regionCount; ++i) {
        const DisplayRect rect = m_presentRegions.rect(i);
        totalPixels += rect.w * rect.h;
        const uint16_t* pixels = &m_framebuffer[static_cast<std::size_t>(rect.y *
                                                                         DISPLAY_WIDTH +
                                                                         rect.x)];
        m_display.writeRect(rect.x,
                            rect.y,
                            rect.w,
                            rect.h,
                            pixels,
                            DISPLAY_WIDTH);
    }

    if (m_timingLogsEnabled) {
        const uint32_t us = elapsedUs(startUs);
        std::printf("[display] present rects=%d pixels=%d time=%lu us (%lu ms)%s\n",
                    regionCount,
                    totalPixels,
                    static_cast<unsigned long>(us),
                    static_cast<unsigned long>((us + 500) / 1000),
                    totalPixels >= FULL_SCREEN_PIXELS ? " full-screen" : "");
    }

    m_presentRegions.clear();
}

void DisplayRP2350::setTimingLogsEnabled(bool enabled) {
    m_timingLogsEnabled = enabled;
    m_display.setTimingLogsEnabled(enabled);
}
