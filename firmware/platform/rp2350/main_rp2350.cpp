//
// Created by Miracle Aigbogun on 3/22/26.
//

#include "pico/stdlib.h"
#include "display_rp2350.h"
#include "app/home/calculator_home.h"
#include "app/calculator/calculator_app.h"
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

    display.drawRect(iconX, iconY, iconW, 1, COLOR_HEADER_TEXT);
    display.drawRect(iconX, iconY + iconH - 1, iconW, 1, COLOR_HEADER_TEXT);
    display.drawRect(iconX, iconY, 1, iconH, COLOR_HEADER_TEXT);
    display.drawRect(iconX + iconW - 1, iconY, 1, iconH, COLOR_HEADER_TEXT);
    display.drawRect(iconX + iconW, iconY + 3, 2, 4, COLOR_HEADER_TEXT);
    display.drawRect(iconX + 2, iconY + 2, fillW, iconH - 4, Display::GREEN);
    display.drawText(label, labelX, 7, COLOR_HEADER_TEXT);
}

void drawGlobalHeader(DisplayRP2350& display, AppId app) {
    display.drawRect(0, 0, SCREEN_W, HEADER_HEIGHT, COLOR_HEADER_BG);
    display.drawText(appTitle(app), 8, 7, COLOR_HEADER_TEXT);
    display.drawText("DEG", SCREEN_W / 2 - Display::textWidth("DEG") / 2, 7,
                     COLOR_HEADER_MUTED);
    drawBatteryIndicator(display);
}

void renderHome(DisplayRP2350& display, HomeScreen& home) {
    display.clear(Display::BLACK);
    drawGlobalHeader(display, AppId::Home);
    display.drawRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_HOME_BG);
    home.renderContent(CONTENT_Y, CONTENT_H);
}

class RP2350AppController {
public:
    RP2350AppController(DisplayRP2350& display,
                        Keypad& keypad,
                        HomeScreen& home,
                        CalculatorApp& calculator)
        : m_display(display)
        , m_keypad(keypad)
        , m_home(home)
        , m_calculator(calculator)
        , m_activeApp(AppId::Home) {}

    void init() {
        m_display.init();
        m_keypad.init();
        m_home.enter();
        renderHome(m_display, m_home);
    }

    void tick() {
        const Key pressed = m_keypad.getKey();

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

        // Graphing placeholder until graphing app exists.
        if (m_activeApp == AppId::Graphing && pressed != Key::NONE) {
            m_display.clear(Display::BLACK);
            drawGlobalHeader(m_display, AppId::Graphing);
            m_display.drawRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COLOR_HOME_BG);
            m_display.drawText("Graphing", SCREEN_W / 2 - Display::textWidth("Graphing") / 2,
                               118, Display::WHITE);
            m_display.drawText("Coming soon", SCREEN_W / 2 - Display::textWidth("Coming soon") / 2,
                               132, COLOR_HEADER_MUTED);
            m_display.drawText("Press Home to return", 8, SCREEN_H - 12, COLOR_HEADER_MUTED);
            m_display.present();
        }
    }

private:
    DisplayRP2350& m_display;
    Keypad& m_keypad;
    HomeScreen& m_home;
    CalculatorApp& m_calculator;
    AppId m_activeApp;

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
            m_calculator.render();
        } else if (app == AppId::Graphing) {
            m_activeApp = AppId::Graphing;
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

    HomeScreen home(display);
    CalculatorApp calculator(display, keypad);

    RP2350AppController app(display, keypad, home, calculator);
    app.init();

    while (true) {
        app.tick();
        sleep_ms(16);
    }
}
