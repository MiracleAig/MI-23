//
// Created by Miracle Aigbogun on 3/22/26.
//

#include "pico/stdlib.h"

#include "app/boot/boot_manager.h"
#include "app/calculator/calculator_app.h"
#include "app/graphing/graph_app.h"
#include "app/home/calculator_home.h"
#include "app/settings/settings_app.h"
#include "app/settings/settings_state.h"
#include "display_rp2350.h"
#include "hal/system_time.h"
#include "keypad_rp2350_2.h"
#include "platform/rp2350/keypad_rp2350.h"
#include "platform/rp2350/settings_store_rp2350.h"
#include "platform/rp2350/startup_rp2350.h"

#include <cstdio>

namespace {

static constexpr int SCREEN_W = DISPLAY_WIDTH;
static constexpr int SCREEN_H = DISPLAY_HEIGHT;
static constexpr int HEADER_HEIGHT = 22;
static constexpr int CONTENT_Y = HEADER_HEIGHT;
static constexpr int CONTENT_H = SCREEN_H - HEADER_HEIGHT;

const uint16_t COLOR_HEADER_BG = Display::rgb(22, 35, 48);
const uint16_t COLOR_HEADER_TEXT = Display::WHITE;
const uint16_t COLOR_HEADER_MUTED = Display::rgb(150, 160, 172);

CalculatorAppConfig rpCalculatorConfig(const SettingsState& settings) {
    CalculatorAppConfig config;
    config.showOnScreenKeypad = false;
    config.uiScale = 2;
    config.settings = &settings;
    return config;
}

class DualKeypad : public Keypad {
public:
    DualKeypad(Keypad& primary, Keypad& secondary)
        : m_primary(primary)
        , m_secondary(secondary) {}

    void init() override {
        m_primary.init();
        m_secondary.init();
    }

    Key getKey() override {
        const Key first = m_primary.getKey();
        if (first != Key::NONE) {
            return first;
        }
        return m_secondary.getKey();
    }

private:
    Keypad& m_primary;
    Keypad& m_secondary;
};

const char* appTitle(AppId app) {
    switch (app) {
        case AppId::Home: return "Home";
        case AppId::Calculator: return "Calculator";
        case AppId::Graphing: return "Graphing";
        case AppId::Settings: return "Settings";
        default: return "";
    }
}

int getBatteryLevel() {
    return 100;
}

void drawBatteryIndicator(DisplayRP2350& display) {
    const int level = getBatteryLevel();
    char label[8] = {};
    std::snprintf(label, sizeof(label), "%d%%", level);

    const int labelX = SCREEN_W - Display::textWidth(label) - 6;
    const int iconW = 24;
    const int iconH = 10;
    const int iconX = labelX - iconW - 5;
    const int iconY = (HEADER_HEIGHT - iconH) / 2;
    const int fillW = (iconW - 4) * level / 100;

    display.fillRect(iconX, iconY, iconW, 1, COLOR_HEADER_TEXT);
    display.fillRect(iconX, iconY + iconH - 1, iconW, 1, COLOR_HEADER_TEXT);
    display.fillRect(iconX, iconY, 1, iconH, COLOR_HEADER_TEXT);
    display.fillRect(iconX + iconW - 1, iconY, 1, iconH, COLOR_HEADER_TEXT);
    display.fillRect(iconX + iconW, iconY + 3, 2, 4, COLOR_HEADER_TEXT);
    display.fillRect(iconX + 2, iconY + 2, fillW, iconH - 4, Display::GREEN);
    display.drawText(label, labelX, 7, COLOR_HEADER_TEXT);
}

void drawGlobalHeader(DisplayRP2350& display, AppId app, const SettingsState& settings) {
    display.fillRect(0, 0, SCREEN_W, HEADER_HEIGHT, COLOR_HEADER_BG);
    display.drawText(appTitle(app), 8, 7, COLOR_HEADER_TEXT);
    const char* angleLabel = settings.angleMode == AngleMode::Degrees ? "DEG" : "RAD";
    display.drawText(angleLabel,
                     SCREEN_W / 2 - Display::textWidth(angleLabel) / 2,
                     7,
                     COLOR_HEADER_MUTED);
    drawBatteryIndicator(display);
}

class RP2350AppController {
public:
    RP2350AppController(DisplayRP2350& display,
                        Keypad& keypad,
                        HomeScreen& home,
                        CalculatorApp& calculator,
                        SettingsApp& settingsApp,
                        SettingsStore& settingsStore,
                        BootManager& boot,
                        SettingsState& settings)
        : m_display(display)
        , m_keypad(keypad)
        , m_home(home)
        , m_calculator(calculator)
        , m_settingsApp(settingsApp)
        , m_settingsStore(settingsStore)
        , m_boot(boot)
        , m_settings(settings)
        , m_graph(&m_settings)
        , m_activeApp(AppId::Boot)
        , m_waitingForRelease(false)
        , m_shellDirty(true)
        , m_lastHeaderAngleMode(settings.angleMode) {}

    void init() {
        m_display.init();
        m_boot.begin();
    }

