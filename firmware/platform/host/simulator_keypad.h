#pragma once

#include "hal/display.h"
#include "hal/keypad.h"

class SimulatorKeypad {
public:
    void render(Display& display) const;
    Key hitTest(int x, int y) const;
};
