#include "platform/host/simulator_keypad.h"

#include "graphics/font.h"
#include "platform/host/simulator_layout.h"

namespace {

struct SimulatorButton {
    const char* label;
    Key key;
};

constexpr int KEY_COLS = 6;
constexpr int KEY_ROWS = 7;
constexpr int KEY_MARGIN = 4;
constexpr int KEY_W = (SIMULATOR_WINDOW_WIDTH - KEY_MARGIN * (KEY_COLS + 1)) / KEY_COLS;
constexpr int KEY_H = (SIMULATOR_KEYPAD_HEIGHT - KEY_MARGIN * (KEY_ROWS + 1)) / KEY_ROWS;

const Color COLOR_PANEL = Display::rgb(12, 15, 20);
const Color COLOR_NORMAL = Display::rgb(42, 48, 58);
const Color COLOR_ACTION = Display::rgb(30, 90, 140);
const Color COLOR_FUNCTION = Display::rgb(80, 50, 100);
const Color COLOR_TEXT = Display::WHITE;
const Color COLOR_BORDER = Display::rgb(94, 102, 116);

const SimulatorButton BUTTONS[KEY_ROWS][KEY_COLS] = {
    {
        {"sin", Key::SIN},
        {"cos", Key::COS},
        {"tan", Key::TAN},
        {"cot", Key::COT},
        {"sec", Key::SEC},
        {"csc", Key::CSC},
    },
    {
        {"asin", Key::ASIN},
        {"acos", Key::ACOS},
        {"atan", Key::ATAN},
        {"acot", Key::ACOT},
        {"asec", Key::ASEC},
        {"acsc", Key::ACSC},
    },
    {
        {"sqrt", Key::SQRT},
        {"pi", Key::PI},
        {"^", Key::POWER},
        {"(", Key::OPEN_PAREN},
        {")", Key::CLOSE_PAREN},
        {"CLR", Key::CLEAR},
    },
    {
        {"7", Key::NUM_7},
        {"8", Key::NUM_8},
        {"9", Key::NUM_9},
        {"/", Key::DIVIDE},
        {"%", Key::PERCENT},
        {"!", Key::FACTORIAL},
    },
    {
        {"4", Key::NUM_4},
        {"5", Key::NUM_5},
        {"6", Key::NUM_6},
        {"*", Key::MULTIPLY},
        {"e", Key::E_CONST},
        {"log", Key::LOG},
    },
    {
        {"1", Key::NUM_1},
        {"2", Key::NUM_2},
        {"3", Key::NUM_3},
        {"-", Key::MINUS},
        {"ln", Key::LN},
        {"x", Key::X_VAR},
    },
    {
        {"0", Key::NUM_0},
        {".", Key::DOT},
        {"+", Key::PLUS},
        {"ENT", Key::ENTER},
        {"Ans", Key::ANS},
        {"nroot", Key::ROOT},
    },
};

int keyX(int col) {
    return KEY_MARGIN + col * (KEY_W + KEY_MARGIN);
}

int keyY(int row) {
    return SIMULATOR_KEYPAD_Y + KEY_MARGIN + row * (KEY_H + KEY_MARGIN);
}

Color colorForKey(Key key) {
    if (key == Key::ENTER || key == Key::CLEAR) {
        return COLOR_ACTION;
    }
    if (key == Key::SIN || key == Key::COS || key == Key::TAN ||
        key == Key::COT || key == Key::SEC || key == Key::CSC ||
        key == Key::ASIN || key == Key::ACOS || key == Key::ATAN ||
        key == Key::ACOT || key == Key::ASEC || key == Key::ACSC ||
        key == Key::SQRT || key == Key::ROOT || key == Key::LOG ||
        key == Key::LN) {
        return COLOR_FUNCTION;
    }
    return COLOR_NORMAL;
}

} // namespace

void SimulatorKeypad::render(Display& display) const {
    display.fillRect(0,
                     SIMULATOR_KEYPAD_Y,
                     SIMULATOR_WINDOW_WIDTH,
                     SIMULATOR_KEYPAD_HEIGHT,
                     COLOR_PANEL);

    for (int row = 0; row < KEY_ROWS; row++) {
        for (int col = 0; col < KEY_COLS; col++) {
            const SimulatorButton& button = BUTTONS[row][col];
            const int x = keyX(col);
            const int y = keyY(row);

            display.fillRect(x, y, KEY_W, KEY_H, colorForKey(button.key));
            display.drawRect(x, y, KEY_W, KEY_H, COLOR_BORDER);

            const int labelX = x + (KEY_W - Display::textWidth(button.label)) / 2;
            const int labelY = y + (KEY_H - FONT_CHAR_HEIGHT) / 2;
            display.drawText(button.label, labelX, labelY, COLOR_TEXT);
        }
    }
}

Key SimulatorKeypad::hitTest(int x, int y) const {
    if (y < SIMULATOR_KEYPAD_Y || y >= SIMULATOR_WINDOW_HEIGHT) {
        return Key::NONE;
    }

    for (int row = 0; row < KEY_ROWS; row++) {
        for (int col = 0; col < KEY_COLS; col++) {
            const int x0 = keyX(col);
            const int y0 = keyY(row);
            if (x >= x0 && x < x0 + KEY_W &&
                y >= y0 && y < y0 + KEY_H) {
                return BUTTONS[row][col].key;
            }
        }
    }

    return Key::NONE;
}
