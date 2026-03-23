//
// Created by Miracle Aigbogun on 3/22/26.
//

#include "display_rp2350.h"
#include "graphics/font.h"

DisplayRP2350::DisplayRP2350()
    : m_display(spi1, 13, 14, 15, 10, 11)
    // spi1, CS=GP13, DC=GP14, RST=GP15, SCK=GP10, MOSI=GP11
{}

void DisplayRP2350::init() {
    m_display.init();
    clear(Display::BLACK);
}

void DisplayRP2350::clear(uint16_t color) {
    // Note: DISPLAY_WIDTH and DISPLAY_HEIGHT in display.h are 320x240
    // (landscape) but the ST7789 is physically 240x320 (portrait).
    // We swap them here so the rest of the codebase sees a landscape display
    // matching the simulator, without changing anything else.
    m_display.fillRect(0, 0, ST7789_WIDTH, ST7789_HEIGHT, color);
}

void DisplayRP2350::drawPixel(int x, int y, uint16_t color) {
    m_display.drawPixel(x, y, color);
}

void DisplayRP2350::drawRect(int x, int y, int w, int h, uint16_t color) {
    m_display.fillRect(x, y, w, h, color);
}

void DisplayRP2350::drawChar(char c, int x, int y, uint16_t color) {
    uint8_t idx = (uint8_t)c;
    // Each character is 5 bytes in FONT_DATA, one byte per column
    // Each byte represents 8 vertical pixels, LSB at the top
    const uint8_t* bitmap = &FONT_DATA[idx * FONT_CHAR_WIDTH];

    for (int col = 0; col < FONT_CHAR_WIDTH; col++) {
        uint8_t colData = bitmap[col];
        for (int row = 0; row < FONT_CHAR_HEIGHT; row++) {
            if (colData & (1 << row)) {
                m_display.drawPixel(x + col, y + row, color);
            }
        }
    }
}

void DisplayRP2350::drawText(const char* text, int x, int y, uint16_t color) {
    int cursorX = x;
    while (*text) {
        drawChar(*text, cursorX, y, color);
        cursorX += FONT_CHAR_ADVANCE;
        text++;
    }
}

void DisplayRP2350::present() {
    // ST7789 is write-through — pixels go directly to the display
    // as they're drawn, so there's nothing to flush here.
    // This method exists to satisfy the Display interface.
}