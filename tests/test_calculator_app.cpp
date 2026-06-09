#include <gtest/gtest.h>

#include "app/calculator/calculator_app.h"
#include "platform/host/simulator_keypad.h"
#include "platform/host/simulator_layout.h"

#include <cstring>

class CalculatorNullDisplay : public Display {
public:
    void init() override {}
    void clear(Color) override {}
    void drawPixel(int, int, Color) override {}
    void fillRect(int, int, int, int, Color) override {}
    void drawText(const char*, int, int, Color) override {}
    void present() override {}
};

class CalculatorNullKeypad : public Keypad {
public:
    void init() override {}
    Key getKey() override { return Key::NONE; }
};

static CalculatorApp makeCalculator(CalculatorNullDisplay& display,
                                    CalculatorNullKeypad& keypad) {
    CalculatorAppConfig config{};
    config.showOnScreenKeypad = false;
    return CalculatorApp(display, keypad, config);
}

TEST(CalculatorAppPointerInput, HiddenOnScreenKeypadDoesNotAcceptClicks) {
    CalculatorNullDisplay display;
    CalculatorNullKeypad keypad;
    CalculatorApp app = makeCalculator(display, keypad);

    app.handlePointerDown(BTN_MARGIN + 1, BTN_AREA_TOP + BTN_MARGIN + 1);

    EXPECT_STREQ(app.input(), "");
}

TEST(CalculatorAppPointerInput, VisibleOnScreenKeypadAcceptsClicks) {
    CalculatorNullDisplay display;
    CalculatorNullKeypad keypad;
    CalculatorAppConfig config{};
    config.showOnScreenKeypad = true;
    CalculatorApp app(display, keypad, config);

    app.handlePointerDown(BTN_MARGIN + 1, BTN_AREA_TOP + BTN_MARGIN + 1);

    EXPECT_STREQ(app.input(), "sin(");
    EXPECT_EQ(app.cursorPos(), 4);
}

TEST(SimulatorKeypadPointerInput, VisibleFunctionButtonMapsToOneFunctionKey) {
    SimulatorKeypad keypad;
    constexpr int keyCols = 6;
    constexpr int keyRows = 7;
    constexpr int keyMargin = 4;
    constexpr int keyW =
        (SIMULATOR_WINDOW_WIDTH - keyMargin * (keyCols + 1)) / keyCols;
    constexpr int keyH =
        (SIMULATOR_KEYPAD_HEIGHT - keyMargin * (keyRows + 1)) / keyRows;

    const int cosX = keyMargin + 1 * (keyW + keyMargin) + keyW / 2;
    const int cosY = SIMULATOR_KEYPAD_Y + keyMargin + keyH / 2;

    EXPECT_EQ(keypad.hitTest(cosX, cosY), Key::COS);
    EXPECT_EQ(keypad.hitTest(cosX, SIMULATOR_KEYPAD_Y - 1), Key::NONE);
}

TEST(CalculatorAppInput, FunctionButtonsInsertOnlyOpenCallPrefix) {
    struct Case {
        Key key;
        const char* expected;
    };

    constexpr Case cases[] = {
        {Key::SIN, "sin("},
        {Key::COS, "cos("},
        {Key::TAN, "tan("},
        {Key::COT, "cot("},
        {Key::SEC, "sec("},
        {Key::CSC, "csc("},
        {Key::ASIN, "asin("},
        {Key::ACOS, "acos("},
        {Key::ATAN, "atan("},
        {Key::ACOT, "acot("},
        {Key::ASEC, "asec("},
        {Key::ACSC, "acsc("},
        {Key::LOG, "log("},
        {Key::LN, "ln("},
        {Key::SQRT, "sqrt("},
        {Key::ROOT, "root("},
    };

    for (const Case& c : cases) {
        CalculatorNullDisplay display;
        CalculatorNullKeypad keypad;
        CalculatorApp app = makeCalculator(display, keypad);

        app.handleKey(c.key);

        EXPECT_STREQ(app.input(), c.expected);
        EXPECT_EQ(app.cursorPos(), static_cast<int>(std::strlen(c.expected)));
    }
}

