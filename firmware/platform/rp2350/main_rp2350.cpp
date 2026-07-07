//
// Created by Miracle Aigbogun on 3/22/26.
//

#include "pico/stdlib.h"

#include "app/boot/boot_manager.h"
#include "app/calculator/calculator_app.h"
#include "app/companion/companion_link_app.h"
#include "app/files/file_browser_app.h"
#include "app/graphing/graph_app.h"
#include "app/home/calculator_home.h"
#include "app/settings/settings_app.h"
#include "app/settings/settings_state.h"
#include "app/ui/title_bar.h"
#include "display_rp2350.h"
#include "core/companion/CompanionProtocol.h"
#include "hal/system_time.h"
#include "keypad_rp2350_2.h"
#include "platform/rp2350/axiom_fs_flash_block_device.h"
#include "platform/rp2350/axiom_fs_flash_config.h"
#include "platform/rp2350/companion_system_actions_rp2350.h"
#include "platform/rp2350/keypad_rp2350.h"
#include "platform/rp2350/settings_store_rp2350.h"
#include "platform/rp2350/startup_rp2350.h"
#include "platform/rp2350/usb_cdc_transport_rp2350.h"

#include <cstdio>

namespace {

static constexpr int SCREEN_W = DISPLAY_WIDTH;
static constexpr int SCREEN_H = DISPLAY_HEIGHT;
static constexpr int CONTENT_Y = SystemTitleBar::kHeight;
static constexpr int CONTENT_H = SCREEN_H - CONTENT_Y;

#ifndef MI23_BUILD_TARGET
#define MI23_BUILD_TARGET "rp2350"
#endif

#ifndef MI23_FIRMWARE_VERSION
#define MI23_FIRMWARE_VERSION "dev"
#endif

#ifndef MI23_HARDWARE_REVISION
#define MI23_HARDWARE_REVISION "unknown"
#endif

void waitForUsbSerialInDebugBuild() {
#ifndef NDEBUG
    sleep_ms(1800);
#endif
}

void logEarlyBootDiagnostics(AxiomFS::FileSystem& filesystem) {
    std::printf("[boot][rp2350] MI-23 firmware=%s build_target=%s\n",
                MI23_FIRMWARE_VERSION,
                MI23_BUILD_TARGET);
    std::printf("[boot][rp2350] detected platform=RP2350\n");
    std::printf("[boot][rp2350] AxiomFS backend selected=%s\n", filesystem.backendName());
    std::printf("[boot][rp2350] flash configured=%lu detected=%lu\n",
                static_cast<unsigned long>(PICO_FLASH_SIZE_BYTES),
                static_cast<unsigned long>(RP2350FlashBlockDevice::detectedFlashSize()));
    std::printf("[boot][rp2350] filesystem offset=%lu size=%lu\n",
                static_cast<unsigned long>(RP2350FlashLayout::kLittleFsOffset),
                static_cast<unsigned long>(RP2350FlashLayout::kLittleFsSize));
}

CalculatorAppConfig rpCalculatorConfig(const SettingsState& settings, AxiomFS::FileSystem* filesystem) {
    CalculatorAppConfig config;
    config.showOnScreenKeypad = false;
    config.uiScale = 2;
    config.settings = &settings;
    config.filesystem = filesystem;
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

class RP2350AppController {
public:
    RP2350AppController(DisplayRP2350& display,
                        Keypad& keypad,
                        HomeScreen& home,
                        CalculatorApp& calculator,
                        FileBrowserApp& files,
                        SettingsApp& settingsApp,
                        CompanionLinkApp& companionLink,
                        SettingsStore& settingsStore,
                        AxiomFS::FileSystem& filesystem,
                        BootManager& boot,
                        SettingsState& settings)
        : m_display(display)
        , m_keypad(keypad)
        , m_home(home)
        , m_calculator(calculator)
        , m_files(files)
        , m_settingsApp(settingsApp)
        , m_companionLink(companionLink)
        , m_settingsStore(settingsStore)
        , m_boot(boot)
        , m_settings(settings)
        , m_graph(&m_settings, &filesystem)
        , m_activeApp(AppId::Boot)
        , m_waitingForRelease(false)
        , m_shellDirty(true)
        , m_titleBar()
        , m_status()
        , m_renderedStatus() {}

    void init() {
        m_display.init();
        m_boot.begin();
    }

    void tick() {
        const uint64_t nowMs = systemTimeMs();
        m_companionLink.tick(nowMs);

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
                renderHomeContent("navigation");
            }
            return;
        }

        if (m_activeApp == AppId::Calculator) {
            m_calculator.handleKey(pressed);
            const bool blinkChanged = m_calculator.updateBlink(nowMs);
            if (blinkChanged && m_settings.developer.inputEventLogs) {
                std::printf("[render] rp2350 blink toggled; render requested\n");
            }
            drawCurrentShell();
            m_calculator.renderContent(CONTENT_Y, CONTENT_H);
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

        if (m_activeApp == AppId::Files) {
            m_files.handleKey(pressed);
            if (m_files.needsRender()) {
                drawCurrentShell();
                m_files.renderContent(0, CONTENT_Y, SCREEN_W, CONTENT_H);
                m_display.present();
            }
            return;
        }

        if (m_activeApp == AppId::Settings) {
            if (m_settingsApp.handleKey(pressed)) {
                goHome();
                return;
            }
            if (m_settingsApp.consumeCompanionLinkRequest()) {
                maybePersistSettings();
                launch(AppId::Companion);
                return;
            }
            if (m_settingsApp.needsRender()) {
                drawCurrentShell();
                m_settingsApp.renderContent(0, CONTENT_Y, SCREEN_W, CONTENT_H);
                m_display.present();
            }
            return;
        }

        if (m_activeApp == AppId::Companion) {
            if (m_companionLink.handleKey(pressed)) {
                goHome();
                return;
            }
            if (m_companionLink.needsRender()) {
                drawCurrentShell();
                m_companionLink.renderContent(0, CONTENT_Y, SCREEN_W, CONTENT_H);
                m_display.present();
            }
        }
    }

private:
    DisplayRP2350& m_display;
    Keypad& m_keypad;
    HomeScreen& m_home;
    CalculatorApp& m_calculator;
    FileBrowserApp& m_files;
    SettingsApp& m_settingsApp;
    CompanionLinkApp& m_companionLink;
    SettingsStore& m_settingsStore;
    BootManager& m_boot;
    SettingsState& m_settings;
    GraphApp m_graph;
    AppId m_activeApp;
    bool m_waitingForRelease;
    bool m_shellDirty;
    SystemTitleBar m_titleBar;
    SystemStatusState m_status;
    SystemStatusState m_renderedStatus;

    void drawCurrentShell() {
        if (m_activeApp == AppId::Boot) {
            return;
        }
        syncStatus();
        if (!m_shellDirty && m_status == m_renderedStatus) {
            return;
        }

        m_titleBar.render(m_display, m_status);
        m_renderedStatus = m_status;
        m_shellDirty = false;
    }

    void syncStatus() {
        m_status.setAppTitle(appTitleForId(m_activeApp));
        m_status.setBatteryPercentage(SystemStatusState::kDefaultBatteryPercentage);
        m_status.setAngleMode(m_settings.angleMode);
        m_status.setInputLayer(InputLayer::Base);
    }

    void finishBoot() {
        (void)m_calculator.loadPersistentHistory();
        m_home.enter();
        m_activeApp = AppId::Home;
        m_shellDirty = true;
        drawCurrentShell();
        renderHomeContent("boot-finish");
    }

    void goHome() {
        if (m_activeApp != AppId::Home) {
            if (m_activeApp == AppId::Companion) {
                m_display.setTimingLogsEnabled(true);
            }
            maybePersistSettings();
            m_home.enter();
            m_activeApp = AppId::Home;
            m_shellDirty = true;
            drawCurrentShell();
            renderHomeContent("home");
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
            m_shellDirty = true;
            (void)m_calculator.loadPersistentHistory();
            m_calculator.requestRender();
            m_calculator.updateBlink(systemTimeMs());
            drawCurrentShell();
            m_calculator.renderContent(CONTENT_Y, CONTENT_H);
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

        if (app == AppId::Files) {
            m_activeApp = AppId::Files;
            m_files.enter();
            m_shellDirty = true;
            drawCurrentShell();
            m_files.renderContent(0, CONTENT_Y, SCREEN_W, CONTENT_H);
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
            return;
        }

        if (app == AppId::Companion) {
            m_activeApp = AppId::Companion;
            m_display.setTimingLogsEnabled(false);
            m_companionLink.enter(systemTimeMs());
            m_shellDirty = true;
            drawCurrentShell();
            m_companionLink.renderContent(0, CONTENT_Y, SCREEN_W, CONTENT_H);
            m_display.present();
        }
    }

    void renderHomeContent(const char* reason) {
        const uint32_t startUs = time_us_32();
        m_home.renderContent(CONTENT_Y, CONTENT_H);
        const uint32_t us = time_us_32() - startUs;
        std::printf("[render] home content reason=%s time=%lu us (%lu ms)\n",
                    reason,
                    static_cast<unsigned long>(us),
                    static_cast<unsigned long>((us + 500) / 1000));
    }
};

} // namespace

int main() {
    stdio_init_all();
    waitForUsbSerialInDebugBuild();

    DisplayRP2350 display;
    KeypadRP2350 keypad1;
    KeypadRP2350_2 keypad2;
    DualKeypad keypad(keypad1, keypad2);

    SettingsState settings;
    RP2350SettingsStore settingsStore;
    RP2350StartupBackend startup(keypad, settingsStore);
    logEarlyBootDiagnostics(startup.filesystem());

    HomeScreen home(display);
    CalculatorApp calculator(display, keypad, rpCalculatorConfig(settings, &startup.filesystem()));
    FileBrowserApp files(display, &startup.filesystem());
    SettingsApp settingsApp(display, settings, "Hardware", &startup.filesystem());
    RP2350UsbCdcTransport companionTransport;
    RP2350CompanionSystemActions companionSystemActions;
    Companion::CompanionProtocol companionProtocol(startup.filesystem(),
                                                   settings,
                                                   settingsStore,
                                                   {MI23_FIRMWARE_VERSION, MI23_HARDWARE_REVISION},
                                                   &companionSystemActions);
    Companion::CompanionSession companionSession(companionTransport, companionProtocol);
    CompanionLinkApp companionLink(display, companionSession);
    BootManager boot(display, settings, startup);

    RP2350AppController app(display,
                            keypad,
                            home,
                            calculator,
                            files,
                            settingsApp,
                            companionLink,
                            settingsStore,
                            startup.filesystem(),
                            boot,
                            settings);
    app.init();

    while (true) {
        app.tick();
        sleep_ms(16);
    }
}
