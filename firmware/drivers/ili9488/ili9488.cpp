//
// Created by Miracle Aigbogun on 7/1/26.
//

#include "drivers/ili9488/ili9488.h"

#include <algorithm>
#include <cstdio>

namespace {

inline uint8_t highByte(uint16_t value) {
    return static_cast<uint8_t>(value >> 8);
}

inline uint8_t lowByte(uint16_t value) {
    return static_cast<uint8_t>(value & 0xFF);
}

static inline uint16_t rgb565_to_bgr565(uint16_t c) {
    return static_cast<uint16_t>(((c & 0xF800) >> 11) |
                                 (c & 0x07E0) |
                                 ((c & 0x001F) << 11));
}

inline uint16_t transportColor(uint16_t color) {
    if (ili9488_config::LCD_SWAP_RB_CHANNELS) {
        return rgb565_to_bgr565(color);
    }
    return color;
}

} // namespace

ILI9488::ILI9488(spi_inst_t* spi,
                 uint pin_cs,
                 uint pin_dc,
                 uint pin_rst,
                 uint pin_sck,
                 uint pin_mosi,
                 uint pin_bl)
    : m_spi(spi)
    , m_pin_cs(pin_cs)
    , m_pin_dc(pin_dc)
    , m_pin_rst(pin_rst)
    , m_pin_sck(pin_sck)
    , m_pin_mosi(pin_mosi)
    , m_pin_bl(pin_bl) {}

namespace {

bool use18BitPixelTransfer() {
    return ili9488_config::LCD_COLMOD_VALUE == ili9488_config::COLMOD_18BIT;
}

} // namespace

