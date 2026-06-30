//
// Created by Miracle Aigbogun on 3/22/26.
//

#include "drivers/st7789/st7789.h"

#include "hardware/spi.h"
#include "pico/stdlib.h"

namespace {

constexpr uint32_t ST7789_SPI_HZ = 40000000;
constexpr int PIXELS_PER_FILL_CHUNK = 64;

inline uint8_t highByte(uint16_t value) {
    return static_cast<uint8_t>(value >> 8);
}

inline uint8_t lowByte(uint16_t value) {
    return static_cast<uint8_t>(value & 0xFF);
}

} // namespace

ST7789::ST7789(spi_inst_t* spi,
               uint pin_cs,
               uint pin_dc,
               uint pin_rst,
               uint pin_sck,
               uint pin_mosi)
    : _spi(spi)
    , _pin_cs(pin_cs)
    , _pin_dc(pin_dc)
    , _pin_rst(pin_rst)
    , _pin_sck(pin_sck)
    , _pin_mosi(pin_mosi) {}

void ST7789::init() {
    spi_init(_spi, ST7789_SPI_HZ);
    spi_set_format(_spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(_pin_sck, GPIO_FUNC_SPI);
    gpio_set_function(_pin_mosi, GPIO_FUNC_SPI);

    gpio_init(_pin_cs);
    gpio_init(_pin_dc);
    gpio_init(_pin_rst);
    gpio_set_dir(_pin_cs, GPIO_OUT);
    gpio_set_dir(_pin_dc, GPIO_OUT);
    gpio_set_dir(_pin_rst, GPIO_OUT);

    csHigh();

    gpio_put(_pin_rst, 1);
    sleep_ms(10);
    gpio_put(_pin_rst, 0);
    sleep_ms(10);
    gpio_put(_pin_rst, 1);
    sleep_ms(120);

    sendCommand(ST7789_SWRESET);
    sleep_ms(150);
    sendCommand(ST7789_SLPOUT);
    sleep_ms(10);

    const uint8_t pixelFormat = 0x55;
    sendCommand(ST7789_COLMOD);
    sendData(&pixelFormat, 1);
    sleep_ms(10);

    const uint8_t madctl = 0xA0;
    sendCommand(ST7789_MADCTL);
    sendData(&madctl, 1);

    sendCommand(ST7789_INVON);
    sleep_ms(10);
    sendCommand(ST7789_NORON);
    sleep_ms(10);
    sendCommand(ST7789_DISPON);
    sleep_ms(10);
}

void ST7789::sendCommand(uint8_t cmd) {
    dcLow();
    csLow();
    spi_write_blocking(_spi, &cmd, 1);
    csHigh();
}

void ST7789::sendData(const uint8_t* data, size_t length) {
    if (!data || length == 0) {
        return;
    }
    dcHigh();
    csLow();
    spi_write_blocking(_spi, data, static_cast<int>(length));
    csHigh();
}

void ST7789::setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    const uint8_t casetData[4] = {highByte(x0), lowByte(x0), highByte(x1), lowByte(x1)};
    const uint8_t rasetData[4] = {highByte(y0), lowByte(y0), highByte(y1), lowByte(y1)};

    csLow();

    dcLow();
    const uint8_t caset = ST7789_CASET;
    spi_write_blocking(_spi, &caset, 1);
    dcHigh();
    spi_write_blocking(_spi, casetData, 4);

    dcLow();
    const uint8_t raset = ST7789_RASET;
    spi_write_blocking(_spi, &raset, 1);
    dcHigh();
    spi_write_blocking(_spi, rasetData, 4);

    dcLow();
    const uint8_t ramwr = ST7789_RAMWR;
    spi_write_blocking(_spi, &ramwr, 1);
    dcHigh();
}

void ST7789::drawPixel(int16_t x, int16_t y, uint16_t color) {
    writeRect(x, y, 1, 1, &color, 1);
}

void ST7789::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
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
    if (x >= ST7789_WIDTH || y >= ST7789_HEIGHT) {
        return;
    }
    if (x + w > ST7789_WIDTH) {
        w = ST7789_WIDTH - x;
    }
    if (y + h > ST7789_HEIGHT) {
        h = ST7789_HEIGHT - y;
    }
    if (w <= 0 || h <= 0) {
        return;
    }

    setWindow(static_cast<uint16_t>(x),
              static_cast<uint16_t>(y),
              static_cast<uint16_t>(x + w - 1),
              static_cast<uint16_t>(y + h - 1));

    const uint8_t hi = highByte(color);
    const uint8_t lo = lowByte(color);
    uint8_t chunk[PIXELS_PER_FILL_CHUNK * 2];
    for (int i = 0; i < PIXELS_PER_FILL_CHUNK; ++i) {
        chunk[i * 2] = hi;
        chunk[i * 2 + 1] = lo;
    }

    int32_t pixelsRemaining = static_cast<int32_t>(w) * static_cast<int32_t>(h);
    while (pixelsRemaining > 0) {
        const int batchPixels = pixelsRemaining > PIXELS_PER_FILL_CHUNK
            ? PIXELS_PER_FILL_CHUNK
            : static_cast<int>(pixelsRemaining);
        spi_write_blocking(_spi, chunk, batchPixels * 2);
        pixelsRemaining -= batchPixels;
    }

    csHigh();
}

void ST7789::writeRect(int16_t x,
                       int16_t y,
                       int16_t w,
                       int16_t h,
                       const uint16_t* pixels,
                       int sourceStridePixels) {
    if (!pixels || w <= 0 || h <= 0 || sourceStridePixels <= 0) {
        return;
    }

    if (x < 0 || y < 0 || x + w > ST7789_WIDTH || y + h > ST7789_HEIGHT) {
        return;
    }

    setWindow(static_cast<uint16_t>(x),
              static_cast<uint16_t>(y),
              static_cast<uint16_t>(x + w - 1),
              static_cast<uint16_t>(y + h - 1));

    uint8_t rowBytes[ST7789_WIDTH * 2];
    for (int row = 0; row < h; ++row) {
        const uint16_t* src = pixels + row * sourceStridePixels;
        for (int col = 0; col < w; ++col) {
            const uint16_t value = src[col];
            rowBytes[col * 2] = highByte(value);
            rowBytes[col * 2 + 1] = lowByte(value);
        }
        spi_write_blocking(_spi, rowBytes, w * 2);
    }

    csHigh();
}

void ST7789::fillScreen(uint16_t color) {
    fillRect(0, 0, ST7789_WIDTH, ST7789_HEIGHT, color);
}
