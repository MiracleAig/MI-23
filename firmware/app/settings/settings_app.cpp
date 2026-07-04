#include "app/settings/settings_app.h"

#include "graphics/font.h"

#include <cstdio>

namespace {

const Color COLOR_BG = Display::rgb(8, 10, 14);
const Color COLOR_PANEL = Display::rgb(18, 24, 32);
const Color COLOR_PANEL_SELECTED = Display::rgb(31, 42, 54);
const Color COLOR_TEXT = Display::WHITE;
const Color COLOR_MUTED = Display::rgb(150, 160, 172);
const Color COLOR_FOCUS = Display::rgb(255, 230, 95);
const Color COLOR_WARN = Display::rgb(255, 180, 80);

constexpr int MAIN_ITEM_COUNT = 10;
constexpr int DEV_ITEM_COUNT = 5;
constexpr int PRECISION_VALUES[] = {3, 6, 9, 12};

const char* onOffLabel(bool value) {
    return value ? "On" : "Off";
}

void drawTextFit(Display& display,
                 const char* text,
                 int x,
                 int y,
                 int maxWidth,
                 Color color) {
    if (!text || maxWidth <= 0) {
        return;
    }

    char buffer[64] = {};
    const int maxChars = maxWidth / FONT_CHAR_ADVANCE;
    if (maxChars <= 0) {
        return;
    }

    int i = 0;
    for (; i < maxChars && i < static_cast<int>(sizeof(buffer)) - 1 && text[i] != '\0'; ++i) {
        buffer[i] = text[i];
    }
    buffer[i] = '\0';
    display.drawText(buffer, x, y, color);
}

const char* mainLabel(int index) {
    switch (index) {
        case 0: return "Angle Mode";
        case 1: return "Graph Grid";
        case 2: return "Graph Axes";
        case 3: return "Graph Resolution";
        case 4: return "Theme";
        case 5: return "UI Scale";
        case 6: return "About";
        case 7: return "Reset Settings";
        case 8: return "Developer Options";
        case 9: return "Calculator Precision";
        default: return "";
    }
}

const char* developerLabel(int index) {
    switch (index) {
        case 0: return "Show touch regions";
        case 1: return "Show cursor position";
        case 2: return "Show graph bounds";
        case 3: return "Parser logs";
        case 4: return "Input event logs";
        default: return "";
    }
}

bool developerValue(const DeveloperSettings& dev, int index) {
    switch (index) {
        case 0: return dev.showTouchRegions;
        case 1: return dev.showCursorPosition;
        case 2: return dev.showGraphBounds;
        case 3: return dev.parserLogs;
        case 4: return dev.inputEventLogs;
        default: return false;
    }
}

const char* buildTypeLabel() {
#ifdef NDEBUG
    return "Release";
#else
    return "Debug";
#endif
}

} // namespace

SettingsApp::SettingsApp(Display& display,
                         SettingsState& settings,
                         const char* platformName)
    : m_display(display)
    , m_settings(settings)
    , m_platformName(platformName)
    , m_screen(Screen::Main)
    , m_selectedIndex(0)
    , m_developerIndex(0)
    , m_dirty(false)
    , m_saveRequested(false)
    , m_needsRender(true)
    , m_contentBounds{0, 22, DISPLAY_WIDTH, DISPLAY_HEIGHT - 22} {}

void SettingsApp::enter() {
    m_screen = Screen::Main;
    m_selectedIndex = 0;
    m_developerIndex = 0;
    m_saveRequested = false;
    invalidateContent();
    requestRender();
}

