#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include "../firmware/app/home/calculator_home.h"

class HomeDisplayStub : public Display {
public:
    void init() override {}

    void clear(Color color) override {
        fillRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, color);
    }

    void drawPixel(int x, int y, Color color) override {
        if (!clipPoint(x, y) || x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) {
            return;
        }
        pixels[static_cast<std::size_t>(y * DISPLAY_WIDTH + x)] = color.rgb565();
    }

    void fillRect(int x, int y, int w, int h, Color color) override {
        DisplayRect rect{x, y, w, h};
        if (!clipRect(rect)) {
            return;
        }
        rect = DirtyRegionList::intersect(rect, {0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT});
        if (rect.isEmpty()) {
            return;
        }

        for (int row = 0; row < rect.h; ++row) {
            for (int col = 0; col < rect.w; ++col) {
                drawPixel(rect.x + col, rect.y + row, color);
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

TEST(HomeScreen, SelectionOutlineMovesWhenArrowKeyChangesSelection) {
    HomeDisplayStub display;
    HomeScreen home(display);

    constexpr int columns = 4;
    constexpr int horizontalPadding = 32;
    constexpr int tileWidth = 88;
    constexpr int cellWidth = (DISPLAY_WIDTH - horizontalPadding * 2) / columns;
    constexpr int tileY = 131;
    constexpr int firstTileX = horizontalPadding + (cellWidth - tileWidth) / 2;
    constexpr int secondTileX = firstTileX + cellWidth;

    const uint16_t selectedOutline = Display::rgb(255, 230, 95).rgb565();
    const uint16_t unselectedOutline = Display::rgb(94, 102, 116).rgb565();

    home.enter();
    home.renderContent(22, DISPLAY_HEIGHT - 22);

    EXPECT_EQ(display.at(firstTileX, tileY), selectedOutline);
    EXPECT_EQ(display.at(secondTileX, tileY), unselectedOutline);

    home.handleKey(Key::CURSOR_RIGHT);
    home.renderContent(22, DISPLAY_HEIGHT - 22);

    EXPECT_EQ(display.at(firstTileX, tileY), unselectedOutline);
    EXPECT_EQ(display.at(secondTileX, tileY), selectedOutline);
}