void ILI9488::init() {
    std::printf("[display] init started (spi=%s sck=%u mosi=%u cs=%u dc=%u rst=%u bl=%u)\n",
                (m_spi == spi1) ? "spi1" : "spi0",
                m_pin_sck,
                m_pin_mosi,
                m_pin_cs,
                m_pin_dc,
                m_pin_rst,
                m_pin_bl);

    spi_init(m_spi, ili9488_config::SPI_FREQUENCY_HZ);
    spi_set_format(m_spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(m_pin_sck, GPIO_FUNC_SPI);
    gpio_set_function(m_pin_mosi, GPIO_FUNC_SPI);
    std::printf("[display] spi pins configured and clock set to %u Hz\n",
                ili9488_config::SPI_FREQUENCY_HZ);

    gpio_init(m_pin_cs);
    gpio_init(m_pin_dc);
    gpio_init(m_pin_rst);
    gpio_set_dir(m_pin_cs, GPIO_OUT);
    gpio_set_dir(m_pin_dc, GPIO_OUT);
    gpio_set_dir(m_pin_rst, GPIO_OUT);
    csHigh();

    if (m_pin_bl != NO_PIN) {
        gpio_init(m_pin_bl);
        gpio_set_dir(m_pin_bl, GPIO_OUT);
        gpio_put(m_pin_bl, 0);
    }

    gpio_put(m_pin_rst, 1);
    sleep_ms(5);
    gpio_put(m_pin_rst, 0);
    sleep_ms(20);
    gpio_put(m_pin_rst, 1);
    sleep_ms(120);
    std::printf("[display] reset complete\n");

    sendCommand(CMD_SWRESET);
    sleep_ms(120);

    const uint8_t gammaPos[] = {
        0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78,
        0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F
    };
    sendCommand(CMD_GMCTRP1);
    sendData(gammaPos, sizeof(gammaPos));

    const uint8_t gammaNeg[] = {
        0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45,
        0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F
    };
    sendCommand(CMD_GMCTRN1);
    sendData(gammaNeg, sizeof(gammaNeg));

    const uint8_t pwctr3[] = {0x17, 0x15};
    sendCommand(CMD_PWCTR3);
    sendData(pwctr3, sizeof(pwctr3));

    const uint8_t pwctr4 = 0x41;
    sendCommand(CMD_PWCTR4);
    sendData(&pwctr4, 1);

    const uint8_t vmctr1[] = {0x00, 0x12, 0x80};
    sendCommand(CMD_VMCTR1);
    sendData(vmctr1, sizeof(vmctr1));

    const uint8_t madctl = ili9488_config::LANDSCAPE_MADCTL;
    sendCommand(CMD_MADCTL);
    sendData(&madctl, 1);
    std::printf("[display] MADCTL sent: 0x%02X (MY=%d MX=%d MV=%d BGR=%d)\n",
                madctl,
                ili9488_config::LCD_MADCTL_USE_MY ? 1 : 0,
                ili9488_config::LCD_MADCTL_USE_MX ? 1 : 0,
                ili9488_config::LCD_MADCTL_USE_MV ? 1 : 0,
                ili9488_config::LCD_USE_BGR ? 1 : 0);

    const uint8_t colmod = ili9488_config::LCD_COLMOD_VALUE;
    sendCommand(CMD_COLMOD);
    sendData(&colmod, 1);
    std::printf("[display] pixel format sent: 0x%02X\n", colmod);
    sleep_ms(10);

    const uint8_t frmctr1 = 0xA0;
    sendCommand(CMD_FRMCTR1);
    sendData(&frmctr1, 1);

    const uint8_t dfunctr[] = {0x02, 0x02};
    sendCommand(CMD_DFUNCTR);
    sendData(dfunctr, sizeof(dfunctr));

    const uint8_t adjctrl3[] = {0xA9, 0x51, 0x2C, 0x82};
    sendCommand(CMD_ADJCTRL3);
    sendData(adjctrl3, sizeof(adjctrl3));

    sendCommand(CMD_SLPOUT);
    std::printf("[display] sleep out sent\n");
    sleep_ms(120);

    sendCommand(ili9488_config::LCD_INVERT_COLORS ? CMD_INVON : CMD_INVOFF);
    std::printf("[display] inversion mode set to %s\n",
                ili9488_config::LCD_INVERT_COLORS ? "INVON (0x21)" : "INVOFF (0x20)");
    std::printf("[display] RGB/BGR handling: MADCTL_BGR=%d, driver RGB565 swap=%d\n",
                ili9488_config::LCD_USE_BGR ? 1 : 0,
                ili9488_config::LCD_SWAP_RB_CHANNELS ? 1 : 0);

    sendCommand(CMD_DISPON);
    std::printf("[display] display on sent\n");
    sleep_ms(20);

    if (m_pin_bl != NO_PIN) {
        gpio_put(m_pin_bl, 1);
    }

    if (ili9488_config::LCD_RUN_STARTUP_COLOR_TEST) {
        std::printf("[display] color fill test started\n");
        fillScreen(0xF800);
        sleep_ms(ili9488_config::LCD_STARTUP_COLOR_DELAY_MS);
        fillScreen(0x07E0);
        sleep_ms(ili9488_config::LCD_STARTUP_COLOR_DELAY_MS);
        fillScreen(0x001F);
        sleep_ms(ili9488_config::LCD_STARTUP_COLOR_DELAY_MS);
        fillScreen(0xFFFF);
        sleep_ms(ili9488_config::LCD_STARTUP_COLOR_DELAY_MS);
        fillScreen(0x0000);
        sleep_ms(ili9488_config::LCD_STARTUP_COLOR_DELAY_MS);
    }
}

void ILI9488::sendCommand(uint8_t cmd) {
    dcLow();
    csLow();
    spi_write_blocking(m_spi, &cmd, 1);
    csHigh();
}

void ILI9488::sendData(const uint8_t* data, std::size_t length) {
    if (!data || length == 0) {
        return;
    }

    dcHigh();
    csLow();
    spi_write_blocking(m_spi, data, static_cast<int>(length));
    csHigh();
}

void ILI9488::setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    const uint16_t xStart = static_cast<uint16_t>(x0 + ili9488_config::X_OFFSET);
    const uint16_t xEnd = static_cast<uint16_t>(x1 + ili9488_config::X_OFFSET);
    const uint16_t yStart = static_cast<uint16_t>(y0 + ili9488_config::Y_OFFSET);
    const uint16_t yEnd = static_cast<uint16_t>(y1 + ili9488_config::Y_OFFSET);

    const uint8_t casetData[4] = {
        highByte(xStart), lowByte(xStart), highByte(xEnd), lowByte(xEnd)
    };
    const uint8_t rasetData[4] = {
        highByte(yStart), lowByte(yStart), highByte(yEnd), lowByte(yEnd)
    };

    csLow();

    dcLow();
    const uint8_t caset = CMD_CASET;
    spi_write_blocking(m_spi, &caset, 1);
    dcHigh();
    spi_write_blocking(m_spi, casetData, 4);

    dcLow();
    const uint8_t raset = CMD_RASET;
    spi_write_blocking(m_spi, &raset, 1);
    dcHigh();
    spi_write_blocking(m_spi, rasetData, 4);

    dcLow();
    const uint8_t ramwr = CMD_RAMWR;
    spi_write_blocking(m_spi, &ramwr, 1);
    dcHigh();
}

void ILI9488::rgb565ToRgb888(uint16_t color, uint8_t& r, uint8_t& g, uint8_t& b) {
    const uint8_t r5 = static_cast<uint8_t>((color >> 11) & 0x1F);
    const uint8_t g6 = static_cast<uint8_t>((color >> 5) & 0x3F);
    const uint8_t b5 = static_cast<uint8_t>(color & 0x1F);

    r = static_cast<uint8_t>((r5 << 3) | (r5 >> 2));
    g = static_cast<uint8_t>((g6 << 2) | (g6 >> 4));
    b = static_cast<uint8_t>((b5 << 3) | (b5 >> 2));
}

