#pragma once

#include "app/home/calculator_home.h"
#include "app/settings/settings_state.h"
#include "hal/display.h"
#include "hal/keypad.h"

#include <cstddef>

struct StartupCheckResult {
    bool ok = true;
    bool continueAllowed = true;
    bool repaired = false;
    const char* detail = nullptr;
};

class StartupBackend {
public:
    virtual ~StartupBackend() = default;

    virtual const char* platformName() const = 0;
    virtual const char* firmwareVersion() const = 0;

    virtual StartupCheckResult initializeInput() = 0;
    virtual StartupCheckResult loadSettings(SettingsState& settings) = 0;
    virtual StartupCheckResult checkStorage() = 0;
    virtual StartupCheckResult verifyResources(SettingsState& settings) = 0;
    virtual StartupCheckResult startRuntime(SettingsState& settings) = 0;
};

class BootManager {
public:
    BootManager(Display& display,
                SettingsState& settings,
                StartupBackend& backend);

    void begin();
    void tick();
    void handleKey(Key key);
    void render();

    bool needsRender() const;
    bool isFinished() const;
    bool bootSucceeded() const;
    bool canContinue() const;
    bool inputReady() const;

private:
    enum class State {
        Idle,
        AnnouncingStage,
        RunningStage,
        AwaitingContinue,
        Finished,
    };

    enum class Stage {
        DisplayInit,
        InputInit,
        LoadSettings,
        CheckStorage,
        VerifyResources,
        StartRuntime,
        Count,
    };

    Display& m_display;
    SettingsState& m_settings;
    StartupBackend& m_backend;
    State m_state;
    Stage m_stage;
    int m_completedStages;
    bool m_bootSucceeded;
    bool m_continueAllowed;
    bool m_needsRender;
    char m_status[48];
    char m_detail[96];

    void setStatus(const char* text);
    void setDetail(const char* text);
    void appendDetail(const char* text);
    void advanceStage();
    void finish(bool success);
    void fail(const StartupCheckResult& result);
    int totalStages() const;
    int progressPercent() const;
    const char* stageLabel(Stage stage) const;
    void renderNormal();
    void renderError();
};
