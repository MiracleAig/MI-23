//
// Created by Miracle Aigbogun on 3/23/26.
//

#pragma once

#include "hal/keypad.h"
#include "pico/stdlib.h"

class KeypadRP2350 : public Keypad {
public:
    KeypadRP2350();

    void init() override;
    Key getKey() override;

private:
    Key scanMatrix();
    Key mapKey(int row, int col);

    Key m_lastStableKey;
    Key m_candidateKey;
    uint32_t m_candidateTime;
    uint32_t m_lastRepeatTime;
    bool m_repeating;

    static constexpr uint32_t DEBOUNCE_MS     = 30;
    static constexpr uint32_t REPEAT_DELAY_MS = 500;
    static constexpr uint32_t REPEAT_RATE_MS  = 80;


    static constexpr uint ROW_PINS[4] = {19, 20, 26, 21};
    static constexpr uint COL_PINS[4] = {16,  6,  9,  7};
};