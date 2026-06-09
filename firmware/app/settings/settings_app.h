#pragma once

#include "app/settings/settings_state.h"
#include "hal/display.h"
#include "hal/keypad.h"

class SettingsApp {
public:
    SettingsApp(Display& display, SettingsState& settings, const char* platformName);

    void enter();
    bool handleKey(Key key);
    void renderContent(int x, int y, int w, int h);
    void requestRender();
    bool needsRender() const;

private:
    enum class Screen {
        Main,
        About,
        Developer,
        ResetConfirm,
    };

    Display& m_display;
    SettingsState& m_settings;
    const char* m_platformName;
    Screen m_screen;
    int m_selectedIndex;
    int m_developerIndex;
    bool m_needsRender;

    void renderMain(int x, int y, int w, int h);
    void renderAbout(int x, int y, int w, int h);
    void renderDeveloper(int x, int y, int w, int h);
    void renderResetConfirm(int x, int y, int w, int h);
    void cycleSelected(int direction);
    void cyclePrecision(int direction);
    void toggleDeveloperSelected();
};
