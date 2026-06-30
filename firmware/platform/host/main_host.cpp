//
// Created by Miracle Aigbogun on 3/10/26.
//

#include "platform/host/display_sdl.h"
#include "platform/host/keypad_host.h"
#include "platform/host/simulator_keypad.h"
#include "platform/host/settings_store_host.h"
#include "platform/host/startup_host.h"
#include "app/boot/boot_manager.h"
#include "app/home/calculator_home.h"
#include "app/calculator/calculator_app.h"
#include "app/graphing/graph_app.h"
#include "app/settings/settings_app.h"
#include "app/settings/settings_state.h"
#include "hal/system_time.h"
#include <SDL2/SDL.h>
#include <cstdio>

namespace {
CalculatorAppConfig hostCalculatorConfig(const SettingsState& settings) {
    CalculatorAppConfig config;
    config.showOnScreenKeypad = false;
    config.settings = &settings;
    return config;
}

const uint16_t COLOR_BG = Display::rgb(8, 10, 14);
const uint16_t COLOR_HEADER = Display::rgb(22, 35, 48);
const uint16_t COLOR_MUTED = Display::rgb(150, 160, 172);

void drawHeader(Display& display, const char* title) {
    display.fillRect(0, 0, DISPLAY_WIDTH, 22, COLOR_HEADER);
    display.drawText(title, 8, 7, Display::WHITE);
    display.drawText("Home", DISPLAY_WIDTH - Display::textWidth("Home") - 8, 7,
                     COLOR_MUTED);
}

void drawHomeHeader(Display& display) {
    display.fillRect(0, 0, DISPLAY_WIDTH, 22, COLOR_HEADER);
    display.drawText("MI-23 Home", 8, 7, Display::WHITE);
    display.drawText("Home", DISPLAY_WIDTH - Display::textWidth("Home") - 8, 7,
                     COLOR_MUTED);
}

class HostAppController {
public:
    HostAppController(DisplaySDL& display, KeypadHost& keypad)
        : m_display(display)
        , m_keypad(keypad)
        , m_settings()
        , m_settingsStore()
        , m_startup(m_keypad, m_settingsStore)
        , m_boot(display, m_settings, m_startup)
        , m_home(display)
        , m_calculator(display, keypad, hostCalculatorConfig(m_settings))
        , m_simulatorKeypad()
        , m_graph(&m_settings)
        , m_settingsApp(display, m_settings, "Simulator")
        , m_activeApp(AppId::Boot)
        , m_needsFrame(true)
        , m_shellDirty(true)
    {}

    void init() {
        m_display.init();
        if (!m_display.isReady()) {
            m_display.setQuit();
            return;
        }
        m_boot.begin();
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
                m_needsFrame = true;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN &&
                event.button.button == SDL_BUTTON_LEFT) {
                const int logicalX = event.button.x / 2;
                const int logicalY = event.button.y / 2;
                const Key keypadKey = m_simulatorKeypad.hitTest(logicalX, logicalY);
                if (keypadKey != Key::NONE) {
                    if (m_settings.developer.inputEventLogs) {
                        printf("[input] mouse x=%d y=%d key=%d\n",
                               logicalX,
                               logicalY,
                               static_cast<int>(keypadKey));
                    }
                    dispatchKey(keypadKey);
                }
            }

            m_keypad.handleEvent(event);
        }
    }

    void update() {
        if (m_activeApp == AppId::Calculator && m_calculator.updateBlink(systemTimeMs())) {
            m_needsFrame = true;
        }
        if (m_activeApp == AppId::Home && m_home.needsRender()) {
            m_needsFrame = true;
        }

        const Key pressed = m_keypad.getKey();
        if (pressed != Key::NONE) {
            dispatchKey(pressed);
        }
    }

    void render() {
        if (!m_needsFrame) {
            SDL_Delay(16);
            return;
        }

        m_display.setPresentEnabled(false);

        if (m_activeApp == AppId::Home) {
            if (m_shellDirty) {
                drawHomeHeader(m_display);
            }
            m_home.renderContent(22, DISPLAY_HEIGHT - 22);
        } else if (m_activeApp == AppId::Boot) {
            m_boot.render();
        } else if (m_activeApp == AppId::Calculator) {
            m_calculator.render();
        } else if (m_activeApp == AppId::Graphing) {
            if (m_shellDirty) {
                drawHeader(m_display, "Graphing");
            }
            m_graph.renderContent(m_display, 0, 22, DISPLAY_WIDTH, DISPLAY_HEIGHT - 22);
        } else if (m_activeApp == AppId::Settings) {
            if (m_shellDirty) {
                drawHeader(m_display, "Settings");
            }
            m_settingsApp.renderContent(0, 22, DISPLAY_WIDTH, DISPLAY_HEIGHT - 22);
        }

        if (m_activeApp != AppId::Boot) {
            m_simulatorKeypad.render(m_display, &m_settings);
        }
        m_display.setPresentEnabled(true);
        m_display.forcePresent();
        m_needsFrame = false;
        m_shellDirty = false;

        SDL_Delay(16);
    }

private:
    DisplaySDL& m_display;
    KeypadHost& m_keypad;
    SettingsState m_settings;
    HostSettingsStore m_settingsStore;
    HostStartupBackend m_startup;
    BootManager m_boot;
    HomeScreen m_home;
    CalculatorApp m_calculator;
    SimulatorKeypad m_simulatorKeypad;
    GraphApp m_graph;
    SettingsApp m_settingsApp;
    AppId m_activeApp;
    bool m_needsFrame;
    bool m_shellDirty;

    void dispatchKey(Key key) {
        if (m_activeApp == AppId::Boot) {
            m_boot.handleKey(key);
            if (m_boot.isFinished()) {
                finishBoot();
            }
        } else if (key == Key::HOME) {
            maybePersistSettings();
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
        } else if (m_activeApp == AppId::Settings) {
            if (m_settingsApp.handleKey(key)) {
                maybePersistSettings();
                goHome();
            }
        }
        if (key != Key::NONE) {
            m_needsFrame = true;
        }
    }

    void goHome() {
        if (m_activeApp != AppId::Home) {
            m_home.enter();
            m_activeApp = AppId::Home;
            m_needsFrame = true;
            m_shellDirty = true;
        }
    }

    void finishBoot() {
        m_home.enter();
        m_activeApp = AppId::Home;
        m_needsFrame = true;
        m_shellDirty = true;
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
        if (app == AppId::Calculator || app == AppId::Graphing || app == AppId::Settings) {
            m_activeApp = app;
            m_shellDirty = true;
            if (app == AppId::Calculator) {
                m_calculator.requestRender();
                m_calculator.updateBlink(systemTimeMs());
            } else if (app == AppId::Graphing) {
                m_graph.enter();
            } else if (app == AppId::Settings) {
                m_settingsApp.enter();
            }
            m_needsFrame = true;
        }
    }

public:
    void updateBoot() {
        if (m_activeApp != AppId::Boot) {
            return;
        }

        m_boot.tick();
        if (m_boot.isFinished()) {
            finishBoot();
        }
        if (m_boot.needsRender()) {
            m_needsFrame = true;
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
        app.updateBoot();
        app.update();
        app.render();
    }

    printf("Simulator Closed.\n");
    return 0;
}
