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

class CalculatorClipTrackingDisplay : public Display {
public:
    static constexpr int TitleBarHeight = 22;

    void init() override {}

    void clear(Color color) override {
        DisplayRect rect{0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT};
        if (!clipRect(rect)) {
            return;
        }
        record(rect);
        (void)color;
    }

    void drawPixel(int x, int y, Color color) override {
        if (!clipPoint(x, y)) {
            return;
        }
        record({x, y, 1, 1});
        (void)color;
    }

    void fillRect(int x, int y, int w, int h, Color color) override {
        DisplayRect rect{x, y, w, h};
        if (!clipRect(rect)) {
            return;
        }
        record(rect);
        (void)color;
    }

    void drawText(const char* text, int x, int y, Color color) override {
        if (!text) {
            return;
        }
        DisplayRect rect{x, y, Display::textWidth(text), FONT_CHAR_HEIGHT};
        if (!clipRect(rect)) {
            return;
        }
        record(rect);
        (void)color;
    }

    void present() override {}

    bool wroteAboveTitleBar = false;

private:
    void record(DisplayRect rect) {
        if (rect.y < TitleBarHeight) {
            wroteAboveTitleBar = true;
        }
    }
};

class CalculatorNullKeypad : public Keypad {
public:
    void init() override {}
    Key getKey() override { return Key::NONE; }
};

