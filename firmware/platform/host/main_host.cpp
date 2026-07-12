//
// Created by Miracle Aigbogun on 3/10/26.
//

#include "platform/host/display_sdl.h"
#include "platform/host/companion_system_actions_host.h"
#include "platform/host/keypad_host.h"
#include "platform/host/simulator_keypad.h"
#include "platform/host/settings_store_host.h"
#include "platform/host/startup_host.h"
#include "platform/host/usb_cdc_transport_host.h"
#include "app/boot/boot_manager.h"
#include "app/home/calculator_home.h"
#include "app/calculator/calculator_app.h"
#include "app/companion/companion_link_app.h"
#include "app/files/file_browser_app.h"
#include "app/graphing/graph_app.h"
#include "app/settings/settings_app.h"
#include "app/settings/settings_state.h"
#include "app/ui/title_bar.h"
#include "core/companion/CompanionProtocol.h"
#include "hal/system_time.h"
#include "mi23_metadata.h"
#include <SDL2/SDL.h>
#include <cstdio>

namespace {

CalculatorAppConfig hostCalculatorConfig(const SettingsState& settings, AxiomFS::FileSystem* filesystem) {
    CalculatorAppConfig config;
    config.showOnScreenKeypad = false;
    config.settings = &settings;
    config.filesystem = filesystem;
    return config;
}

static constexpr int CONTENT_Y = SystemTitleBar::kHeight;
static constexpr int CONTENT_H = DISPLAY_HEIGHT - CONTENT_Y;

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
        , m_calculator(display, keypad, hostCalculatorConfig(m_settings, &m_startup.filesystem()))
        , m_files(display, &m_startup.filesystem())
        , m_simulatorKeypad()
        , m_graph(&m_settings, &m_startup.filesystem())
        , m_settingsApp(display, m_settings, "Simulator", &m_startup.filesystem())
        , m_companionTransport()
        , m_companionSystemActions()
        , m_companionProtocol(m_startup.filesystem(),
                              m_settings,
                              m_settingsStore,
                              Companion::DeviceInfo{},
                              &m_companionSystemActions)
        , m_companionSession(m_companionTransport, m_companionProtocol)
        , m_companionLink(display, m_companionSession)
        , m_activeApp(AppId::Boot)
        , m_needsFrame(true)
        , m_shellDirty(true)
        , m_titleBar()
        , m_status()
        , m_renderedStatus()
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

        if (syncStatus()) {
            m_needsFrame = true;
        }
    }

    void update() {
        const uint64_t nowMs = systemTimeMs();
        m_companionLink.tick(nowMs);

        if (m_activeApp == AppId::Calculator && m_calculator.updateBlink(nowMs)) {
            m_needsFrame = true;
        }
        if (m_activeApp == AppId::Home && m_home.needsRender()) {
            m_needsFrame = true;
        }
        if (m_activeApp == AppId::Files && m_files.needsRender()) {
            m_needsFrame = true;
        }
        if (m_activeApp == AppId::Companion && m_companionLink.needsRender()) {
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
            drawSystemTitleBar();
            m_home.renderContent(CONTENT_Y, CONTENT_H);
        } else if (m_activeApp == AppId::Boot) {
            m_boot.render();
        } else if (m_activeApp == AppId::Calculator) {
            drawSystemTitleBar();
            m_calculator.renderContent(CONTENT_Y, CONTENT_H);
        } else if (m_activeApp == AppId::Graphing) {
            drawSystemTitleBar();
            m_graph.renderContent(m_display, 0, CONTENT_Y, DISPLAY_WIDTH, CONTENT_H);
        } else if (m_activeApp == AppId::Files) {
            drawSystemTitleBar();
            m_files.renderContent(0, CONTENT_Y, DISPLAY_WIDTH, CONTENT_H);
        } else if (m_activeApp == AppId::Settings) {
            drawSystemTitleBar();
            m_settingsApp.renderContent(0, CONTENT_Y, DISPLAY_WIDTH, CONTENT_H);
        } else if (m_activeApp == AppId::Companion) {
            drawSystemTitleBar();
            m_companionLink.renderContent(0, CONTENT_Y, DISPLAY_WIDTH, CONTENT_H);
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
    FileBrowserApp m_files;
    SimulatorKeypad m_simulatorKeypad;
    GraphApp m_graph;
    SettingsApp m_settingsApp;
    HostUsbCdcTransport m_companionTransport;
    HostCompanionSystemActions m_companionSystemActions;
    Companion::CompanionProtocol m_companionProtocol;
    Companion::CompanionSession m_companionSession;
    CompanionLinkApp m_companionLink;
    AppId m_activeApp;
    bool m_needsFrame;
    bool m_shellDirty;
    SystemTitleBar m_titleBar;
    SystemStatusState m_status;
    SystemStatusState m_renderedStatus;

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
        } else if (m_activeApp == AppId::Files) {
            m_files.handleKey(key);
        } else if (m_activeApp == AppId::Settings) {
            if (m_settingsApp.handleKey(key)) {
                maybePersistSettings();
                goHome();
            }
            if (m_settingsApp.consumeCompanionLinkRequest()) {
                maybePersistSettings();
                launch(AppId::Companion);
            }
        } else if (m_activeApp == AppId::Companion) {
            if (m_companionLink.handleKey(key)) {
                goHome();
            }
        }
        if (key != Key::NONE) {
            m_needsFrame = true;
        }
        if (syncStatus()) {
            m_needsFrame = true;
        }
    }

    bool syncStatus() {
        const SystemStatusState before = m_status;
        m_status.setAppTitle(appTitleForId(m_activeApp));
        m_status.setBatteryPercentage(SystemStatusState::kDefaultBatteryPercentage);
        m_status.setAngleMode(m_settings.angleMode);
        m_status.setInputLayer(m_keypad.activeLayer());
        return before != m_status;
    }

    void drawSystemTitleBar() {
        if (m_activeApp == AppId::Boot) {
            return;
        }

        syncStatus();
        if (m_shellDirty || m_status != m_renderedStatus) {
            m_titleBar.render(m_display, m_status);
            m_renderedStatus = m_status;
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
        (void)m_calculator.loadPersistentHistory();
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
        if (app == AppId::Calculator || app == AppId::Graphing ||
            app == AppId::Files || app == AppId::Settings ||
            app == AppId::Companion) {
            m_activeApp = app;
            m_shellDirty = true;
            if (app == AppId::Calculator) {
                (void)m_calculator.loadPersistentHistory();
                m_calculator.requestRender();
                m_calculator.updateBlink(systemTimeMs());
            } else if (app == AppId::Graphing) {
                m_graph.enter();
            } else if (app == AppId::Files) {
                m_files.enter();
            } else if (app == AppId::Settings) {
                m_settingsApp.enter();
            } else if (app == AppId::Companion) {
                m_companionLink.enter(systemTimeMs());
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