bool SettingsApp::handleKey(Key key) {
    if (key == Key::NONE) {
        return false;
    }

    if (m_screen == Screen::About) {
        if (key == Key::CLEAR || key == Key::ENTER) {
            m_screen = Screen::Main;
            invalidateContent();
        }
        requestRender();
        return false;
    }

    if (m_screen == Screen::ResetConfirm) {
        if (key == Key::ENTER) {
            m_settings.resetToDefaults();
            m_screen = Screen::Main;
            invalidateContent();
        } else if (key == Key::CLEAR) {
            m_screen = Screen::Main;
            invalidateContent();
        }
        requestRender();
        return false;
    }

    if (m_screen == Screen::Developer) {
        const int oldDeveloperIndex = m_developerIndex;
        if (key == Key::CLEAR) {
            m_screen = Screen::Main;
            invalidateContent();
        } else if (key == Key::CURSOR_UP) {
            m_developerIndex = (m_developerIndex + DEV_ITEM_COUNT - 1) % DEV_ITEM_COUNT;
        } else if (key == Key::CURSOR_DOWN) {
            m_developerIndex = (m_developerIndex + 1) % DEV_ITEM_COUNT;
        } else if (key == Key::ENTER || key == Key::CURSOR_LEFT || key == Key::CURSOR_RIGHT) {
            toggleDeveloperSelected();
        }
        if (oldDeveloperIndex != m_developerIndex) {
            invalidateRect(developerRowRect(m_contentBounds.x,
                                            m_contentBounds.y,
                                            m_contentBounds.w,
                                            oldDeveloperIndex));
            invalidateRect(developerRowRect(m_contentBounds.x,
                                            m_contentBounds.y,
                                            m_contentBounds.w,
                                            m_developerIndex));
        }
        requestRender();
        return false;
    }

    const int oldSelectedIndex = m_selectedIndex;
    switch (key) {
        case Key::CLEAR:
            if (m_dirty) {
                m_saveRequested = true;
            }
            return true;
        case Key::CURSOR_UP:
            m_selectedIndex = (m_selectedIndex + MAIN_ITEM_COUNT - 1) % MAIN_ITEM_COUNT;
            break;
        case Key::CURSOR_DOWN:
            m_selectedIndex = (m_selectedIndex + 1) % MAIN_ITEM_COUNT;
            break;
        case Key::CURSOR_LEFT:
            cycleSelected(-1);
            break;
        case Key::CURSOR_RIGHT:
            cycleSelected(1);
            break;
        case Key::ENTER:
            if (m_selectedIndex == 6) {
                m_screen = Screen::About;
            } else if (m_selectedIndex == 7) {
                m_screen = Screen::ResetConfirm;
            } else if (m_selectedIndex == 8) {
                m_screen = Screen::Developer;
            } else {
                cycleSelected(1);
            }
            break;
        default:
            break;
    }

    if (oldSelectedIndex != m_selectedIndex) {
        invalidateContent();
    } else if (key == Key::CURSOR_LEFT || key == Key::CURSOR_RIGHT || key == Key::ENTER) {
        invalidateContent();
    }

    requestRender();
    return false;
}

void SettingsApp::renderContent(int x, int y, int w, int h) {
    if (m_contentBounds.x != x || m_contentBounds.y != y ||
        m_contentBounds.w != w || m_contentBounds.h != h) {
        m_contentBounds = {x, y, w, h};
        invalidateRect(m_contentBounds);
        requestRender();
    }

    if (!m_needsRender) {
        return;
    }

    if (m_dirtyRegions.empty()) {
        invalidateRect({x, y, w, h});
    }

    for (int i = 0; i < m_dirtyRegions.count(); ++i) {
        const DisplayRect clip = DirtyRegionList::intersect(m_dirtyRegions.rect(i), {x, y, w, h});
        if (clip.isEmpty()) {
            continue;
        }
        m_display.setClipRect(clip);
        if (m_screen == Screen::About) {
            renderAbout(x, y, w, h);
        } else if (m_screen == Screen::Developer) {
            renderDeveloper(x, y, w, h);
        } else if (m_screen == Screen::ResetConfirm) {
            renderResetConfirm(x, y, w, h);
        } else {
            renderMain(x, y, w, h);
        }
    }
    m_display.clearClipRect();
    m_dirtyRegions.clear();
    m_needsRender = false;
}

bool SettingsApp::hasPendingChanges() const {
    return m_dirty;
}

bool SettingsApp::consumeSaveRequest() {
    const bool requested = m_saveRequested;
    if (requested) {
        m_saveRequested = false;
    }
    return requested;
}

void SettingsApp::markSaved() {
    m_dirty = false;
    m_saveRequested = false;
}

void SettingsApp::requestRender() {
    m_needsRender = true;
}

bool SettingsApp::needsRender() const {
    return m_needsRender;
}

