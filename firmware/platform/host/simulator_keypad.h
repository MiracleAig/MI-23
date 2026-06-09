#pragma once

#include "app/settings/settings_state.h"
#include "hal/display.h"
#include "hal/keypad.h"

class SimulatorKeypad {
public:
    void render(Display& display, const SettingsState* settings = nullptr) const;
    Key hitTest(int x, int y) const;
};
