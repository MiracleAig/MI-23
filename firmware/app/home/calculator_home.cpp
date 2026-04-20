//
// Created by miracleaigbogun on 4/19/26.
//

#include "app/home/calculator_home.h"
#include "graphics/font.h"
#include <algorithm>

namespace {

enum class HomeIcon {
    Calculator,
    Graphing,
};

struct HomeAppDefinition {
    AppId id;
    const char* label;
    HomeIcon icon;
};

constexpr HomeAppDefinition HOME_APPS[] = {
    { AppId::Calculator, "Calculator", HomeIcon::Calculator },
    { AppId::Graphing,   "Graphing",   HomeIcon::Graphing   },
};

constexpr int APP_COUNT = static_cast<int>(sizeof(HOME_APPS) / sizeof(HOME_APPS[0]));
constexpr int GRID_COLS = 3;
constexpr int TILE_W = 82;
constexpr int TILE_H = 72;
constexpr int TILE_GAP = 14;
constexpr int GRID_LEFT = (DISPLAY_WIDTH - (GRID_COLS * TILE_W + (GRID_COLS - 1) * TILE_GAP)) / 2;

const uint16_t COLOR_BG = Display::rgb(8, 10, 14);
const uint16_t COLOR_HEADER = Display::rgb(22, 35, 48);
const uint16_t COLOR_TILE = Display::rgb(30, 34, 42);
const uint16_t COLOR_TILE_SELECTED = Display::rgb(36, 86, 132);
const uint16_t COLOR_TILE_BORDER = Display::rgb(94, 102, 116);
const uint16_t COLOR_FOCUS = Display::rgb(255, 230, 95);
const uint16_t COLOR_MUTED = Display::rgb(150, 160, 172);

int appCount() {
    return APP_COUNT;
}

int appIndex(AppId id) {
    for (int i = 0; i < appCount(); i++) {
        if (HOME_APPS[i].id == id) {
            return i;
        }
    }
    return 0;
}

void drawOutline(Display& display,
                 int x,
                 int y,
                 int w,
                 int h,
                 uint16_t color,
                 int thickness = 1) {
    display.drawRect(x, y, w, thickness, color);
    display.drawRect(x, y + h - thickness, w, thickness, color);
    display.drawRect(x, y, thickness, h, color);
    display.drawRect(x + w - thickness, y, thickness, h, color);
}

void drawCenteredText(Display& display,
                      const char* text,
                      int x,
                      int y,
                      int w,
                      uint16_t color) {
    const int textX = x + (w - Display::textWidth(text)) / 2;
    display.drawText(text, textX, y, color);
}

void drawCalculatorIcon(Display& display, int x, int y, uint16_t color) {
    drawOutline(display, x, y, 34, 32, color, 2);
    display.drawRect(x + 5, y + 5, 24, 7, color);

    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            display.drawRect(x + 6 + col * 8,
                             y + 16 + row * 5,
                             4,
                             3,
                             color);
        }
    }
}

void drawGraphingIcon(Display& display, int x, int y, uint16_t color) {
    drawOutline(display, x, y, 38, 32, color, 2);
    display.drawRect(x + 7, y + 6, 2, 20, color);
    display.drawRect(x + 7, y + 24, 24, 2, color);

    display.drawRect(x + 11, y + 20, 3, 3, color);
    display.drawRect(x + 15, y + 17, 3, 3, color);
    display.drawRect(x + 19, y + 13, 3, 3, color);
    display.drawRect(x + 23, y + 10, 3, 3, color);
    display.drawRect(x + 27, y + 12, 3, 3, color);
}

void drawIcon(Display& display,
              const HomeAppDefinition& app,
              int centerX,
              int topY,
              uint16_t color) {
    if (app.icon == HomeIcon::Calculator) {
        drawCalculatorIcon(display, centerX - 17, topY, color);
    } else {
        drawGraphingIcon(display, centerX - 19, topY, color);
    }
}

} // namespace