void SettingsApp::renderMain(int x, int y, int w, int h) {
    m_display.fillRect(x, y, w, h, COLOR_BG);
    m_display.drawText("Settings", x + 8, y + 8, COLOR_TEXT);

    const int rowHeight = 18;
    const int listY = y + 26;
    const int visibleRows = (h - 52) / rowHeight;
    int first = 0;
    if (m_selectedIndex >= visibleRows) {
        first = m_selectedIndex - visibleRows + 1;
    }

    for (int row = 0; row < visibleRows && first + row < MAIN_ITEM_COUNT; ++row) {
        const int index = first + row;
        const int rowY = listY + row * rowHeight;
        const bool selected = index == m_selectedIndex;
        m_display.fillRect(x + 6,
                           rowY - 3,
                           w - 12,
                           rowHeight,
                           selected ? COLOR_PANEL_SELECTED : COLOR_PANEL);
        if (selected) {
            m_display.fillRect(x + 6, rowY - 3, 3, rowHeight, COLOR_FOCUS);
        }

        char value[32] = {};
        switch (index) {
            case 0: std::snprintf(value, sizeof(value), "%s", angleModeLabel(m_settings.angleMode)); break;
            case 1: std::snprintf(value, sizeof(value), "%s", onOffLabel(m_settings.graphGrid)); break;
            case 2: std::snprintf(value, sizeof(value), "%s", onOffLabel(m_settings.graphAxes)); break;
            case 3: std::snprintf(value, sizeof(value), "%s", graphResolutionLabel(m_settings.graphResolution)); break;
            case 4: std::snprintf(value, sizeof(value), "%s", themeModeLabel(m_settings.theme)); break;
            case 5: std::snprintf(value, sizeof(value), "%s", uiScaleModeLabel(m_settings.uiScale)); break;
            case 6: std::snprintf(value, sizeof(value), "Open"); break;
            case 7: std::snprintf(value, sizeof(value), "Open"); break;
            case 8: std::snprintf(value, sizeof(value), "Open"); break;
            case 9: std::snprintf(value, sizeof(value), "%d dp", m_settings.calculatorPrecision); break;
            default: break;
        }

        drawTextFit(m_display, mainLabel(index), x + 14, rowY, w / 2, selected ? COLOR_TEXT : COLOR_MUTED);
        const int valueX = x + w - Display::textWidth(value) - 12;
        m_display.drawText(value, valueX, rowY, selected ? COLOR_TEXT : COLOR_MUTED);
    }

    m_display.drawText("Up/down select  Left/right change", x + 8, y + h - 22, COLOR_MUTED);
    m_display.drawText("Enter open/change  CLR back", x + 8, y + h - 12, COLOR_MUTED);
}

void SettingsApp::renderAbout(int x, int y, int w, int h) {
    (void)w;
    m_display.fillRect(x, y, w, h, COLOR_BG);
    m_display.drawText("About", x + 8, y + 8, COLOR_TEXT);
    m_display.drawText("Project: MI-23", x + 12, y + 32, COLOR_TEXT);
    m_display.drawText("App: Settings", x + 12, y + 46, COLOR_TEXT);
    m_display.drawText("Version: dev", x + 12, y + 60, COLOR_MUTED);

    char platform[48] = {};
    std::snprintf(platform, sizeof(platform), "Platform: %s", m_platformName ? m_platformName : "Unknown");
    m_display.drawText(platform, x + 12, y + 74, COLOR_MUTED);

    char build[40] = {};
    std::snprintf(build, sizeof(build), "Build: %s", buildTypeLabel());
    m_display.drawText(build, x + 12, y + 88, COLOR_MUTED);
    m_display.drawText("ENT/CLR return", x + 8, y + h - 14, COLOR_MUTED);
}

void SettingsApp::renderDeveloper(int x, int y, int w, int h) {
    m_display.fillRect(x, y, w, h, COLOR_BG);
    m_display.drawText("Developer Options", x + 8, y + 8, COLOR_TEXT);

    const int rowHeight = 20;
    const int listY = y + 30;
    for (int i = 0; i < DEV_ITEM_COUNT; ++i) {
        const int rowY = listY + i * rowHeight;
        const bool selected = i == m_developerIndex;
        m_display.fillRect(x + 6,
                           rowY - 3,
                           w - 12,
                           rowHeight,
                           selected ? COLOR_PANEL_SELECTED : COLOR_PANEL);
        if (selected) {
            m_display.fillRect(x + 6, rowY - 3, 3, rowHeight, COLOR_FOCUS);
        }
        drawTextFit(m_display,
                    developerLabel(i),
                    x + 14,
                    rowY,
                    w - 80,
                    selected ? COLOR_TEXT : COLOR_MUTED);
        const char* value = onOffLabel(developerValue(m_settings.developer, i));
        m_display.drawText(value,
                           x + w - Display::textWidth(value) - 12,
                           rowY,
                           selected ? COLOR_TEXT : COLOR_MUTED);
    }

    m_display.drawText("Enter toggles  CLR back", x + 8, y + h - 14, COLOR_MUTED);
}

