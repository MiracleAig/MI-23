#include <gtest/gtest.h>

#include "app/calculator/calculator_app.h"

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

    EXPECT_STREQ(app.input(), "sin()");
    EXPECT_EQ(app.cursorPos(), 4);
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

    EXPECT_STREQ(app.input(), expected);
    EXPECT_EQ(app.cursorPos(), 7);
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
        EXPECT_STREQ(app.input(), "sin(1+2)");
    }

    {
        CalculatorApp app = makeCalculator(display, keypad);
        app.handleKey(Key::COS);
        app.handleKey(Key::NUM_3);
        app.handleKey(Key::MULTIPLY);
        app.handleKey(Key::NUM_4);
        EXPECT_STREQ(app.input(), "cos(3*4)");
    }

    {
        CalculatorApp app = makeCalculator(display, keypad);
        app.handleKey(Key::TAN);
        app.handleKey(Key::NUM_5);
        app.handleKey(Key::MINUS);
        app.handleKey(Key::NUM_2);
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

    EXPECT_STREQ(app.input(), "sin((1+2)*3)");
}

TEST(CalculatorAppInput, DeletingInsideTrigFunctionKeepsClosingParen) {
    CalculatorNullDisplay display;
    CalculatorNullKeypad keypad;
    CalculatorApp app = makeCalculator(display, keypad);

    app.handleKey(Key::SIN);
    app.handleKey(Key::NUM_1);
    app.handleKey(Key::PLUS);
    app.handleKey(Key::NUM_2);
    app.handleKey(Key::CLEAR);

    EXPECT_STREQ(app.input(), "sin(1+)");
    EXPECT_EQ(app.cursorPos(), 6);

    app.handleKey(Key::NUM_3);
    EXPECT_STREQ(app.input(), "sin(1+3)");
}

TEST(CalculatorAppInput, TypingAfterMovingOutOfTrigFunctionKeepsFunctionIntact) {
    CalculatorNullDisplay display;
    CalculatorNullKeypad keypad;
    CalculatorApp app = makeCalculator(display, keypad);

    app.handleKey(Key::SIN);
    app.handleKey(Key::NUM_1);
    app.handleKey(Key::CURSOR_RIGHT);
    app.handleKey(Key::PLUS);
    app.handleKey(Key::NUM_2);

    EXPECT_STREQ(app.input(), "sin(1)+2");
    EXPECT_EQ(app.cursorPos(), 8);
}
