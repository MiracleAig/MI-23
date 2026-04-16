//
// Created by Miracle Aigbogun on 3/22/26.
// Minimal ST7789 Driver for RP2350 for Pico SDK SPI
//


#pragma once
#include "pico/stdlib.h"
#include "hardware/spi.h"

// Display dimensions
#define ST7789_WIDTH  320
#define ST7789_HEIGHT 240

// ST7789 command definitions
#define ST7789_SWRESET  0x01  // Software reset
#define ST7789_SLPOUT   0x11  // Sleep out
#define ST7789_NORON    0x13  // Normal display mode on
#define ST7789_INVON    0x21  // Display inversion on (ST7789 needs this)
#define ST7789_DISPON   0x29  // Display on
#define ST7789_CASET    0x2A  // Column address set
#define ST7789_RASET    0x2B  // Row address set
#define ST7789_RAMWR    0x2C  // Memory write
#define ST7789_MADCTL   0x36  // Memory data access control (rotation)
#define ST7789_COLMOD   0x3A  // Pixel format set

// Color definitions in RGB565 format
// RGB565 packs a pixel into 16 bits: 5 bits red, 6 bits green, 5 bits blue
#define ST7789_BLACK    0x0000
#define ST7789_WHITE    0xFFFF
#define ST7789_RED      0xF800
#define ST7789_GREEN    0x07E0
#define ST7789_BLUE     0x001F

class ST7789 {
public:
    ST7789(spi_inst_t* spi, uint pin_cs, uint pin_dc, uint pin_rst, uint pin_sck, uint pin_mosi);

    void init();
    void fillScreen(uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void drawPixel(int16_t x, int16_t y, uint16_t color);

    // Convert RGB888 (normal 0-255 per channel) to RGB565
    static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }

private:
    spi_inst_t* _spi;
    uint _pin_cs, _pin_dc, _pin_rst, _pin_sck, _pin_mosi;

    void sendCommand(uint8_t cmd);
    void sendData(uint8_t data);
    void sendData16(uint16_t data);
    void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void csLow()  { gpio_put(_pin_cs, 0); }
    void csHigh() { gpio_put(_pin_cs, 1); }
    void dcLow()  { gpio_put(_pin_dc, 0); } // low = command
    void dcHigh() { gpio_put(_pin_dc, 1); } // high = data
};