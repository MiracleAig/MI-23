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
    Files,
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
    { AppId::Files,      "Files",      HomeIcon::Files      },
    { AppId::Settings,   "Settings",   HomeIcon::Settings   },
};

constexpr int APP_COUNT = static_cast<int>(sizeof(HOME_APPS) / sizeof(HOME_APPS[0]));
constexpr int HOME_COLUMNS = 4;
constexpr int HOME_HORIZONTAL_PADDING = 32;
constexpr int HOME_HEADER_HEIGHT = 18;
constexpr int HOME_TITLE_TO_GRID_GAP = 24;
constexpr int HOME_TILE_W = 88;
constexpr int HOME_TILE_H = 78;
constexpr int HOME_ROW_GAP = 22;
constexpr int HOME_ICON_TOP_PADDING = 12;
constexpr int HOME_LABEL_BOTTOM_PADDING = 10;
constexpr int HOME_LABEL_HORIZONTAL_PADDING = 8;
constexpr int HOME_BOTTOM_SAFE_AREA = 44;
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

int textLength(const char* text) {
    if (!text) {
        return 0;
    }

    int length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

struct HomeLayout {
    int columns;
    int gridLeft;
    int gridTop;
    int cellW;
    int rowStep;
    int tileW;
    int tileH;
};

HomeLayout homeLayout(int contentY, int contentHeight) {
    constexpr int availableWidth = DISPLAY_WIDTH - HOME_HORIZONTAL_PADDING * 2;
    constexpr int cellW = availableWidth / HOME_COLUMNS;
    const int rowCount = (appCount() + HOME_COLUMNS - 1) / HOME_COLUMNS;
    const int gridHeight = rowCount * HOME_TILE_H + std::max(0, rowCount - 1) * HOME_ROW_GAP;
    const int gridAreaTop = contentY + HOME_HEADER_HEIGHT + HOME_TITLE_TO_GRID_GAP;
    const int gridAreaBottom = contentY + contentHeight - HOME_BOTTOM_SAFE_AREA;
    const int gridAreaHeight = std::max(HOME_TILE_H, gridAreaBottom - gridAreaTop);
    const int gridTop = gridAreaTop + std::max(0, gridAreaHeight - gridHeight) / 2;

    return {
        HOME_COLUMNS,
        HOME_HORIZONTAL_PADDING,
        gridTop,
        cellW,
        HOME_TILE_H + HOME_ROW_GAP,
        HOME_TILE_W,
        HOME_TILE_H
    };
}

DisplayRect tileRectForIndex(int contentY, int contentHeight, int index) {
    const HomeLayout layout = homeLayout(contentY, contentHeight);
    const int row = index / layout.columns;
    const int col = index % layout.columns;
    const int remainingApps = appCount() - row * layout.columns;
    const int appsInRow = std::min(layout.columns, remainingApps);
    const int rowLeft = layout.gridLeft + (layout.columns - appsInRow) * layout.cellW / 2;
    const int tileInsetX = (layout.cellW - layout.tileW) / 2;

    return {
        rowLeft + col * layout.cellW + tileInsetX,
        layout.gridTop + row * layout.rowStep,
        layout.tileW,
        layout.tileH
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

void drawCenteredTextFit(Display& display,
                         const char* text,
                         int x,
                         int y,
                         int w,
                         uint16_t color) {
    if (!text || w <= 0) {
        return;
    }

    const int maxChars = w / FONT_CHAR_ADVANCE;
    if (maxChars <= 0) {
        return;
    }

    char clipped[24] = {};
    const int length = textLength(text);
    if (length <= maxChars) {
        drawCenteredText(display, text, x, y, w, color);
        return;
    }

    if (maxChars <= 3) {
        for (int i = 0; i < maxChars; ++i) {
            clipped[i] = '.';
        }
        clipped[maxChars] = '\0';
    } else {
        const int copyLength = std::min(maxChars - 3,
                                        static_cast<int>(sizeof(clipped)) - 4);
        for (int i = 0; i < copyLength; ++i) {
            clipped[i] = text[i];
        }
        clipped[copyLength] = '.';
        clipped[copyLength + 1] = '.';
        clipped[copyLength + 2] = '.';
        clipped[copyLength + 3] = '\0';
    }

    drawCenteredText(display, clipped, x, y, w, color);
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

void drawFilesIcon(Display& display, int x, int y, uint16_t color) {
    drawOutline(display, x, y + 6, 38, 26, color, 2);
    display.fillRect(x + 4, y + 2, 14, 6, color);
    display.fillRect(x + 4, y + 12, 28, 2, color);
    display.fillRect(x + 4, y + 19, 24, 2, color);
    display.fillRect(x + 4, y + 26, 18, 2, color);
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
    } else if (app.icon == HomeIcon::Files) {
        drawFilesIcon(display, centerX - 19, topY, color);
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
        tileRectForIndex(m_contentY, m_contentHeight, oldIndex),
        tileRectForIndex(m_contentY, m_contentHeight, newIndex));
    invalidateRect(DirtyRegionList::intersect(
        dirty,
        {0, m_contentY, DISPLAY_WIDTH, m_contentHeight}));
}

void HomeScreen::renderContentArea(int contentY, int contentHeight) {
    m_display.fillRect(0, contentY, DISPLAY_WIDTH, contentHeight, COLOR_BG);
    m_display.drawText("Select an app", 8, contentY + 6, COLOR_MUTED);

    for (int i = 0; i < appCount(); i++) {
        const DisplayRect tile = tileRectForIndex(contentY, contentHeight, i);
        const int x = tile.x;
        const int y = tile.y;
        const bool selected = i == m_selectedIndex;

        m_display.fillRect(x, y, tile.w, tile.h,
                           selected ? COLOR_TILE_SELECTED : COLOR_TILE);
        drawOutline(m_display,
                    x,
                    y,
                    tile.w,
                    tile.h,
                    selected ? COLOR_FOCUS : COLOR_TILE_BORDER,
                    selected ? 3 : 1);

        const uint16_t iconColor = selected ? Display::WHITE.rgb565() : COLOR_MUTED;
        drawIcon(m_display,
                 HOME_APPS[i],
                 x + tile.w / 2,
                 y + HOME_ICON_TOP_PADDING,
                 iconColor);
        drawCenteredTextFit(m_display,
                            HOME_APPS[i].label,
                            x + HOME_LABEL_HORIZONTAL_PADDING,
                            y + tile.h - FONT_CHAR_HEIGHT - HOME_LABEL_BOTTOM_PADDING,
                            tile.w - HOME_LABEL_HORIZONTAL_PADDING * 2,
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

    const int rowCount = (count + HOME_COLUMNS - 1) / HOME_COLUMNS;
    const int currentRow = m_selectedIndex / HOME_COLUMNS;
    const int currentCol = m_selectedIndex % HOME_COLUMNS;
    const int nextRow = std::clamp(currentRow + deltaRow, 0, rowCount - 1);
    const int lastColInRow = std::min(HOME_COLUMNS - 1,
                                      count - nextRow * HOME_COLUMNS - 1);
    const int nextCol = std::clamp(currentCol + deltaCol, 0, lastColInRow);

    m_selectedIndex = nextRow * HOME_COLUMNS + nextCol;
}

bool HomeScreen::needsRender() const {
    return m_needsRender;
}

void HomeScreen::invalidateRect(DisplayRect rect) {
    m_dirtyRegions.add(rect);
}