static CalculatorApp makeCalculator(Display& display,
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

TEST(CalculatorAppRendering, ContentRenderDiscardsStaleFullScreenDirtyRect) {
    CalculatorClipTrackingDisplay display;
    CalculatorNullKeypad keypad;
    CalculatorApp app = makeCalculator(display, keypad);

    app.requestRender();
    app.renderContent(CalculatorClipTrackingDisplay::TitleBarHeight,
                      DISPLAY_HEIGHT - CalculatorClipTrackingDisplay::TitleBarHeight);

    EXPECT_FALSE(display.wroteAboveTitleBar);
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

TEST(SimulatorKeypadPointerInput, VisibleSequenceMapsToExpectedCosExpressionKeys) {
    SimulatorKeypad keypad;
    constexpr int keyCols = 6;
    constexpr int keyRows = 7;
    constexpr int keyMargin = 4;
    constexpr int keyW =
        (SIMULATOR_WINDOW_WIDTH - keyMargin * (keyCols + 1)) / keyCols;
    constexpr int keyH =
        (SIMULATOR_KEYPAD_HEIGHT - keyMargin * (keyRows + 1)) / keyRows;

    auto keyAt = [&](int row, int col) {
        const int x = keyMargin + col * (keyW + keyMargin) + keyW / 2;
        const int y = SIMULATOR_KEYPAD_Y + keyMargin + row * (keyH + keyMargin)
            + keyH / 2;
        return keypad.hitTest(x, y);
    };

    EXPECT_EQ(keyAt(0, 1), Key::COS);
    EXPECT_EQ(keyAt(4, 1), Key::E_CONST);
    EXPECT_EQ(keyAt(5, 3), Key::PLUS);
    EXPECT_EQ(keyAt(2, 5), Key::NUM_3);
    EXPECT_EQ(keyAt(4, 4), Key::CLOSE_PAREN);
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

TEST(CalculatorAppInput, ParenthesisButtonsInsertLiteralCharacters) {
    CalculatorNullDisplay display;
    CalculatorNullKeypad keypad;
    CalculatorApp app = makeCalculator(display, keypad);

    app.handleKey(Key::OPEN_PAREN);
    EXPECT_STREQ(app.input(), "(");
    EXPECT_EQ(app.cursorPos(), 1);

    app.handleKey(Key::CLOSE_PAREN);
    EXPECT_STREQ(app.input(), "()");
    EXPECT_EQ(app.cursorPos(), 2);
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

TEST(CalculatorAppInput, VisibleKeypadCosSequenceBuildsExpectedExpression) {
    SimulatorKeypad simulatorKeypad;
    CalculatorNullDisplay display;
    CalculatorNullKeypad keypad;
    CalculatorApp app = makeCalculator(display, keypad);
    constexpr int keyCols = 6;
    constexpr int keyRows = 7;
    constexpr int keyMargin = 4;
    constexpr int keyW =
        (SIMULATOR_WINDOW_WIDTH - keyMargin * (keyCols + 1)) / keyCols;
    constexpr int keyH =
        (SIMULATOR_KEYPAD_HEIGHT - keyMargin * (keyRows + 1)) / keyRows;

    auto pressVisible = [&](int row, int col) {
        const int x = keyMargin + col * (keyW + keyMargin) + keyW / 2;
        const int y = SIMULATOR_KEYPAD_Y + keyMargin + row * (keyH + keyMargin)
            + keyH / 2;
        app.handleKey(simulatorKeypad.hitTest(x, y));
    };

    pressVisible(0, 1); // cos
    pressVisible(4, 1); // e
    pressVisible(5, 3); // +
    pressVisible(2, 5); // 3
    pressVisible(4, 4); // )

    EXPECT_STREQ(app.input(), "cos(e+3)");
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

TEST(CalculatorAppInput, FunctionCallsWithConstantsStayUncorruptedAfterCloseParen) {
    struct Case {
        Key function;
        Key first;
        Key op;
        Key second;
        const char* expected;
    };

    constexpr Case cases[] = {
        {Key::COS, Key::E_CONST, Key::PLUS, Key::NUM_3, "cos(e+3)"},
        {Key::COS, Key::E_CONST, Key::PLUS, Key::NUM_4, "cos(e+4)"},
        {Key::SIN, Key::NUM_1, Key::NONE, Key::NONE, "sin(1)"},
        {Key::LOG, Key::NUM_1, Key::NONE, Key::NUM_0, "log(10)"},
        {Key::SQRT, Key::NUM_9, Key::NONE, Key::NONE, "sqrt(9)"},
    };

    for (const Case& c : cases) {
        CalculatorNullDisplay display;
        CalculatorNullKeypad keypad;
        CalculatorApp app = makeCalculator(display, keypad);

        app.handleKey(c.function);
        app.handleKey(c.first);
        if (c.op != Key::NONE) {
            app.handleKey(c.op);
        }
        if (c.second != Key::NONE) {
            app.handleKey(c.second);
        }
        app.handleKey(Key::CLOSE_PAREN);

        EXPECT_STREQ(app.input(), c.expected);
        EXPECT_EQ(app.cursorPos(), static_cast<int>(std::strlen(c.expected)));
    }
}

TEST(CalculatorAppSettings, PrecisionControlsFormattedResult) {
    CalculatorNullDisplay display;
    CalculatorNullKeypad keypad;
    SettingsState settings;
    settings.calculatorPrecision = 3;
    CalculatorAppConfig config{};
    config.settings = &settings;
    CalculatorApp app(display, keypad, config);

    app.handleKey(Key::NUM_1);
    app.handleKey(Key::DIVIDE);
    app.handleKey(Key::NUM_3);
    app.handleKey(Key::ENTER);

    ASSERT_EQ(app.historySize(), 1);
    EXPECT_STREQ(app.historyAt(0).result.c_str(), "0.333");
}

TEST(CalculatorAppSettings, AngleModeDegreesAffectsTrigEvaluation) {
    CalculatorNullDisplay display;
    CalculatorNullKeypad keypad;
    SettingsState settings;
    settings.angleMode = AngleMode::Degrees;
    settings.calculatorPrecision = 3;
    CalculatorAppConfig config{};
    config.settings = &settings;
    CalculatorApp app(display, keypad, config);

    app.handleKey(Key::SIN);
    app.handleKey(Key::NUM_9);
    app.handleKey(Key::NUM_0);
    app.handleKey(Key::CLOSE_PAREN);
    app.handleKey(Key::ENTER);

    ASSERT_EQ(app.historySize(), 1);
    EXPECT_STREQ(app.historyAt(0).result.c_str(), "1.000");
}
