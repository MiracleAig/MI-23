//
// Created by Miracle Aigbogun on 3/28/26.
//

#include "platform/rp2350/keypad_rp2350_2.h"
#include "pico/stdlib.h"


KeypadRP2350_2::KeypadRP2350_2()
    : m_lastStableKey(Key::NONE)
    , m_candidateKey(Key::NONE)
    , m_candidateTime(0)
    , m_lastRepeatTime(0)
    , m_repeating(false)
{}

void KeypadRP2350_2::init() {
    for (uint row : ROW_PINS) {
        gpio_init(row);
        gpio_set_dir(row, GPIO_OUT);
        gpio_put(row, 1);
    }
    for (uint col : COL_PINS) {
        gpio_init(col);
        gpio_set_dir(col, GPIO_IN);
        gpio_pull_up(col);
    }
}

Key KeypadRP2350_2::getKey() {
    Key current = scanMatrix();
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (current != m_candidateKey) {
        m_candidateKey  = current;
        m_candidateTime = now;
        m_repeating     = false;
        return Key::NONE;
    }

    uint32_t heldMs = now - m_candidateTime;
    if (heldMs < DEBOUNCE_MS) return Key::NONE;

    if (current == Key::NONE) {
        if (m_lastStableKey != Key::NONE) {
            m_lastStableKey = Key::NONE;
            m_repeating     = false;
        }
        return Key::NONE;
    }

    if (current != m_lastStableKey) {
        m_lastStableKey  = current;
        m_lastRepeatTime = now;
        m_repeating      = false;
        return current;
    }

    if (!m_repeating) {
        if (heldMs >= REPEAT_DELAY_MS) {
            m_repeating      = true;
            m_lastRepeatTime = now;
            return current;
        }
    } else {
        if (now - m_lastRepeatTime >= REPEAT_RATE_MS) {
            m_lastRepeatTime = now;
            return current;
        }
    }

    return Key::NONE;
}

Key KeypadRP2350_2::scanMatrix() {
    Key found = Key::NONE;
    for (int r = 0; r < 4; r++) {
        gpio_put(ROW_PINS[r], 0);
        sleep_us(10);
        for (int c = 0; c < 4; c++) {
            if (!gpio_get(COL_PINS[c])) {
                if (found != Key::NONE) {
                    gpio_put(ROW_PINS[r], 1);
                    return Key::NONE;
                }
                found = mapKey(r, c);
            }
        }
        gpio_put(ROW_PINS[r], 1);
    }
    return found;
}

Key KeypadRP2350_2::mapKey(int row, int col) {
    // clang-format off
    static const Key TABLE[4][4] = {
        { Key::CURSOR_LEFT, Key::CURSOR_RIGHT, Key::CURSOR_UP,   Key::CURSOR_DOWN },
        { Key::OPEN_PAREN,  Key::CLOSE_PAREN,  Key::POWER,       Key::SQRT        },
        { Key::MULTIPLY,    Key::DIVIDE,        Key::PERCENT,     Key::NEGATE      },
        { Key::SIN,         Key::COS,           Key::TAN,         Key::PI          },
    };
    // clang-format on
    return TABLE[row][col];
}
