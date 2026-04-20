//
// Created by Miracle Aigbogun on 3/10/26.
//

#include "platform/host/display_sdl.h"
#include "platform/host/keypad_host.h"
#include "app/home/calculator_home.h"
#include "app/calculator/calculator_app.h"
#include <SDL2/SDL.h>
#include <cstdio>

namespace {

const uint16_t COLOR_BG = Display::rgb(8, 10, 14);
const uint16_t COLOR_HEADER = Display::rgb(22, 35, 48);
const uint16_t COLOR_MUTED = Display::rgb(150, 160, 172);
const uint16_t COLOR_FOCUS = Display::rgb(255, 230, 95);

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

void renderGraphingPlaceholder(Display& display) {
    display.clear(COLOR_BG);
    display.drawRect(0, 0, DISPLAY_WIDTH, 22, COLOR_HEADER);
    display.drawText("Graphing", 8, 7, Display::WHITE);
    display.drawText("Home", DISPLAY_WIDTH - Display::textWidth("Home") - 8, 7,
                     COLOR_MUTED);

    const int iconX = DISPLAY_WIDTH / 2 - 38;
    const int iconY = 62;
    drawOutline(display, iconX, iconY, 76, 58, COLOR_FOCUS, 2);
    display.drawRect(iconX + 16, iconY + 12, 2, 34, Display::WHITE);
    display.drawRect(iconX + 16, iconY + 44, 44, 2, Display::WHITE);
    display.drawRect(iconX + 22, iconY + 36, 4, 4, Display::WHITE);
    display.drawRect(iconX + 30, iconY + 30, 4, 4, Display::WHITE);
    display.drawRect(iconX + 38, iconY + 22, 4, 4, Display::WHITE);
    display.drawRect(iconX + 46, iconY + 18, 4, 4, Display::WHITE);
    display.drawRect(iconX + 54, iconY + 24, 4, 4, Display::WHITE);

    display.drawText("Graphing", DISPLAY_WIDTH / 2 - Display::textWidth("Graphing") / 2,
                     138, Display::WHITE);
    display.drawText("Coming soon",
                     DISPLAY_WIDTH / 2 - Display::textWidth("Coming soon") / 2,
                     152, COLOR_MUTED);
    display.drawText("Press Home to return", 8, DISPLAY_HEIGHT - 12, COLOR_MUTED);
    display.present();
}

class HostAppController {
public:
    HostAppController(DisplaySDL& display, KeypadHost& keypad)
        : m_display(display)
        , m_keypad(keypad)
        , m_home(display)
        , m_calculator(display, keypad)
        , m_activeApp(AppId::Home)
    {}

    void init() {
        m_display.init();
        m_keypad.init();
        m_home.enter();
    }

    void handleEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                m_display.setQuit();
            }
            if (event.type == SDL_KEYDOWN &&
                event.key.keysym.sym == SDLK_ESCAPE) {
                m_display.setQuit();
            }
            if (event.type == SDL_MOUSEWHEEL &&
                m_activeApp == AppId::Calculator) {
                m_calculator.scrollHistory(event.wheel.y > 0 ? -1 : 1);
            }
            if (event.type == SDL_MOUSEBUTTONDOWN &&
                event.button.button == SDL_BUTTON_LEFT &&
                m_activeApp == AppId::Calculator) {
                m_calculator.handlePointerDown(event.button.x / 2,
                                               event.button.y / 2);
            }

            m_keypad.handleEvent(event);
        }
    }

    void update() {
        const Key pressed = m_keypad.getKey();

        if (pressed == Key::HOME) {
            goHome();
            return;
        }

        if (m_activeApp == AppId::Home) {
            const AppId launchTarget = m_home.handleKey(pressed);
            if (launchTarget != AppId::Home) {
                launch(launchTarget);
            }
        } else if (m_activeApp == AppId::Calculator) {
            m_calculator.handleKey(pressed);
        }
    }

    void render() {
        if (m_activeApp == AppId::Home) {
            m_home.render();
        } else if (m_activeApp == AppId::Calculator) {
            m_calculator.render();
        } else if (m_activeApp == AppId::Graphing) {
            renderGraphingPlaceholder(m_display);
        }

        SDL_Delay(16);
    }

private:
    DisplaySDL& m_display;
    KeypadHost& m_keypad;
    HomeScreen m_home;
    CalculatorApp m_calculator;
    AppId m_activeApp;

    void goHome() {
        if (m_activeApp != AppId::Home) {
            m_home.enter();
            m_activeApp = AppId::Home;
        }
    }

    void launch(AppId app) {
        if (app == AppId::Calculator || app == AppId::Graphing) {
            m_activeApp = app;
        }
    }
};

} // namespace

int main() {
    printf("Calculator Simulator Is Starting...\n");

    DisplaySDL display;
    KeypadHost  keypad;

    HostAppController app(display, keypad);
    app.init();

    printf("Calculator Simulator Initialized. Press Escape To Quit.\n");

    while (!display.shouldQuit()) {
        app.handleEvents();
        app.update();
        app.render();
    }

    printf("Simulator Closed.\n");
    return 0;
}
