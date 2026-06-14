//
// Created by Miracle Aigbogun on 3/22/26.
//

#include "pico/stdlib.h"
#include "display_rp2350.h"
#include "app/home/calculator_home.h"
#include "app/calculator/calculator_app.h"
#include "app/graphing/graph_app.h"
#include "app/settings/settings_app.h"
#include "app/settings/settings_state.h"
#include "platform/rp2350/keypad_rp2350.h"
#include "keypad_rp2350_2.h"
#include <cstdio>

namespace {

static constexpr int SCREEN_W = 320;
static constexpr int SCREEN_H = 240;
static constexpr int HEADER_HEIGHT = 22;
static constexpr int CONTENT_Y = HEADER_HEIGHT;
static constexpr int CONTENT_H = SCREEN_H - HEADER_HEIGHT;

const uint16_t COLOR_HEADER_BG = Display::rgb(22, 35, 48);
const uint16_t COLOR_HEADER_TEXT = Display::WHITE;
const uint16_t COLOR_HEADER_MUTED = Display::rgb(150, 160, 172);
const uint16_t COLOR_HOME_BG = Display::rgb(8, 10, 14);

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
        : m_primary(primary), m_secondary(secondary) {}

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
    snprintf(label, sizeof(label), "%d%%", level);

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

void drawGlobalHeader(DisplayRP2350& display, AppId app) {
    display.fillRect(0, 0, SCREEN_W, HEADER_HEIGHT, COLOR_HEADER_BG);
    display.drawText(appTitle(app), 8, 7, COLOR_HEADER_TEXT);
    display.drawText("DEG", SCREEN_W / 2 - Display::textWidth("DEG") / 2, 7,
                     COLOR_HEADER_MUTED);
    drawBatteryIndicator(display);
}

void renderHome(DisplayRP2350& display, HomeScreen& home) {
    display.clear(Display::BLACK);
    drawGlobalHeader(display, AppId::Home);
    display.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_HOME_BG);
    home.renderContent(CONTENT_Y, CONTENT_H);
}

void renderGraphingApp(DisplayRP2350& display, GraphApp& graphApp) {
    display.clear(Display::BLACK);
    drawGlobalHeader(display, AppId::Graphing);
    display.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_HOME_BG);
    graphApp.renderContent(display, 0, CONTENT_Y, SCREEN_W, CONTENT_H);
    display.present();
}

void renderSettingsApp(DisplayRP2350& display, SettingsApp& settingsApp) {
    display.clear(Display::BLACK);
    drawGlobalHeader(display, AppId::Settings);
    display.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_HOME_BG);
    settingsApp.renderContent(0, CONTENT_Y, SCREEN_W, CONTENT_H);
    display.present();
}

class RP2350AppController {
public:
    RP2350AppController(DisplayRP2350& display,
                        Keypad& keypad,
                        HomeScreen& home,
                        CalculatorApp& calculator,
                        SettingsApp& settingsApp,
                        SettingsState& settings)
        : m_display(display)
        , m_keypad(keypad)
        , m_home(home)
        , m_calculator(calculator)
        , m_settingsApp(settingsApp)
        , m_settings(settings)
        , m_graph(&m_settings)
        , m_activeApp(AppId::Home)
        , m_waitingForRelease(false) {}

    void init() {
        m_display.init();
        m_keypad.init();
        m_home.enter();
        renderHome(m_display, m_home);
    }

    void tick() {
        const Key raw = m_keypad.getKey();
        Key pressed = Key::NONE;
        if (raw == Key::NONE) {
            m_waitingForRelease = false;
        } else if (!m_waitingForRelease) {
            pressed = raw;
            m_waitingForRelease = true;
        }

        if (pressed == Key::HOME) {
            goHome();
            return;
        }

        if (m_activeApp == AppId::Home) {
            const AppId launchTarget = m_home.handleKey(pressed);
            if (launchTarget != AppId::Home) {
                launch(launchTarget);
            } else if (pressed != Key::NONE) {
                renderHome(m_display, m_home);
            }
            return;
        }

        if (m_activeApp == AppId::Calculator) {
            m_calculator.handleKey(pressed);
            m_calculator.render();
            return;
        }

        if (m_activeApp == AppId::Graphing) {
            m_graph.handleKey(pressed);
            if (m_graph.needsRender()) {
                renderGraphingApp(m_display, m_graph);
            }
            return;
        }

        if (m_activeApp == AppId::Settings) {
            if (m_settingsApp.handleKey(pressed)) {
                goHome();
                return;
            }
            if (m_settingsApp.needsRender()) {
                renderSettingsApp(m_display, m_settingsApp);
            }
            return;
        }
    }

private:
    DisplayRP2350& m_display;
    Keypad& m_keypad;
    HomeScreen& m_home;
    CalculatorApp& m_calculator;
    SettingsApp& m_settingsApp;
    SettingsState& m_settings;
    GraphApp m_graph;
    AppId m_activeApp;
    bool m_waitingForRelease;

    void goHome() {
        if (m_activeApp != AppId::Home) {
            m_home.enter();
            m_activeApp = AppId::Home;
            renderHome(m_display, m_home);
        }
    }

    void launch(AppId app) {
        if (app == AppId::Calculator) {
            m_activeApp = AppId::Calculator;
            m_calculator.requestRender();
            m_calculator.render();
        } else if (app == AppId::Graphing) {
            m_activeApp = AppId::Graphing;
            m_graph.enter();
            renderGraphingApp(m_display, m_graph);
        } else if (app == AppId::Settings) {
            m_activeApp = AppId::Settings;
            m_settingsApp.enter();
            renderSettingsApp(m_display, m_settingsApp);
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
    HomeScreen home(display);
    CalculatorApp calculator(display, keypad, rpCalculatorConfig(settings));
    SettingsApp settingsApp(display, settings, "Hardware");

    RP2350AppController app(display, keypad, home, calculator, settingsApp, settings);
    app.init();

    while (true) {
        app.tick();
        sleep_ms(16);
    }
}
