//
// Created by Miracle Aigbogun on 3/22/26.
//

#include "st7789.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"

ST7789::ST7789(spi_inst_t* spi, uint pin_cs, uint pin_dc, uint pin_rst, uint pin_sck, uint pin_mosi)
    : _spi(spi), _pin_cs(pin_cs), _pin_dc(pin_dc), _pin_rst(pin_rst),
      _pin_sck(pin_sck), _pin_mosi(pin_mosi) {}

void ST7789::init() {
    // Initialize SPI at 40MHz
    spi_init(_spi, 40000000);
    spi_set_format(_spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    // Set up GPIO pins
    gpio_set_function(_pin_sck,  GPIO_FUNC_SPI);
    gpio_set_function(_pin_mosi, GPIO_FUNC_SPI);

    gpio_init(_pin_cs);
    gpio_init(_pin_dc);
    gpio_init(_pin_rst);
    gpio_set_dir(_pin_cs,  GPIO_OUT);
    gpio_set_dir(_pin_dc,  GPIO_OUT);
    gpio_set_dir(_pin_rst, GPIO_OUT);

    csHigh();

    // Hardware reset — pulse the RST pin low then high
    // The display needs this before it will accept commands
    gpio_put(_pin_rst, 1);
    sleep_ms(10);
    gpio_put(_pin_rst, 0);
    sleep_ms(10);
    gpio_put(_pin_rst, 1);
    sleep_ms(120); // ST7789 needs 120ms after reset before commands

    // Initialization sequence
    sendCommand(ST7789_SWRESET); sleep_ms(150);
    sendCommand(ST7789_SLPOUT);  sleep_ms(10);

    // Pixel format: 16 bits per pixel (RGB565)
    // 0x55 means 16-bit for both RGB and MCU interface
    sendCommand(ST7789_COLMOD);
    sendData(0x55);
    sleep_ms(10);

    // Memory access control — 0x60 = landscape (rotated 90 degrees)
    // Bit breakdown:
    // MX (bit 6) = 1 — mirror X axis
    // MV (bit 5) = 1 — swap X and Y axes (this is what rotates 90 degrees)
    // All other bits 0
    sendCommand(ST7789_MADCTL);
    sendData(0xA0);

    // 0x00 portrait normal
    // 0xC0 portrait inverted (180)
    // 0x60 lanscape normal
    // 0xA0 landscape inverted (180)

    sendCommand(ST7789_INVON);  sleep_ms(10); // needed for ST7789
    sendCommand(ST7789_NORON);  sleep_ms(10);
    sendCommand(ST7789_DISPON); sleep_ms(10);
}

void ST7789::sendCommand(uint8_t cmd) {
    dcLow();  // DC low = command mode
    csLow();
    spi_write_blocking(_spi, &cmd, 1);
    csHigh();
}

void ST7789::sendData(uint8_t data) {
    dcHigh(); // DC high = data mode
    csLow();
    spi_write_blocking(_spi, &data, 1);
    csHigh();
}

void ST7789::sendData16(uint16_t data) {
    // ST7789 expects 16-bit color in big-endian order
    // (high byte first) but the RP2350 is little-endian,
    // so we need to swap the bytes
    uint8_t bytes[2] = { (uint8_t)(data >> 8), (uint8_t)(data & 0xFF) };
    dcHigh();
    csLow();
    spi_write_blocking(_spi, bytes, 2);
    csHigh();
}

void ST7789::setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // Tell the display which rectangle of pixels we're about to write to
    sendCommand(ST7789_CASET); // column (x) range
    sendData(x0 >> 8); sendData(x0 & 0xFF);
    sendData(x1 >> 8); sendData(x1 & 0xFF);

    sendCommand(ST7789_RASET); // row (y) range
    sendData(y0 >> 8); sendData(y0 & 0xFF);
    sendData(y1 >> 8); sendData(y1 & 0xFF);

    sendCommand(ST7789_RAMWR); // begin writing pixels
}

void ST7789::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= ST7789_WIDTH || y < 0 || y >= ST7789_HEIGHT) return;
    setWindow(x, y, x, y);
    sendData16(color);
}

void ST7789::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    setWindow(x, y, x + w - 1, y + h - 1);
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    dcHigh();
    csLow();
    for (int32_t i = w * h; i > 0; i--) {
        spi_write_blocking(_spi, &hi, 1);
        spi_write_blocking(_spi, &lo, 1);
    }
    csHigh();
}

void ST7789::fillScreen(uint16_t color) {
    fillRect(0, 0, ST7789_WIDTH, ST7789_HEIGHT, color);
}