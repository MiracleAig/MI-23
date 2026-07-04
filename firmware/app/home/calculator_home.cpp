//
// Created by miracleaigbogun on 4/19/26.
//

#include "app/home/calculator_home.h"
#include "graphics/font.h"
#include <algorithm>
#include <cstdio>

namespace {

enum class HomeIcon {
    Calculator,
    Graphing,
    Settings,
};

struct HomeAppDefinition {
    AppId id;
    const char* label;
    HomeIcon icon;
};

constexpr HomeAppDefinition HOME_APPS[] = {
    { AppId::Calculator, "Calculator", HomeIcon::Calculator },
    { AppId::Graphing,   "Graphing",   HomeIcon::Graphing   },
    { AppId::Settings,   "Settings",   HomeIcon::Settings   },
};

constexpr int APP_COUNT = static_cast<int>(sizeof(HOME_APPS) / sizeof(HOME_APPS[0]));
constexpr int GRID_COLS = 3;
constexpr int TILE_W = 82;
constexpr int TILE_H = 72;
constexpr int TILE_GAP = 14;
constexpr int GRID_LEFT = (DISPLAY_WIDTH - (GRID_COLS * TILE_W + (GRID_COLS - 1) * TILE_GAP)) / 2;
constexpr int DEFAULT_CONTENT_Y = 22;
constexpr int DEFAULT_CONTENT_HEIGHT = DISPLAY_HEIGHT - DEFAULT_CONTENT_Y;

const uint16_t COLOR_BG = Display::rgb(8, 10, 14);
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

DisplayRect tileRectForIndex(int contentY, int index) {
    const int row = index / GRID_COLS;
    const int col = index % GRID_COLS;
    const int gridTop = contentY + 22;
    return {
        GRID_LEFT + col * (TILE_W + TILE_GAP),
        gridTop + row * (TILE_H + TILE_GAP),
        TILE_W,
        TILE_H
    };
}

void drawOutline(Display& display,
                 int x,
                 int y,
                 int w,
                 int h,
                 uint16_t color,
                 int thickness = 1) {
    display.fillRect(x, y, w, thickness, color);
    display.fillRect(x, y + h - thickness, w, thickness, color);
    display.fillRect(x, y, thickness, h, color);
    display.fillRect(x + w - thickness, y, thickness, h, color);
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
    display.fillRect(x + 5, y + 5, 24, 7, color);

    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            display.fillRect(x + 6 + col * 8,
                             y + 16 + row * 5,
                             4,
                             3,
                             color);
        }
    }
}

void drawGraphingIcon(Display& display, int x, int y, uint16_t color) {
    drawOutline(display, x, y, 38, 32, color, 2);
    display.fillRect(x + 7, y + 6, 2, 20, color);
    display.fillRect(x + 7, y + 24, 24, 2, color);

    display.fillRect(x + 11, y + 20, 3, 3, color);
    display.fillRect(x + 15, y + 17, 3, 3, color);
    display.fillRect(x + 19, y + 13, 3, 3, color);
    display.fillRect(x + 23, y + 10, 3, 3, color);
    display.fillRect(x + 27, y + 12, 3, 3, color);
}

void drawSettingsIcon(Display& display, int x, int y, uint16_t color) {
    drawOutline(display, x + 4, y, 30, 30, color, 2);
    display.fillRect(x + 10, y + 7, 18, 2, color);
    display.fillRect(x + 10, y + 14, 18, 2, color);
    display.fillRect(x + 10, y + 21, 18, 2, color);
    display.fillRect(x + 14, y + 5, 4, 6, color);
    display.fillRect(x + 23, y + 12, 4, 6, color);
    display.fillRect(x + 17, y + 19, 4, 6, color);
}

void drawIcon(Display& display,
              const HomeAppDefinition& app,
              int centerX,
              int topY,
              uint16_t color) {
    if (app.icon == HomeIcon::Calculator) {
        drawCalculatorIcon(display, centerX - 17, topY, color);
    } else if (app.icon == HomeIcon::Graphing) {
        drawGraphingIcon(display, centerX - 19, topY, color);
    } else {
        drawSettingsIcon(display, centerX - 19, topY, color);
    }
}

} // namespace

HomeScreen::HomeScreen(Display& display)
    : m_display(display)
    , m_selectedIndex(appIndex(AppId::Calculator))
    , m_contentY(DEFAULT_CONTENT_Y)
    , m_contentHeight(DEFAULT_CONTENT_HEIGHT)
    , m_needsRender(true)
{}

