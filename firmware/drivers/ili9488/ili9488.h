//
// Created by Miracle Aigbogun on 7/1/26.
// ILI9488 SPI display driver for 320x480 panels (landscape render target 480x320).
//

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "hardware/spi.h"
#include "pico/stdlib.h"

enum class TftControllerType : uint8_t {
    ILI9488 = 0,
};

namespace ili9488_config {

constexpr TftControllerType CONTROLLER = TftControllerType::ILI9488;

constexpr uint16_t NATIVE_WIDTH = 320;
constexpr uint16_t NATIVE_HEIGHT = 480;
constexpr uint16_t LANDSCAPE_WIDTH = 480;
constexpr uint16_t LANDSCAPE_HEIGHT = 320;

// MADCTL bit definitions (0x36).
constexpr uint8_t MADCTL_MY = 0x80;   // Row address order
constexpr uint8_t MADCTL_MX = 0x40;   // Column address order
constexpr uint8_t MADCTL_MV = 0x20;   // Row/column exchange
constexpr uint8_t MADCTL_BGR = 0x08;  // RGB/BGR color order select

// Orientation bits for the current landscape layout.
constexpr bool LCD_MADCTL_USE_MY = false;
constexpr bool LCD_MADCTL_USE_MX = false;
constexpr bool LCD_MADCTL_USE_MV = true;

// First attempt to correct red/blue swap through MADCTL only.
constexpr bool LCD_USE_BGR = false;

// Fallback for panels that ignore MADCTL color order and still swap red/blue.
constexpr bool LCD_SWAP_RB_CHANNELS = true;

// Display inversion control: false sends INVOFF (0x20), true sends INVON (0x21).
constexpr bool LCD_INVERT_COLORS = true;

constexpr uint8_t LANDSCAPE_MADCTL =
    (LCD_MADCTL_USE_MY ? MADCTL_MY : 0x00) |
    (LCD_MADCTL_USE_MX ? MADCTL_MX : 0x00) |
    (LCD_MADCTL_USE_MV ? MADCTL_MV : 0x00) |
    (LCD_USE_BGR ? MADCTL_BGR : 0x00);

constexpr uint16_t X_OFFSET = 0;
constexpr uint16_t Y_OFFSET = 0;

// Conservative default for bring-up. Raise after signal-integrity validation.
constexpr uint32_t SPI_FREQUENCY_HZ = 10000000;

// ILI9488 SPI write path is configured for 18-bit pixel transfer (RGB666 payload).
constexpr uint8_t COLMOD_16BIT = 0x55;
constexpr uint8_t COLMOD_18BIT = 0x66;
// Set to COLMOD_16BIT for ST7796-like panels, COLMOD_18BIT for ILI9488 18-bit SPI path.
constexpr uint8_t LCD_COLMOD_VALUE = COLMOD_16BIT;

constexpr bool LCD_RUN_STARTUP_COLOR_TEST = false;
constexpr uint32_t LCD_STARTUP_COLOR_DELAY_MS = 300;

} // namespace ili9488_config

class ILI9488 {
public:
    static constexpr uint NO_PIN = static_cast<uint>(-1);

    ILI9488(spi_inst_t* spi,
            uint pin_cs,
            uint pin_dc,
            uint pin_rst,
            uint pin_sck,
            uint pin_mosi,
            uint pin_bl = NO_PIN);

    void init();
    void fillScreen(uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void writeRect(int16_t x,
                   int16_t y,
                   int16_t w,
                   int16_t h,
                   const uint16_t* pixels,
                   int sourceStridePixels);

    static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
        return static_cast<uint16_t>(((r & 0xF8) << 8) |
                                     ((g & 0xFC) << 3) |
                                     (b >> 3));
    }

private:
    static constexpr uint8_t CMD_SWRESET = 0x01;
    static constexpr uint8_t CMD_SLPOUT = 0x11;
    static constexpr uint8_t CMD_INVOFF = 0x20;
    static constexpr uint8_t CMD_INVON = 0x21;
    static constexpr uint8_t CMD_DISPON = 0x29;
    static constexpr uint8_t CMD_CASET = 0x2A;
    static constexpr uint8_t CMD_RASET = 0x2B;
    static constexpr uint8_t CMD_RAMWR = 0x2C;
    static constexpr uint8_t CMD_MADCTL = 0x36;
    static constexpr uint8_t CMD_COLMOD = 0x3A;
    static constexpr uint8_t CMD_FRMCTR1 = 0xB1;
    static constexpr uint8_t CMD_DFUNCTR = 0xB6;
    static constexpr uint8_t CMD_PWCTR3 = 0xC0;
    static constexpr uint8_t CMD_PWCTR4 = 0xC1;
    static constexpr uint8_t CMD_VMCTR1 = 0xC5;
    static constexpr uint8_t CMD_GMCTRP1 = 0xE0;
    static constexpr uint8_t CMD_GMCTRN1 = 0xE1;
    static constexpr uint8_t CMD_ADJCTRL3 = 0xF7;

    static constexpr int PIXELS_PER_FILL_CHUNK = 64;

    spi_inst_t* m_spi;
    uint m_pin_cs;
    uint m_pin_dc;
    uint m_pin_rst;
    uint m_pin_sck;
    uint m_pin_mosi;
    uint m_pin_bl;

    std::array<uint8_t, ili9488_config::LANDSCAPE_WIDTH * 3> m_rowBuffer{};

    void sendCommand(uint8_t cmd);
    void sendData(const uint8_t* data, std::size_t length);
    void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    static void rgb565ToRgb888(uint16_t color, uint8_t& r, uint8_t& g, uint8_t& b);

    void csLow() { gpio_put(m_pin_cs, 0); }
    void csHigh() { gpio_put(m_pin_cs, 1); }
    void dcLow() { gpio_put(m_pin_dc, 0); }
    void dcHigh() { gpio_put(m_pin_dc, 1); }
};
