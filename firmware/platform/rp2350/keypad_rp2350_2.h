//
// Created by Miracle Aigbogun on 3/28/26.
// Second 4x4 Keypad - navigation and extended math keys (RED TAPE)
//

#pragma once
#include "config/pin_config.h"
#include "hal/keypad.h"
#include "pico/stdlib.h"


class KeypadRP2350_2 : public Keypad {
public:
    KeypadRP2350_2();

    void init() override;
    Key getKey() override;

private:
    Key scanMatrix();
    Key mapKey(int row, int col);

    Key      m_lastStableKey;
    Key      m_candidateKey;
    uint32_t m_candidateTime;
    uint32_t m_lastRepeatTime;
    bool     m_repeating;

    static constexpr uint32_t DEBOUNCE_MS     = 30;
    static constexpr uint32_t REPEAT_DELAY_MS = 500;
    static constexpr uint32_t REPEAT_RATE_MS  = 80;

    static constexpr uint ROW_PINS[4] = {
        PIN_KP2_ROW_0, PIN_KP2_ROW_1, PIN_KP2_ROW_2, PIN_KP2_ROW_3
    };
    static constexpr uint COL_PINS[4] = {
        PIN_KP2_COL_0, PIN_KP2_COL_1, PIN_KP2_COL_2, PIN_KP2_COL_3
    };
};