void HomeScreen::enter() {
    m_selectedIndex = appIndex(AppId::Calculator);
    m_contentY = DEFAULT_CONTENT_Y;
    m_contentHeight = DEFAULT_CONTENT_HEIGHT;
    invalidateContent(m_contentY, m_contentHeight);
    m_needsRender = true;
}

AppId HomeScreen::handleKey(Key key) {
    const int oldSelectedIndex = m_selectedIndex;

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

    if (oldSelectedIndex != m_selectedIndex) {
        invalidateSelectionChange(oldSelectedIndex, m_selectedIndex);
        m_needsRender = true;
        std::printf("[render] home selection %d -> %d redraw=tiles\n",
                    oldSelectedIndex,
                    m_selectedIndex);
    } else if (key != Key::NONE) {
        std::printf("[render] home selection unchanged=%d key=%d redraw=0\n",
                    m_selectedIndex,
                    static_cast<int>(key));
    }

    return AppId::Home;
}

void HomeScreen::render() {
    renderContent(DEFAULT_CONTENT_Y, DEFAULT_CONTENT_HEIGHT);
}

void HomeScreen::renderContent(int contentY, int contentHeight) {
    if (!m_needsRender) {
        return;
    }
    m_contentY = contentY;
    m_contentHeight = contentHeight;
    if (m_dirtyRegions.empty()) {
        invalidateRect({0, contentY, DISPLAY_WIDTH, contentHeight});
    }

    for (int i = 0; i < m_dirtyRegions.count(); ++i) {
        const DisplayRect clip = DirtyRegionList::intersect(
            m_dirtyRegions.rect(i),
            {0, contentY, DISPLAY_WIDTH, contentHeight});
        if (clip.isEmpty()) {
            continue;
        }
        m_display.setClipRect(clip);
        renderContentArea(contentY, contentHeight);
    }
    m_display.clearClipRect();
    m_dirtyRegions.clear();
    m_display.present();
    m_needsRender = false;
}

void HomeScreen::invalidateContent(int contentY, int contentHeight) {
    invalidateRect({0, contentY, DISPLAY_WIDTH, contentHeight});
}

void HomeScreen::invalidateSelectionChange(int oldIndex, int newIndex) {
    if (oldIndex < 0 || oldIndex >= appCount() ||
        newIndex < 0 || newIndex >= appCount()) {
        return;
    }

    const DisplayRect dirty = DirtyRegionList::merge(
        tileRectForIndex(m_contentY, oldIndex),
        tileRectForIndex(m_contentY, newIndex));
    invalidateRect(DirtyRegionList::intersect(
        dirty,
        {0, m_contentY, DISPLAY_WIDTH, m_contentHeight}));
}

void HomeScreen::renderContentArea(int contentY, int contentHeight) {
    m_display.fillRect(0, contentY, DISPLAY_WIDTH, contentHeight, COLOR_BG);
    m_display.drawText("Select an app", 8, contentY + 6, COLOR_MUTED);

    for (int i = 0; i < appCount(); i++) {
        const DisplayRect tile = tileRectForIndex(contentY, i);
        const int x = tile.x;
        const int y = tile.y;
        const bool selected = i == m_selectedIndex;

        m_display.fillRect(x, y, TILE_W, TILE_H,
                           selected ? COLOR_TILE_SELECTED : COLOR_TILE);
        drawOutline(m_display,
                    x,
                    y,
                    TILE_W,
                    TILE_H,
                    selected ? COLOR_FOCUS : COLOR_TILE_BORDER,
                    selected ? 3 : 1);

        const uint16_t iconColor = selected ? Display::WHITE.rgb565() : COLOR_MUTED;
        drawIcon(m_display, HOME_APPS[i], x + TILE_W / 2, y + 11, iconColor);
        drawCenteredText(m_display,
                         HOME_APPS[i].label,
                         x,
                         y + TILE_H - FONT_CHAR_HEIGHT - 9,
                         TILE_W,
                         selected ? Display::WHITE.rgb565() : COLOR_MUTED);
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

bool HomeScreen::needsRender() const {
    return m_needsRender;
}

void HomeScreen::invalidateRect(DisplayRect rect) {
    m_dirtyRegions.add(rect);
}
