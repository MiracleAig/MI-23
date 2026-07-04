//
// Created by Miracle Aigbogun on 3/10/26.
//

#pragma once
#include "app/ui/system_status.h"
#include "hal/keypad.h"
#include <SDL2/SDL.h>

class KeypadHost : public Keypad {
public:
    void init() override;

    Key getKey() override;

    void handleEvent(const SDL_Event &event);
    InputLayer activeLayer() const;
private:
    Key m_currentKey = Key::NONE;
    bool m_shiftHeld = false;
    bool m_alphaHeld = false;
};
