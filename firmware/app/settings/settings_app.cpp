#include "app/settings/settings_app.h"

#include "graphics/font.h"

#include <cstdio>

#ifndef MI23_ENABLE_DEVELOPER_OPTIONS
#define MI23_ENABLE_DEVELOPER_OPTIONS 0
#endif

#ifndef MI23_COMPANION_DEBUG_LOGS
#define MI23_COMPANION_DEBUG_LOGS 0
#endif

namespace {

const Color COLOR_BG = Display::rgb(8, 10, 14);
const Color COLOR_PANEL = Display::rgb(18, 24, 32);
const Color COLOR_PANEL_SELECTED = Display::rgb(31, 42, 54);
const Color COLOR_TEXT = Display::WHITE;
const Color COLOR_MUTED = Display::rgb(150, 160, 172);
const Color COLOR_FOCUS = Display::rgb(255, 230, 95);
const Color COLOR_WARN = Display::rgb(255, 180, 80);

enum class MainItem {
    AngleMode,
    GraphGrid,
    GraphAxes,
    GraphResolution,
    Theme,
    UiScale,
    About,
    ResetSettings,
    DeveloperOptions,
    CalculatorPrecision,
    StorageManager,
};

constexpr int MAIN_ITEM_COUNT = MI23_ENABLE_DEVELOPER_OPTIONS ? 11 : 10;
constexpr int DEV_TOGGLE_COUNT = 5;
constexpr int DEV_FS_ACTION_COUNT = MI23_ENABLE_DEVELOPER_OPTIONS ? 6 : 0;
constexpr int DEV_ITEM_COUNT = DEV_TOGGLE_COUNT + DEV_FS_ACTION_COUNT;
constexpr int STORAGE_ACTION_COUNT = 3;
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

MainItem mainItemAt(int index) {
    switch (index) {
        case 0: return MainItem::AngleMode;
        case 1: return MainItem::GraphGrid;
        case 2: return MainItem::GraphAxes;
        case 3: return MainItem::GraphResolution;
        case 4: return MainItem::Theme;
        case 5: return MainItem::UiScale;
        case 6: return MainItem::About;
        case 7: return MainItem::ResetSettings;
#if MI23_ENABLE_DEVELOPER_OPTIONS
        case 8: return MainItem::DeveloperOptions;
        case 9: return MainItem::CalculatorPrecision;
        case 10: return MainItem::StorageManager;
#else
        case 8: return MainItem::CalculatorPrecision;
        case 9: return MainItem::StorageManager;
#endif
        default: return MainItem::AngleMode;
    }
}

const char* mainLabel(MainItem item) {
    switch (item) {
        case MainItem::AngleMode: return "Angle Mode";
        case MainItem::GraphGrid: return "Graph Grid";
        case MainItem::GraphAxes: return "Graph Axes";
        case MainItem::GraphResolution: return "Graph Resolution";
        case MainItem::Theme: return "Theme";
        case MainItem::UiScale: return "UI Scale";
        case MainItem::About: return "About";
        case MainItem::ResetSettings: return "Reset Settings";
        case MainItem::DeveloperOptions: return "Developer Options";
        case MainItem::CalculatorPrecision: return "Calculator Precision";
        case MainItem::StorageManager: return "Storage Manager";
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
#if MI23_ENABLE_DEVELOPER_OPTIONS
        case 5: return "Companion Link";
        case 6: return "Filesystem Status";
        case 7: return "Remount Filesystem";
        case 8: return "Format Filesystem";
        case 9: return "Erase Filesystem";
        case 10: return "Run FS Check";
#endif
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

bool developerItemIsToggle(int index) {
    return index >= 0 && index < DEV_TOGGLE_COUNT;
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
                         const char* platformName,
                         AxiomFS::FileSystem* filesystem)
    : m_display(display)
    , m_settings(settings)
    , m_platformName(platformName)
    , m_filesystem(filesystem)
    , m_screen(Screen::Main)
    , m_selectedIndex(0)
    , m_developerIndex(0)
    , m_storageIndex(0)
    , m_storageMessage{}
    , m_developerMessage{}
    , m_probeResult{}
    , m_hasProbeResult(false)
    , m_dirty(false)
    , m_saveRequested(false)
    , m_companionLinkRequested(false)
    , m_needsRender(true)
    , m_contentBounds{0, 22, DISPLAY_WIDTH, DISPLAY_HEIGHT - 22} {}

void SettingsApp::enter() {
    m_screen = Screen::Main;
    m_selectedIndex = 0;
    m_developerIndex = 0;
    m_storageIndex = 0;
    setStorageMessage("");
    setDeveloperMessage("");
    m_probeResult = {};
    m_hasProbeResult = false;
    m_saveRequested = false;
    m_companionLinkRequested = false;
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

    if (m_screen == Screen::FilesystemStatus || m_screen == Screen::FilesystemCheck) {
        if (key == Key::CLEAR || key == Key::ENTER) {
            m_screen = Screen::Developer;
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

#if MI23_ENABLE_DEVELOPER_OPTIONS
    if (m_screen == Screen::DeveloperFormatConfirm) {
        if (key == Key::ENTER) {
            std::printf("[settings][dev] format filesystem confirmed\n");
            if (m_filesystem) {
                const AxiomFS::HealthResult health = AxiomFS::formatAndInitialize(*m_filesystem);
                if (health.status == AxiomFS::FilesystemStatus::Healthy) {
                    setDeveloperMessage("Format ok. Filesystem ready.");
                } else {
                    char message[96] = {};
                    std::snprintf(message,
                                  sizeof(message),
                                  "Format failed: %s",
                                  AxiomFS::filesystemStatusToString(health.status));
                    setDeveloperMessage(message);
                }
            } else {
                setDeveloperMessage("Storage backend unavailable.");
            }
            m_screen = Screen::Developer;
            invalidateContent();
        } else if (key == Key::CLEAR) {
            std::printf("[settings][dev] format filesystem canceled\n");
            m_screen = Screen::Developer;
            invalidateContent();
        }
        requestRender();
        return false;
    }

    if (m_screen == Screen::EraseConfirm) {
        if (key == Key::ENTER) {
            std::printf("[settings][dev] erase filesystem confirmed\n");
            if (m_filesystem) {
                const AxiomFS::Status status = m_filesystem->eraseStorageRegion();
                if (status == AxiomFS::Status::Ok) {
                    setDeveloperMessage("Erase ok. Reboot now.");
                } else {
                    char message[96] = {};
                    std::snprintf(message,
                                  sizeof(message),
                                  "Erase failed: %s",
                                  AxiomFS::statusToString(status));
                    setDeveloperMessage(message);
                }
            } else {
                setDeveloperMessage("Storage backend unavailable.");
            }
            m_screen = Screen::Developer;
            invalidateContent();
        } else if (key == Key::CLEAR) {
            std::printf("[settings][dev] erase filesystem canceled\n");
            m_screen = Screen::Developer;
            invalidateContent();
        }
        requestRender();
        return false;
    }
#endif

    if (m_screen == Screen::FormatConfirm) {
        if (key == Key::ENTER) {
            if (m_filesystem) {
                const AxiomFS::HealthResult health = AxiomFS::formatAndInitialize(*m_filesystem);
                if (health.status == AxiomFS::FilesystemStatus::Healthy) {
                    setStorageMessage("Storage formatted and ready.");
                } else {
                    char message[96] = {};
                    std::snprintf(message,
                                  sizeof(message),
                                  "Format failed: %s",
                                  AxiomFS::filesystemStatusToString(health.status));
                    setStorageMessage(message);
                }
            } else {
                setStorageMessage("Storage backend unavailable.");
            }
            m_screen = Screen::Storage;
            invalidateContent();
        } else if (key == Key::CLEAR) {
            m_screen = Screen::Storage;
            invalidateContent();
        }
        requestRender();
        return false;
    }

    if (m_screen == Screen::Storage) {
        const int oldStorageIndex = m_storageIndex;
        if (key == Key::CLEAR) {
            m_screen = Screen::Main;
            invalidateContent();
        } else if (key == Key::CURSOR_UP) {
            m_storageIndex = (m_storageIndex + STORAGE_ACTION_COUNT - 1) % STORAGE_ACTION_COUNT;
        } else if (key == Key::CURSOR_DOWN) {
            m_storageIndex = (m_storageIndex + 1) % STORAGE_ACTION_COUNT;
        } else if (key == Key::ENTER) {
            runStorageAction();
        }
        if (oldStorageIndex != m_storageIndex) {
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
            if (developerItemIsToggle(m_developerIndex)) {
                toggleDeveloperSelected();
            } else if (key == Key::ENTER) {
                runDeveloperAction();
            }
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
            if (mainItemAt(m_selectedIndex) == MainItem::About) {
                m_screen = Screen::About;
            } else if (mainItemAt(m_selectedIndex) == MainItem::ResetSettings) {
                m_screen = Screen::ResetConfirm;
#if MI23_ENABLE_DEVELOPER_OPTIONS
            } else if (mainItemAt(m_selectedIndex) == MainItem::DeveloperOptions) {
                m_screen = Screen::Developer;
#endif
            } else if (mainItemAt(m_selectedIndex) == MainItem::StorageManager) {
                m_screen = Screen::Storage;
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
        } else if (m_screen == Screen::FilesystemStatus) {
            renderFilesystemStatus(x, y, w, h);
        } else if (m_screen == Screen::FilesystemCheck) {
            renderFilesystemCheck(x, y, w, h);
        } else if (m_screen == Screen::ResetConfirm) {
            renderResetConfirm(x, y, w, h);
        } else if (m_screen == Screen::Storage) {
            renderStorage(x, y, w, h);
        } else if (m_screen == Screen::FormatConfirm) {
            renderFormatConfirm(x, y, w, h);
        } else if (m_screen == Screen::DeveloperFormatConfirm) {
            renderDeveloperFormatConfirm(x, y, w, h);
        } else if (m_screen == Screen::EraseConfirm) {
            renderEraseConfirm(x, y, w, h);
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

bool SettingsApp::consumeCompanionLinkRequest() {
    const bool requested = m_companionLinkRequested;
    if (requested) {
        m_companionLinkRequested = false;
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

        const MainItem item = mainItemAt(index);
        char value[32] = {};
        switch (item) {
            case MainItem::AngleMode: std::snprintf(value, sizeof(value), "%s", angleModeLabel(m_settings.angleMode)); break;
            case MainItem::GraphGrid: std::snprintf(value, sizeof(value), "%s", onOffLabel(m_settings.graphGrid)); break;
            case MainItem::GraphAxes: std::snprintf(value, sizeof(value), "%s", onOffLabel(m_settings.graphAxes)); break;
            case MainItem::GraphResolution: std::snprintf(value, sizeof(value), "%s", graphResolutionLabel(m_settings.graphResolution)); break;
            case MainItem::Theme: std::snprintf(value, sizeof(value), "%s", themeModeLabel(m_settings.theme)); break;
            case MainItem::UiScale: std::snprintf(value, sizeof(value), "%s", uiScaleModeLabel(m_settings.uiScale)); break;
            case MainItem::About:
            case MainItem::ResetSettings:
            case MainItem::DeveloperOptions:
            case MainItem::StorageManager:
                std::snprintf(value, sizeof(value), "Open");
                break;
            case MainItem::CalculatorPrecision:
                std::snprintf(value, sizeof(value), "%d dp", m_settings.calculatorPrecision);
                break;
            default: break;
        }

        drawTextFit(m_display, mainLabel(item), x + 14, rowY, w / 2, selected ? COLOR_TEXT : COLOR_MUTED);
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
    m_display.drawText(AxiomFS::releaseLabel(), x + 12, y + 102, COLOR_WARN);

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
        const bool destructive = i == 8 || i == 9;
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
                    destructive ? COLOR_WARN : (selected ? COLOR_TEXT : COLOR_MUTED));
        const char* value = developerItemIsToggle(i)
            ? onOffLabel(developerValue(m_settings.developer, i))
            : "Open";
        m_display.drawText(value,
                           x + w - Display::textWidth(value) - 12,
                           rowY,
                           selected ? COLOR_TEXT : COLOR_MUTED);
    }

    if (m_developerMessage[0] != '\0') {
        drawTextFit(m_display, m_developerMessage, x + 12, y + h - 28, w - 24, COLOR_WARN);
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

void SettingsApp::renderStorage(int x, int y, int w, int h) {
    m_display.fillRect(x, y, w, h, COLOR_BG);
    m_display.drawText("Storage Manager", x + 8, y + 8, COLOR_TEXT);

    const AxiomFS::HealthResult& health = AxiomFS::getLastHealthResult();
    AxiomFS::StorageStats stats;
    if (m_filesystem) {
        stats = AxiomFS::getStorageStats(*m_filesystem);
    }

    char line[64] = {};
    const AxiomFS::FilesystemStatus displayStatus = m_filesystem
        ? health.status
        : AxiomFS::FilesystemStatus::NotMounted;
    std::snprintf(line, sizeof(line), "Status: %s", AxiomFS::filesystemStatusToString(displayStatus));
    m_display.drawText(line, x + 12, y + 30, COLOR_TEXT);
    std::snprintf(line, sizeof(line), "Backend: %s", m_filesystem ? m_filesystem->backendName() : "None");
    m_display.drawText(line, x + 12, y + 44, COLOR_MUTED);

    if (stats.queryStatus == AxiomFS::Status::Ok) {
        std::snprintf(line, sizeof(line), "Total: %llu B", static_cast<unsigned long long>(stats.totalBytes));
        m_display.drawText(line, x + 12, y + 58, COLOR_MUTED);
        std::snprintf(line, sizeof(line), "Used: %llu B", static_cast<unsigned long long>(stats.usedBytes));
        m_display.drawText(line, x + 12, y + 72, COLOR_MUTED);
        std::snprintf(line, sizeof(line), "Free: %llu B", static_cast<unsigned long long>(stats.freeBytes));
        m_display.drawText(line, x + 12, y + 86, COLOR_MUTED);
        if (stats.fileCount >= 0) {
            std::snprintf(line, sizeof(line), "Files: %d", stats.fileCount);
            m_display.drawText(line, x + 12, y + 100, COLOR_MUTED);
        }
    } else {
        std::snprintf(line, sizeof(line), "Space: %s", AxiomFS::statusToString(stats.queryStatus));
        m_display.drawText(line, x + 12, y + 58, COLOR_WARN);
    }
    if (m_storageMessage[0] != '\0') {
        drawTextFit(m_display, m_storageMessage, x + 12, y + 108, w - 24, COLOR_WARN);
    } else if (!health.detail.empty()) {
        drawTextFit(m_display, health.detail.c_str(), x + 12, y + 108, w - 24, COLOR_MUTED);
    }

    const char* actions[] = {"Remount filesystem", "Run filesystem check", "Format storage"};
    const int actionY = y + 126;
    for (int i = 0; i < STORAGE_ACTION_COUNT; ++i) {
        const int rowY = actionY + i * 18;
        const bool selected = i == m_storageIndex;
        m_display.fillRect(x + 6, rowY - 3, w - 12, 17,
                           selected ? COLOR_PANEL_SELECTED : COLOR_PANEL);
        if (selected) {
            m_display.fillRect(x + 6, rowY - 3, 3, 17, COLOR_FOCUS);
        }
        m_display.drawText(actions[i], x + 14, rowY,
                           (selected || i == 2) ? (i == 2 ? COLOR_WARN : COLOR_TEXT) : COLOR_MUTED);
    }

    m_display.drawText("ENT action  CLR back", x + 8, y + h - 14, COLOR_MUTED);
}

void SettingsApp::renderFormatConfirm(int x, int y, int w, int h) {
    (void)w;
    m_display.fillRect(x, y, w, h, COLOR_BG);
    m_display.drawText("Format Storage", x + 8, y + 8, COLOR_TEXT);
    m_display.drawText("Erase all AxiomFS files?", x + 20, y + 52, COLOR_WARN);
    m_display.drawText("ENT = Confirm format", x + 20, y + 78, COLOR_TEXT);
    m_display.drawText("CLR = Cancel", x + 20, y + 92, COLOR_MUTED);
}

void SettingsApp::renderFilesystemStatus(int x, int y, int w, int h) {
    m_display.fillRect(x, y, w, h, COLOR_BG);
    m_display.drawText("Filesystem Status", x + 8, y + 8, COLOR_TEXT);

    AxiomFS::Diagnostics diagnostics;
    if (m_filesystem) {
        diagnostics = m_filesystem->getDiagnostics();
    } else {
        diagnostics.status = AxiomFS::FilesystemStatus::NotMounted;
        diagnostics.mountStatus = AxiomFS::Status::Unsupported;
    }

    char line[80] = {};
    int lineY = y + 30;
    constexpr int kLineStep = 14;

    std::snprintf(line, sizeof(line), "Backend: %s", diagnostics.backendName);
    drawTextFit(m_display, line, x + 12, lineY, w - 24, COLOR_TEXT);
    lineY += kLineStep;
    std::snprintf(line, sizeof(line), "Mounted: %s", diagnostics.mounted ? "yes" : "no");
    m_display.drawText(line, x + 12, lineY, COLOR_MUTED);
    lineY += kLineStep;
    std::snprintf(line, sizeof(line), "State: %s", AxiomFS::filesystemStatusToString(diagnostics.status));
    drawTextFit(m_display, line, x + 12, lineY, w - 24, COLOR_MUTED);
    lineY += kLineStep;
    std::snprintf(line, sizeof(line), "Mount: %s", AxiomFS::statusToString(diagnostics.mountStatus));
    m_display.drawText(line, x + 12, lineY, COLOR_MUTED);
    lineY += kLineStep;
    std::snprintf(line,
                  sizeof(line),
                  "Reason: %s",
                  AxiomFS::mountFailureReasonToString(diagnostics.mountFailureReason));
    drawTextFit(m_display, line, x + 12, lineY, w - 24, COLOR_MUTED);
    lineY += kLineStep;

    if (diagnostics.geometryKnown) {
        std::snprintf(line, sizeof(line), "Flash: %lu B", static_cast<unsigned long>(diagnostics.flashSize));
        m_display.drawText(line, x + 12, lineY, COLOR_MUTED);
        lineY += kLineStep;
        std::snprintf(line, sizeof(line), "fs_offset: %lu", static_cast<unsigned long>(diagnostics.fsOffset));
        m_display.drawText(line, x + 12, lineY, COLOR_MUTED);
        lineY += kLineStep;
        std::snprintf(line, sizeof(line), "fs_size: %lu B", static_cast<unsigned long>(diagnostics.fsSize));
        m_display.drawText(line, x + 12, lineY, COLOR_MUTED);
        lineY += kLineStep;
        std::snprintf(line, sizeof(line), "Block: %lu B", static_cast<unsigned long>(diagnostics.blockSize));
        m_display.drawText(line, x + 12, lineY, COLOR_MUTED);
        lineY += kLineStep;
    } else {
        m_display.drawText("Geometry: unavailable", x + 12, lineY, COLOR_MUTED);
        lineY += kLineStep;
    }

    if (diagnostics.spaceKnown) {
        std::snprintf(line, sizeof(line), "Used: %llu B",
                      static_cast<unsigned long long>(diagnostics.usedBytes));
        m_display.drawText(line, x + 12, lineY, COLOR_MUTED);
        lineY += kLineStep;
        std::snprintf(line, sizeof(line), "Free: %llu B",
                      static_cast<unsigned long long>(diagnostics.freeBytes));
        m_display.drawText(line, x + 12, lineY, COLOR_MUTED);
    } else {
        m_display.drawText("Space: unavailable", x + 12, lineY, COLOR_MUTED);
    }

    m_display.drawText("ENT/CLR return", x + 8, y + h - 14, COLOR_MUTED);
}

void SettingsApp::renderFilesystemCheck(int x, int y, int w, int h) {
    m_display.fillRect(x, y, w, h, COLOR_BG);
    m_display.drawText("Filesystem Check", x + 8, y + 8, COLOR_TEXT);

    char line[80] = {};
    if (!m_hasProbeResult) {
        m_display.drawText("No check result.", x + 12, y + 36, COLOR_WARN);
        m_display.drawText("ENT/CLR return", x + 8, y + h - 14, COLOR_MUTED);
        return;
    }

    int lineY = y + 34;
    constexpr int kLineStep = 16;
    std::snprintf(line, sizeof(line), "Probe: %s", AxiomFS::statusToString(m_probeResult.probeStatus));
    m_display.drawText(line, x + 12, lineY, COLOR_TEXT);
    lineY += kLineStep;
    std::snprintf(line,
                  sizeof(line),
                  "Erased: %s",
                  m_probeResult.erasedKnown ? (m_probeResult.erased ? "yes" : "no") : "unknown");
    m_display.drawText(line, x + 12, lineY, COLOR_MUTED);
    lineY += kLineStep;
    std::snprintf(line,
                  sizeof(line),
                  "Magic: %s",
                  m_probeResult.magicKnown ? (m_probeResult.hasMagic ? "yes" : "no") : "unknown");
    m_display.drawText(line, x + 12, lineY, COLOR_MUTED);
    lineY += kLineStep;
    std::snprintf(line, sizeof(line), "Mount: %s", AxiomFS::statusToString(m_probeResult.mountStatus));
    m_display.drawText(line, x + 12, lineY, COLOR_MUTED);
    lineY += kLineStep;
    std::snprintf(line,
                  sizeof(line),
                  "Reason: %s",
                  AxiomFS::mountFailureReasonToString(m_probeResult.mountFailureReason));
    drawTextFit(m_display, line, x + 12, lineY, w - 24, COLOR_MUTED);
    lineY += kLineStep;
    std::snprintf(line,
                  sizeof(line),
                  "Mounted: %s -> %s",
                  m_probeResult.mountedBeforeProbe ? "yes" : "no",
                  m_probeResult.mountedAfterProbe ? "yes" : "no");
    m_display.drawText(line, x + 12, lineY, COLOR_MUTED);

    m_display.drawText("ENT/CLR return", x + 8, y + h - 14, COLOR_MUTED);
}

void SettingsApp::renderDeveloperFormatConfirm(int x, int y, int w, int h) {
    (void)w;
    m_display.fillRect(x, y, w, h, COLOR_BG);
    m_display.drawText("Format Filesystem", x + 8, y + 8, COLOR_TEXT);
    m_display.drawText("Erase all AxiomFS files?", x + 20, y + 52, COLOR_WARN);
    m_display.drawText("Then mount and restore dirs.", x + 20, y + 66, COLOR_WARN);
    m_display.drawText("ENT = Confirm format", x + 20, y + 92, COLOR_TEXT);
    m_display.drawText("CLR = Cancel", x + 20, y + 106, COLOR_MUTED);
}

void SettingsApp::renderEraseConfirm(int x, int y, int w, int h) {
    (void)w;
    m_display.fillRect(x, y, w, h, COLOR_BG);
    m_display.drawText("Erase Filesystem", x + 8, y + 8, COLOR_TEXT);
    m_display.drawText("Erase entire FS region?", x + 20, y + 46, COLOR_WARN);
    m_display.drawText("No format will run now.", x + 20, y + 60, COLOR_WARN);
    m_display.drawText("Reboot after erase.", x + 20, y + 74, COLOR_WARN);
    m_display.drawText("ENT = Confirm erase", x + 20, y + 100, COLOR_TEXT);
    m_display.drawText("CLR = Cancel", x + 20, y + 114, COLOR_MUTED);
}

void SettingsApp::cycleSelected(int direction) {
    switch (mainItemAt(m_selectedIndex)) {
        case MainItem::AngleMode:
            m_settings.angleMode = m_settings.angleMode == AngleMode::Radians
                ? AngleMode::Degrees
                : AngleMode::Radians;
            markChanged();
            break;
        case MainItem::GraphGrid:
            m_settings.graphGrid = !m_settings.graphGrid;
            markChanged();
            break;
        case MainItem::GraphAxes:
            m_settings.graphAxes = !m_settings.graphAxes;
            markChanged();
            break;
        case MainItem::GraphResolution: {
            int value = static_cast<int>(m_settings.graphResolution) + direction;
            if (value < 0) value = 2;
            if (value > 2) value = 0;
            m_settings.graphResolution = static_cast<GraphResolution>(value);
            markChanged();
            break;
        }
        case MainItem::Theme: {
            int value = static_cast<int>(m_settings.theme) + direction;
            if (value < 0) value = 2;
            if (value > 2) value = 0;
            m_settings.theme = static_cast<ThemeMode>(value);
            markChanged();
            break;
        }
        case MainItem::UiScale: {
            int value = static_cast<int>(m_settings.uiScale) + direction;
            if (value < 0) value = 2;
            if (value > 2) value = 0;
            m_settings.uiScale = static_cast<UiScaleMode>(value);
            markChanged();
            break;
        }
        case MainItem::CalculatorPrecision:
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

void SettingsApp::runDeveloperAction() {
#if MI23_ENABLE_DEVELOPER_OPTIONS
    switch (m_developerIndex) {
        case 5:
#if MI23_COMPANION_DEBUG_LOGS
            std::printf("[settings][dev] companion link requested\n");
#endif
            m_companionLinkRequested = true;
            setDeveloperMessage("");
            invalidateContent();
            return;
        case 6:
            if (!m_filesystem) {
                setDeveloperMessage("Storage backend unavailable.");
                invalidateContent();
                return;
            }
            std::printf("[settings][dev] filesystem status opened\n");
            setDeveloperMessage("");
            m_screen = Screen::FilesystemStatus;
            invalidateContent();
            return;
        case 7: {
            if (!m_filesystem) {
                setDeveloperMessage("Storage backend unavailable.");
                invalidateContent();
                return;
            }
            std::printf("[settings][dev] remount filesystem requested\n");
            const AxiomFS::Status status = m_filesystem->remount();
            char message[96] = {};
            std::snprintf(message,
                          sizeof(message),
                          "Remount: %s",
                          AxiomFS::statusToString(status));
            setDeveloperMessage(message);
            invalidateContent();
            return;
        }
        case 8:
            std::printf("[settings][dev] format filesystem prompt opened\n");
            setDeveloperMessage("");
            m_screen = Screen::DeveloperFormatConfirm;
            invalidateContent();
            return;
        case 9:
            std::printf("[settings][dev] erase filesystem prompt opened\n");
            setDeveloperMessage("");
            m_screen = Screen::EraseConfirm;
            invalidateContent();
            return;
        case 10:
            if (!m_filesystem) {
                setDeveloperMessage("Storage backend unavailable.");
                invalidateContent();
                return;
            }
            std::printf("[settings][dev] filesystem check requested\n");
            m_probeResult = m_filesystem->runProbe();
            m_hasProbeResult = true;
            m_screen = Screen::FilesystemCheck;
            invalidateContent();
            return;
        default:
            break;
    }
#else
    (void)this;
#endif
}

void SettingsApp::runStorageAction() {
    if (!m_filesystem) {
        setStorageMessage("Storage backend unavailable.");
        invalidateContent();
        return;
    }

    if (m_storageIndex == 0) {
        const AxiomFS::HealthResult& health = AxiomFS::getLastHealthResult();
        if (health.status == AxiomFS::FilesystemStatus::Unformatted ||
            health.mountFailureReason == AxiomFS::MountFailureReason::NotFormatted) {
            setStorageMessage("Storage is unformatted. Format storage first.");
            invalidateContent();
            return;
        }
        (void)m_filesystem->mount();
        const AxiomFS::HealthResult check = AxiomFS::runHealthCheck(*m_filesystem);
        setStorageMessage(check.status == AxiomFS::FilesystemStatus::Healthy
            ? "Storage remounted."
            : AxiomFS::filesystemStatusToString(check.status));
        invalidateContent();
    } else if (m_storageIndex == 1) {
        const AxiomFS::HealthResult check = AxiomFS::runHealthCheck(*m_filesystem);
        setStorageMessage(check.status == AxiomFS::FilesystemStatus::Healthy
            ? "Filesystem check passed."
            : AxiomFS::filesystemStatusToString(check.status));
        invalidateContent();
    } else if (m_storageIndex == 2) {
        setStorageMessage("");
        m_screen = Screen::FormatConfirm;
        invalidateContent();
    }
}

void SettingsApp::setStorageMessage(const char* message) {
    std::snprintf(m_storageMessage,
                  sizeof(m_storageMessage),
                  "%s",
                  message ? message : "");
}

void SettingsApp::setDeveloperMessage(const char* message) {
    std::snprintf(m_developerMessage,
                  sizeof(m_developerMessage),
                  "%s",
                  message ? message : "");
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
