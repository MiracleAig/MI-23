//
// Created by Miracle Aigbogun on 3/23/26.
//

#include "keypad_rp2350.h"
#include "pico/stdlib.h"


KeypadRP2350::KeypadRP2350()
    : m_lastStableKey(Key::NONE)
    , m_candidateKey(Key::NONE)
    , m_candidateTime(0)
    , m_lastRepeatTime(0)
    , m_repeating(false)
{}

void KeypadRP2350::init() {
    // --- Set up ROW pins as outputs ---
    // During idle (not scanning), all rows are driven HIGH so no key appears pressed.
    for (uint row : ROW_PINS) {
        gpio_init(row);
        gpio_set_dir(row, GPIO_OUT);
        gpio_put(row, 1); // HIGH = idle
    }

    // --- Set up COLUMN pins as inputs with pull-ups ---
    // "Pull-up" means the pin reads HIGH by default.
    // When a key is pressed it connects the column to the active-LOW row,
    // pulling the column pin down to 0V (LOW).
    for (uint col : COL_PINS) {
        gpio_init(col);
        gpio_set_dir(col, GPIO_IN);
        gpio_pull_up(col);
    }
}

// ---------------------------------------------------------------------------
// getKey — call once per frame from your main loop
// Returns a Key the moment it should be acted on (press + debounced repeats).
// Returns Key::NONE when nothing actionable is happening.
// ---------------------------------------------------------------------------
Key KeypadRP2350::getKey() {
    Key current = scanMatrix();
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // --- Debounce logic ---
    // A key only "counts" after it has been held continuously for DEBOUNCE_MS.
    // This filters out the electrical noise ("bounce") when a switch first closes.

    if (current != m_candidateKey) {
        // Signal changed — restart the debounce timer
        m_candidateKey  = current;
        m_candidateTime = now;
        m_repeating     = false;
        return Key::NONE;
    }

    // Signal has been stable — how long?
    uint32_t heldMs = now - m_candidateTime;

    if (heldMs < DEBOUNCE_MS) {
        // Not stable long enough yet
        return Key::NONE;
    }

    // --- Key confirmed as pressed ---

    if (current == Key::NONE) {
        // Nothing held — reset repeat state
        if (m_lastStableKey != Key::NONE) {
            m_lastStableKey  = Key::NONE;
            m_repeating      = false;
        }
        return Key::NONE;
    }

    // Fresh press (key just became stable)
    if (current != m_lastStableKey) {
        m_lastStableKey  = current;
        m_lastRepeatTime = now;
        m_repeating      = false;
        return current; // Fire the initial press event
    }

    // --- Key-repeat logic ---
    // After holding a key for REPEAT_DELAY_MS, fire it again every REPEAT_RATE_MS.
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

// ---------------------------------------------------------------------------
// scanMatrix — drives each row LOW in turn and reads all columns
// ---------------------------------------------------------------------------
Key KeypadRP2350::scanMatrix() {
    Key found = Key::NONE;

    for (int r = 0; r < 4; r++) {
        // Drive this row LOW (active), keep all others HIGH (idle)
        gpio_put(ROW_PINS[r], 0);

        // Small delay to let the signal settle on the wire before reading.
        // "Propagation delay" — electricity isn't truly instant across a wire+resistor.
        sleep_us(10);

        for (int c = 0; c < 4; c++) {
            if (!gpio_get(COL_PINS[c])) { // LOW = key pressed
                if (found != Key::NONE) {
                    // Two keys detected simultaneously — ambiguous, ignore both.
                    // This is called "ghosting" in matrix keyboards.
                    gpio_put(ROW_PINS[r], 1);
                    return Key::NONE;
                }
                found = mapKey(r, c);
            }
        }

        // Restore this row to HIGH before scanning the next one
        gpio_put(ROW_PINS[r], 1);
    }

    return found;
}

// ---------------------------------------------------------------------------
// mapKey — converts (row, col) to a Key enum value
//
// Physical 4x4 layout assumed (standard membrane keypad):
//   [ 1 ][ 2 ][ 3 ][ A ]
//   [ 4 ][ 5 ][ 6 ][ B ]
//   [ 7 ][ 8 ][ 9 ][ C ]
//   [ * ][ 0 ][ + ][ D ]
//
// A/B/C/D (rightmost column) are mapped to calculator actions.
// * and + are mapped to useful math symbols.
// ---------------------------------------------------------------------------
Key KeypadRP2350::mapKey(int row, int col) {
    // clang-format off
    static const Key TABLE[4][4] = {
        { Key::NUM_1, Key::NUM_2, Key::NUM_3, Key::ENTER  },
        { Key::NUM_4, Key::NUM_5, Key::NUM_6, Key::CLEAR  },
        { Key::NUM_7, Key::NUM_8, Key::NUM_9, Key::ESCAPE },
        { Key::PLUS,  Key::NUM_0, Key::DOT,   Key::MINUS  },
    };
    // clang-format on
    return TABLE[row][col];
}