HomeScreen::HomeScreen(Display& display)
    : m_display(display)
    , m_selectedIndex(appIndex(AppId::Calculator))
{}

void HomeScreen::enter() {
    m_selectedIndex = appIndex(AppId::Calculator);
}

AppId HomeScreen::handleKey(Key key) {
    switch (key) {
        case Key::CURSOR_LEFT:
            moveSelection(-1, 0);
            break;
        case Key::CURSOR_RIGHT:
            moveSelection(1, 0);
            break;
        case Key::CURSOR_UP:
            moveSelection(0, -1);
            break;
        case Key::CURSOR_DOWN:
            moveSelection(0, 1);
            break;
        case Key::ENTER:
            return HOME_APPS[m_selectedIndex].id;
        default:
            break;
    }

    return AppId::Home;
}

void HomeScreen::render() {
    m_display.clear(COLOR_BG);

    m_display.drawRect(0, 0, DISPLAY_WIDTH, 22, COLOR_HEADER);
    m_display.drawText("MI-23 Home", 8, 7, Display::WHITE);
    m_display.drawText("Home", DISPLAY_WIDTH - Display::textWidth("Home") - 8, 7,
                       COLOR_MUTED);

    renderContentArea(22, DISPLAY_HEIGHT - 22);
    m_display.present();
}

void HomeScreen::renderContent(int contentY, int contentHeight) {
    renderContentArea(contentY, contentHeight);
    m_display.present();
}

void HomeScreen::renderContentArea(int contentY, int contentHeight) {
    m_display.drawRect(0, contentY, DISPLAY_WIDTH, contentHeight, COLOR_BG);
    m_display.drawText("Select an app", 8, contentY + 6, COLOR_MUTED);

    const int gridTop = contentY + 22;

    for (int i = 0; i < appCount(); i++) {
        const int row = i / GRID_COLS;
        const int col = i % GRID_COLS;
        const int x = GRID_LEFT + col * (TILE_W + TILE_GAP);
        const int y = gridTop + row * (TILE_H + TILE_GAP);
        const bool selected = i == m_selectedIndex;

        m_display.drawRect(x, y, TILE_W, TILE_H,
                           selected ? COLOR_TILE_SELECTED : COLOR_TILE);
        drawOutline(m_display,
                    x,
                    y,
                    TILE_W,
                    TILE_H,
                    selected ? COLOR_FOCUS : COLOR_TILE_BORDER,
                    selected ? 3 : 1);

        const uint16_t iconColor = selected ? Display::WHITE : COLOR_MUTED;
        drawIcon(m_display, HOME_APPS[i], x + TILE_W / 2, y + 11, iconColor);
        drawCenteredText(m_display,
                         HOME_APPS[i].label,
                         x,
                         y + TILE_H - FONT_CHAR_HEIGHT - 9,
                         TILE_W,
                         selected ? Display::WHITE : COLOR_MUTED);
    }

    m_display.drawText("Arrows move   Enter open", 8, contentY + contentHeight - 20,
                       COLOR_MUTED);
    m_display.drawText("Home returns here", 8, contentY + contentHeight - 10,
                       COLOR_MUTED);
}

void HomeScreen::moveSelection(int deltaCol, int deltaRow) {
    const int count = appCount();
    if (count <= 0) {
        m_selectedIndex = 0;
        return;
    }

    const int rowCount = (count + GRID_COLS - 1) / GRID_COLS;
    const int currentRow = m_selectedIndex / GRID_COLS;
    const int currentCol = m_selectedIndex % GRID_COLS;
    const int nextRow = std::clamp(currentRow + deltaRow, 0, rowCount - 1);
    const int lastColInRow = std::min(GRID_COLS - 1,
                                      count - nextRow * GRID_COLS - 1);
    const int nextCol = std::clamp(currentCol + deltaCol, 0, lastColInRow);

    m_selectedIndex = nextRow * GRID_COLS + nextCol;
}