void SettingsApp::renderResetConfirm(int x, int y, int w, int h) {
    (void)w;
    m_display.fillRect(x, y, w, h, COLOR_BG);
    m_display.drawText("Reset Settings", x + 8, y + 8, COLOR_TEXT);
    m_display.drawText("Reset all settings?", x + 20, y + 52, COLOR_WARN);
    m_display.drawText("ENT = Confirm", x + 20, y + 78, COLOR_TEXT);
    m_display.drawText("CLR = Cancel", x + 20, y + 92, COLOR_MUTED);
}

void SettingsApp::cycleSelected(int direction) {
    switch (m_selectedIndex) {
        case 0:
            m_settings.angleMode = m_settings.angleMode == AngleMode::Radians
                ? AngleMode::Degrees
                : AngleMode::Radians;
            markChanged();
            break;
        case 1:
            m_settings.graphGrid = !m_settings.graphGrid;
            markChanged();
            break;
        case 2:
            m_settings.graphAxes = !m_settings.graphAxes;
            markChanged();
            break;
        case 3: {
            int value = static_cast<int>(m_settings.graphResolution) + direction;
            if (value < 0) value = 2;
            if (value > 2) value = 0;
            m_settings.graphResolution = static_cast<GraphResolution>(value);
            markChanged();
            break;
        }
        case 4: {
            int value = static_cast<int>(m_settings.theme) + direction;
            if (value < 0) value = 2;
            if (value > 2) value = 0;
            m_settings.theme = static_cast<ThemeMode>(value);
            markChanged();
            break;
        }
        case 5: {
            int value = static_cast<int>(m_settings.uiScale) + direction;
            if (value < 0) value = 2;
            if (value > 2) value = 0;
            m_settings.uiScale = static_cast<UiScaleMode>(value);
            markChanged();
            break;
        }
        case 9:
            cyclePrecision(direction);
            break;
        default:
            break;
    }
}

void SettingsApp::cyclePrecision(int direction) {
    int index = 1;
    for (int i = 0; i < static_cast<int>(sizeof(PRECISION_VALUES) / sizeof(PRECISION_VALUES[0])); ++i) {
        if (PRECISION_VALUES[i] == m_settings.calculatorPrecision) {
            index = i;
            break;
        }
    }
    const int count = static_cast<int>(sizeof(PRECISION_VALUES) / sizeof(PRECISION_VALUES[0]));
    index += direction;
    while (index < 0) index += count;
    index %= count;
    m_settings.calculatorPrecision = PRECISION_VALUES[index];
    markChanged();
}

void SettingsApp::toggleDeveloperSelected() {
    switch (m_developerIndex) {
        case 0: m_settings.developer.showTouchRegions = !m_settings.developer.showTouchRegions; break;
        case 1: m_settings.developer.showCursorPosition = !m_settings.developer.showCursorPosition; break;
        case 2: m_settings.developer.showGraphBounds = !m_settings.developer.showGraphBounds; break;
        case 3: m_settings.developer.parserLogs = !m_settings.developer.parserLogs; break;
        case 4: m_settings.developer.inputEventLogs = !m_settings.developer.inputEventLogs; break;
        default: break;
    }
    markChanged();
}

void SettingsApp::markChanged() {
    m_settings.sanitize();
    m_dirty = true;
}

void SettingsApp::invalidateRect(DisplayRect rect) {
    m_dirtyRegions.add(rect);
}

void SettingsApp::invalidateContent() {
    invalidateRect(m_contentBounds);
}

DisplayRect SettingsApp::mainRowRect(int x, int y, int w, int index) const {
    constexpr int rowHeight = 18;
    const int listY = y + 26;
    const int rowY = listY + index * rowHeight;
    return {x + 6, rowY - 3, w - 12, rowHeight};
}

DisplayRect SettingsApp::developerRowRect(int x, int y, int w, int index) const {
    constexpr int rowHeight = 20;
    const int listY = y + 30;
    const int rowY = listY + index * rowHeight;
    return {x + 6, rowY - 3, w - 12, rowHeight};
}
