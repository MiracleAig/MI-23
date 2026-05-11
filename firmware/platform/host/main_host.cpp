//
// Created by Miracle Aigbogun on 3/10/26.
//

#include "platform/host/display_sdl.h"
#include "platform/host/keypad_host.h"
#include "platform/host/simulator_keypad.h"
#include "app/home/calculator_home.h"
#include "app/calculator/calculator_app.h"
#include "app/graphing/graph_app.h"
#include <SDL2/SDL.h>
#include <cstdio>

namespace {
CalculatorAppConfig hostCalculatorConfig() {
    CalculatorAppConfig config;
    config.showOnScreenKeypad = false;
    return config;
}

const uint16_t COLOR_BG = Display::rgb(8, 10, 14);
const uint16_t COLOR_HEADER = Display::rgb(22, 35, 48);
const uint16_t COLOR_MUTED = Display::rgb(150, 160, 172);

void renderGraphingApp(Display& display, GraphApp& graphApp) {
    display.clear(COLOR_BG);
    display.fillRect(0, 0, DISPLAY_WIDTH, 22, COLOR_HEADER);
    display.drawText("Graphing", 8, 7, Display::WHITE);
    display.drawText("Home", DISPLAY_WIDTH - Display::textWidth("Home") - 8, 7,
                     COLOR_MUTED);

    graphApp.renderContent(display, 0, 22, DISPLAY_WIDTH, DISPLAY_HEIGHT - 22);
}

class HostAppController {
public:
    HostAppController(DisplaySDL& display, KeypadHost& keypad)
        : m_display(display)
        , m_keypad(keypad)
        , m_home(display)
        , m_calculator(display, keypad, hostCalculatorConfig())
        , m_simulatorKeypad()
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
                event.button.button == SDL_BUTTON_LEFT) {
                const int logicalX = event.button.x / 2;
                const int logicalY = event.button.y / 2;
                const Key keypadKey = m_simulatorKeypad.hitTest(logicalX, logicalY);
                if (keypadKey != Key::NONE) {
                    dispatchKey(keypadKey);
                } else if (m_activeApp == AppId::Calculator) {
                    m_calculator.handlePointerDown(logicalX, logicalY);
                }
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
        } else if (m_activeApp == AppId::Graphing) {
            m_graph.handleKey(pressed);
        }
    }

    void render() {
        m_display.setPresentEnabled(false);

        if (m_activeApp == AppId::Home) {
            m_home.render();
        } else if (m_activeApp == AppId::Calculator) {
            m_calculator.requestRender();
            m_calculator.render();
        } else if (m_activeApp == AppId::Graphing) {
            m_graph.requestRender();
            renderGraphingApp(m_display, m_graph);
        }

        m_simulatorKeypad.render(m_display);
        m_display.setPresentEnabled(true);
        m_display.forcePresent();

        SDL_Delay(16);
    }

private:
    DisplaySDL& m_display;
    KeypadHost& m_keypad;
    HomeScreen m_home;
    CalculatorApp m_calculator;
    SimulatorKeypad m_simulatorKeypad;
    GraphApp m_graph;
    AppId m_activeApp;

    void dispatchKey(Key key) {
        if (key == Key::HOME) {
            goHome();
        } else if (m_activeApp == AppId::Home) {
            const AppId launchTarget = m_home.handleKey(key);
            if (launchTarget != AppId::Home) {
                launch(launchTarget);
            }
        } else if (m_activeApp == AppId::Calculator) {
            m_calculator.handleKey(key);
        } else if (m_activeApp == AppId::Graphing) {
            m_graph.handleKey(key);
        }
    }

    void goHome() {
        if (m_activeApp != AppId::Home) {
            m_home.enter();
            m_activeApp = AppId::Home;
        }
    }

    void launch(AppId app) {
        if (app == AppId::Calculator || app == AppId::Graphing) {
            m_activeApp = app;
            if (app == AppId::Calculator) {
                m_calculator.requestRender();
            } else if (app == AppId::Graphing) {
                m_graph.enter();
            }
        }
    }
};

} // namespace

int main(int argc, char* argv[]) {
    (void) argc;
    (void) argv;

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
