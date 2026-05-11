#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstddef>

#include "../firmware/hal/display.h"

class PixelTrackingDisplay : public Display {
public:
    void init() override {}
    void clear(Color color) override {
        pixels.fill(color.rgb565());
    }

    void drawPixel(int x, int y, Color color) override {
        if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) {
            return;
        }
        pixels[static_cast<std::size_t>(y * DISPLAY_WIDTH + x)] = color.rgb565();
    }

    void fillRect(int x, int y, int w, int h, Color color) override {
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
        if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) {
            return;
        }
        if (x + w > DISPLAY_WIDTH) {
            w = DISPLAY_WIDTH - x;
        }
        if (y + h > DISPLAY_HEIGHT) {
            h = DISPLAY_HEIGHT - y;
        }
        if (w <= 0 || h <= 0) {
            return;
        }

        for (int row = 0; row < h; row++) {
            for (int col = 0; col < w; col++) {
                drawPixel(x + col, y + row, color);
            }
        }
    }

    void drawText(const char*, int, int, Color) override {}
    void present() override {}

    uint16_t at(int x, int y) const {
        return pixels[static_cast<std::size_t>(y * DISPLAY_WIDTH + x)];
    }

    std::array<uint16_t, DISPLAY_WIDTH * DISPLAY_HEIGHT> pixels{};
};

TEST(DisplayPrimitives, PixelLineRectAndFillRectRenderThroughSharedApi) {
    PixelTrackingDisplay display;
    display.clear(Display::BLACK);

    display.drawPixel(2, 3, Display::WHITE);
    EXPECT_EQ(display.at(2, 3), Display::WHITE.rgb565());

    display.drawLine(0, 0, 3, 3, Display::RED);
    EXPECT_EQ(display.at(0, 0), Display::RED.rgb565());
    EXPECT_EQ(display.at(1, 1), Display::RED.rgb565());
    EXPECT_EQ(display.at(2, 2), Display::RED.rgb565());
    EXPECT_EQ(display.at(3, 3), Display::RED.rgb565());

    display.drawRect(10, 10, 4, 3, Display::GREEN);
    EXPECT_EQ(display.at(10, 10), Display::GREEN.rgb565());
    EXPECT_EQ(display.at(13, 10), Display::GREEN.rgb565());
    EXPECT_EQ(display.at(10, 12), Display::GREEN.rgb565());
    EXPECT_EQ(display.at(13, 12), Display::GREEN.rgb565());
    EXPECT_EQ(display.at(11, 11), Display::BLACK.rgb565());

    display.fillRect(20, 20, 3, 2, Display::BLUE);
    EXPECT_EQ(display.at(20, 20), Display::BLUE.rgb565());
    EXPECT_EQ(display.at(22, 21), Display::BLUE.rgb565());
}

TEST(DisplayPrimitives, OutOfBoundsAndEmptyRectanglesAreClippedOrIgnored) {
    PixelTrackingDisplay display;
    display.clear(Display::BLACK);

    display.drawPixel(-1, -1, Display::WHITE);
    display.fillRect(-2, -2, 4, 4, Display::RED);
    display.fillRect(5, 5, 0, 3, Display::GREEN);
    display.fillRect(5, 5, 3, -1, Display::GREEN);
    display.drawLine(-2, -2, 1, 1, Display::BLUE);

    EXPECT_EQ(display.at(0, 0), Display::BLUE.rgb565());
    EXPECT_EQ(display.at(1, 1), Display::BLUE.rgb565());
    EXPECT_EQ(display.at(2, 2), Display::BLACK.rgb565());
    EXPECT_EQ(display.at(5, 5), Display::BLACK.rgb565());
}