TEST(CalculatorAppInput, TrigArgumentAcceptsPiPlusOne) {
    CalculatorNullDisplay display;
    CalculatorNullKeypad keypad;
    CalculatorApp app = makeCalculator(display, keypad);
    const char expected[] = {'s', 'i', 'n', '(', static_cast<char>(128), '+', '1', ')', '\0'};

    app.handleKey(Key::SIN);
    app.handleKey(Key::PI);
    app.handleKey(Key::PLUS);
    app.handleKey(Key::NUM_1);
    app.handleKey(Key::CLOSE_PAREN);

    EXPECT_STREQ(app.input(), expected);
    EXPECT_EQ(app.cursorPos(), 8);
}

TEST(CalculatorAppInput, TrigArgumentsAcceptFullExpressions) {
    CalculatorNullDisplay display;
    CalculatorNullKeypad keypad;

    {
        CalculatorApp app = makeCalculator(display, keypad);
        app.handleKey(Key::SIN);
        app.handleKey(Key::NUM_1);
        app.handleKey(Key::PLUS);
        app.handleKey(Key::NUM_2);
        app.handleKey(Key::CLOSE_PAREN);
        EXPECT_STREQ(app.input(), "sin(1+2)");
    }

    {
        CalculatorApp app = makeCalculator(display, keypad);
        app.handleKey(Key::COS);
        app.handleKey(Key::NUM_3);
        app.handleKey(Key::MULTIPLY);
        app.handleKey(Key::NUM_4);
        app.handleKey(Key::CLOSE_PAREN);
        EXPECT_STREQ(app.input(), "cos(3*4)");
    }

    {
        CalculatorApp app = makeCalculator(display, keypad);
        app.handleKey(Key::TAN);
        app.handleKey(Key::NUM_5);
        app.handleKey(Key::MINUS);
        app.handleKey(Key::NUM_2);
        app.handleKey(Key::CLOSE_PAREN);
        EXPECT_STREQ(app.input(), "tan(5-2)");
    }
}

TEST(CalculatorAppInput, TrigArgumentAcceptsNestedParentheses) {
    CalculatorNullDisplay display;
    CalculatorNullKeypad keypad;
    CalculatorApp app = makeCalculator(display, keypad);

    app.handleKey(Key::SIN);
    app.handleKey(Key::OPEN_PAREN);
    app.handleKey(Key::NUM_1);
    app.handleKey(Key::PLUS);
    app.handleKey(Key::NUM_2);
    app.handleKey(Key::CLOSE_PAREN);
    app.handleKey(Key::MULTIPLY);
    app.handleKey(Key::NUM_3);
    app.handleKey(Key::CLOSE_PAREN);

    EXPECT_STREQ(app.input(), "sin((1+2)*3)");
}

TEST(CalculatorAppInput, BackspaceAfterFunctionPrefixRemovesOnlyTypedCharacters) {
    CalculatorNullDisplay display;
    CalculatorNullKeypad keypad;
    CalculatorApp app = makeCalculator(display, keypad);

    app.handleKey(Key::SIN);
    app.handleKey(Key::NUM_1);
    app.handleKey(Key::PLUS);
    app.handleKey(Key::NUM_2);
    app.handleKey(Key::CLEAR);

    EXPECT_STREQ(app.input(), "sin(1+");
    EXPECT_EQ(app.cursorPos(), 6);

    app.handleKey(Key::NUM_3);
    EXPECT_STREQ(app.input(), "sin(1+3");
}

TEST(CalculatorAppInput, FunctionCallCanBeClosedManuallyBeforeContinuing) {
    CalculatorNullDisplay display;
    CalculatorNullKeypad keypad;
    CalculatorApp app = makeCalculator(display, keypad);

    app.handleKey(Key::SIN);
    app.handleKey(Key::NUM_1);
    app.handleKey(Key::CLOSE_PAREN);
    app.handleKey(Key::PLUS);
    app.handleKey(Key::NUM_2);

    EXPECT_STREQ(app.input(), "sin(1)+2");
    EXPECT_EQ(app.cursorPos(), 8);
}