    void tick() {
        if (m_activeApp == AppId::Boot) {
            m_boot.tick();
            if (m_boot.isFinished()) {
                finishBoot();
            } else if (m_boot.needsRender()) {
                m_boot.render();
            }
        }

        const Key raw = m_boot.inputReady() ? m_keypad.getKey() : Key::NONE;
        Key pressed = Key::NONE;
        if (raw == Key::NONE) {
            m_waitingForRelease = false;
        } else if (!m_waitingForRelease) {
            pressed = raw;
            m_waitingForRelease = true;
        }

        if (m_activeApp == AppId::Boot) {
            m_boot.handleKey(pressed);
            if (m_boot.isFinished()) {
                finishBoot();
            }
            return;
        }

        if (pressed == Key::HOME) {
            goHome();
            return;
        }

        if (m_activeApp == AppId::Home) {
            const AppId launchTarget = m_home.handleKey(pressed);
            if (launchTarget != AppId::Home) {
                launch(launchTarget);
            } else if (m_home.needsRender()) {
                drawCurrentShell();
                m_home.renderContent(CONTENT_Y, CONTENT_H);
            }
            return;
        }

        if (m_activeApp == AppId::Calculator) {
            m_calculator.handleKey(pressed);
            const bool blinkChanged = m_calculator.updateBlink(systemTimeMs());
            if (blinkChanged && m_settings.developer.inputEventLogs) {
                std::printf("[render] rp2350 blink toggled; render requested\n");
            }
            m_calculator.render();
            return;
        }

        if (m_activeApp == AppId::Graphing) {
            m_graph.handleKey(pressed);
            if (m_graph.needsRender()) {
                drawCurrentShell();
                m_graph.renderContent(m_display, 0, CONTENT_Y, SCREEN_W, CONTENT_H);
                m_display.present();
            }
            return;
        }

        if (m_activeApp == AppId::Settings) {
            if (m_settingsApp.handleKey(pressed)) {
                goHome();
                return;
            }
            if (m_settingsApp.needsRender()) {
                drawCurrentShell();
                m_settingsApp.renderContent(0, CONTENT_Y, SCREEN_W, CONTENT_H);
                m_display.present();
            }
        }
    }

private:
    DisplayRP2350& m_display;
    Keypad& m_keypad;
    HomeScreen& m_home;
    CalculatorApp& m_calculator;
    SettingsApp& m_settingsApp;
    SettingsStore& m_settingsStore;
    BootManager& m_boot;
    SettingsState& m_settings;
    GraphApp m_graph;
    AppId m_activeApp;
    bool m_waitingForRelease;
    bool m_shellDirty;
    AngleMode m_lastHeaderAngleMode;

    void drawCurrentShell() {
        if (m_activeApp == AppId::Calculator || m_activeApp == AppId::Boot) {
            return;
        }
        if (!m_shellDirty && m_lastHeaderAngleMode == m_settings.angleMode) {
            return;
        }

        drawGlobalHeader(m_display, m_activeApp, m_settings);
        m_lastHeaderAngleMode = m_settings.angleMode;
        m_shellDirty = false;
    }

    void finishBoot() {
        m_home.enter();
        m_activeApp = AppId::Home;
        m_shellDirty = true;
        drawCurrentShell();
        m_home.renderContent(CONTENT_Y, CONTENT_H);
    }

    void goHome() {
        if (m_activeApp != AppId::Home) {
            maybePersistSettings();
            m_home.enter();
            m_activeApp = AppId::Home;
            m_shellDirty = true;
            drawCurrentShell();
            m_home.renderContent(CONTENT_Y, CONTENT_H);
        }
    }

    void maybePersistSettings() {
        if (m_activeApp != AppId::Settings) {
            return;
        }

        if (m_settingsApp.consumeSaveRequest() || m_settingsApp.hasPendingChanges()) {
            if (m_settingsStore.save(m_settings)) {
                m_settingsApp.markSaved();
            }
        }
    }

    void launch(AppId app) {
        if (app == AppId::Calculator) {
            m_activeApp = AppId::Calculator;
            m_calculator.requestRender();
            m_calculator.updateBlink(systemTimeMs());
            m_calculator.render();
            return;
        }

        if (app == AppId::Graphing) {
            m_activeApp = AppId::Graphing;
            m_graph.enter();
            m_shellDirty = true;
            drawCurrentShell();
            m_graph.renderContent(m_display, 0, CONTENT_Y, SCREEN_W, CONTENT_H);
            m_display.present();
            return;
        }

        if (app == AppId::Settings) {
            m_activeApp = AppId::Settings;
            m_settingsApp.enter();
            m_shellDirty = true;
            drawCurrentShell();
            m_settingsApp.renderContent(0, CONTENT_Y, SCREEN_W, CONTENT_H);
            m_display.present();
        }
    }
};

} // namespace

int main() {
    stdio_init_all();

    DisplayRP2350 display;
    KeypadRP2350 keypad1;
    KeypadRP2350_2 keypad2;
    DualKeypad keypad(keypad1, keypad2);

    SettingsState settings;
    RP2350SettingsStore settingsStore;
    RP2350StartupBackend startup(keypad, settingsStore);
    HomeScreen home(display);
    CalculatorApp calculator(display, keypad, rpCalculatorConfig(settings));
    SettingsApp settingsApp(display, settings, "Hardware");
    BootManager boot(display, settings, startup);

    RP2350AppController app(display,
                            keypad,
                            home,
                            calculator,
                            settingsApp,
                            settingsStore,
                            boot,
                            settings);
    app.init();

    while (true) {
        app.tick();
        sleep_ms(16);
    }
}