void ILI9488::drawPixel(int16_t x, int16_t y, uint16_t color) {
    writeRect(x, y, 1, 1, &color, 1);
}

void ILI9488::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (w <= 0 || h <= 0) {
        return;
    }

    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x >= ili9488_config::LANDSCAPE_WIDTH ||
        y >= ili9488_config::LANDSCAPE_HEIGHT) {
        return;
    }
    if (x + w > ili9488_config::LANDSCAPE_WIDTH) {
        w = static_cast<int16_t>(ili9488_config::LANDSCAPE_WIDTH - x);
    }
    if (y + h > ili9488_config::LANDSCAPE_HEIGHT) {
        h = static_cast<int16_t>(ili9488_config::LANDSCAPE_HEIGHT - y);
    }
    if (w <= 0 || h <= 0) {
        return;
    }

    setWindow(static_cast<uint16_t>(x),
              static_cast<uint16_t>(y),
              static_cast<uint16_t>(x + w - 1),
              static_cast<uint16_t>(y + h - 1));

    uint8_t chunk[PIXELS_PER_FILL_CHUNK * 3];
    const bool use18Bit = use18BitPixelTransfer();
    const int bytesPerPixel = use18Bit ? 3 : 2;
    const uint16_t transport = transportColor(color);

    if (use18Bit) {
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        rgb565ToRgb888(transport, r, g, b);
        for (int i = 0; i < PIXELS_PER_FILL_CHUNK; ++i) {
            const int base = i * 3;
            chunk[base] = r;
            chunk[base + 1] = g;
            chunk[base + 2] = b;
        }
    } else {
        const uint8_t hi = highByte(transport);
        const uint8_t lo = lowByte(transport);
        for (int i = 0; i < PIXELS_PER_FILL_CHUNK; ++i) {
            const int base = i * 2;
            chunk[base] = hi;
            chunk[base + 1] = lo;
        }
    }

    int32_t pixelsRemaining = static_cast<int32_t>(w) * static_cast<int32_t>(h);
    while (pixelsRemaining > 0) {
        const int32_t batchPixels = pixelsRemaining > PIXELS_PER_FILL_CHUNK
            ? PIXELS_PER_FILL_CHUNK
            : pixelsRemaining;
        spi_write_blocking(m_spi, chunk, batchPixels * bytesPerPixel);
        pixelsRemaining -= batchPixels;
    }

    csHigh();
}

void ILI9488::writeRect(int16_t x,
                        int16_t y,
                        int16_t w,
                        int16_t h,
                        const uint16_t* pixels,
                        int sourceStridePixels) {
    if (!pixels || w <= 0 || h <= 0 || sourceStridePixels <= 0) {
        return;
    }

    if (x < 0 || y < 0 ||
        x + w > ili9488_config::LANDSCAPE_WIDTH ||
        y + h > ili9488_config::LANDSCAPE_HEIGHT) {
        return;
    }

    setWindow(static_cast<uint16_t>(x),
              static_cast<uint16_t>(y),
              static_cast<uint16_t>(x + w - 1),
              static_cast<uint16_t>(y + h - 1));

    const bool use18Bit = use18BitPixelTransfer();
    for (int row = 0; row < h; ++row) {
        const uint16_t* src = pixels + row * sourceStridePixels;

        if (use18Bit) {
            for (int col = 0; col < w; ++col) {
                uint8_t r = 0;
                uint8_t g = 0;
                uint8_t b = 0;
                rgb565ToRgb888(transportColor(src[col]), r, g, b);

                const int base = col * 3;
                m_rowBuffer[base] = r;
                m_rowBuffer[base + 1] = g;
                m_rowBuffer[base + 2] = b;
            }
            spi_write_blocking(m_spi, m_rowBuffer.data(), w * 3);
        } else {
            for (int col = 0; col < w; ++col) {
                const uint16_t value = transportColor(src[col]);
                const int base = col * 2;
                // RGB565 on SPI must be sent high byte first, low byte second.
                m_rowBuffer[base] = highByte(value);
                m_rowBuffer[base + 1] = lowByte(value);
            }
            spi_write_blocking(m_spi, m_rowBuffer.data(), w * 2);
        }
    }

    csHigh();
}

void ILI9488::fillScreen(uint16_t color) {
    fillRect(0,
             0,
             static_cast<int16_t>(ili9488_config::LANDSCAPE_WIDTH),
             static_cast<int16_t>(ili9488_config::LANDSCAPE_HEIGHT),
             color);
}
