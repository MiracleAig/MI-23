//
// Created by Miracle Aigbogun on 3/22/26.
//

#pragma once
#include "hal/display.h"
#include "st7789.h"
#include "hardware/spi.h"

class DisplayRP2350 : public Display {
public:
    DisplayRP2350();
    ~DisplayRP2350() override = default;

    void init() override;
    void clear(uint16_t color) override;
    void drawPixel(int x, int y, uint16_t color) override;
    void drawRect(int x, int y, int w, int h, uint16_t color) override;
    void drawText(const char* text, int x, int y, uint16_t color) override;
    void present() override;

private:
    void drawChar(char c, int x, int y, uint16_t color);
    ST7789 m_display;
};