#include <gtest/gtest.h>

#include <algorithm>

#include "../firmware/hal/display.h"
#include "../firmware/math/math_typeset.h"

class NullDisplay : public Display {
public:
    void init() override {}
    void clear(uint16_t) override {}
    void drawPixel(int, int, uint16_t) override {}
    void drawRect(int, int, int, int, uint16_t) override {}
    void drawText(const char*, int, int, uint16_t) override {}
    void present() override {}
};

class RectTrackingDisplay : public NullDisplay {
public:
    int maxRectHeight = 0;

    void drawRect(int, int, int, int h, uint16_t) override {
        maxRectHeight = std::max(maxRectHeight, h);
    }
};

TEST(MathTypeset, MeasuresPlainRow) {
    math_typeset::LayoutMetrics metrics{};
    EXPECT_TRUE(math_typeset::measure("2+3", metrics));
    EXPECT_EQ(metrics.width, 3 * 6);
    EXPECT_GE(metrics.ascent, 7);
}

TEST(MathTypeset, MeasuresSuperscriptAsTallerThanRow) {
    math_typeset::LayoutMetrics base{};
    math_typeset::LayoutMetrics power{};

    ASSERT_TRUE(math_typeset::measure("x", base));
    ASSERT_TRUE(math_typeset::measure("x^2", power));

    EXPECT_GT(power.ascent, base.ascent);
    EXPECT_GT(power.width, base.width);
}

TEST(MathTypeset, FractionHasBothAscentAndDescent) {
    math_typeset::LayoutMetrics metrics{};
    ASSERT_TRUE(math_typeset::measure("1/2", metrics));

    EXPECT_GT(metrics.ascent, 7);
    EXPECT_GT(metrics.descent, 1);
}

TEST(MathTypeset, SquareRootRendersAsRadicalBox) {
    math_typeset::LayoutMetrics functionText{};
    math_typeset::LayoutMetrics radical{};

    ASSERT_TRUE(math_typeset::measure("sqrt(9)", radical));
    ASSERT_TRUE(math_typeset::measure("sqrt9", functionText));

    EXPECT_LT(radical.width, functionText.width);
    EXPECT_GT(radical.ascent, 7);
}

TEST(MathTypeset, NthRootIncludesIndexSpace) {
    math_typeset::LayoutMetrics squareRoot{};
    math_typeset::LayoutMetrics nthRoot{};

    ASSERT_TRUE(math_typeset::measure("sqrt(9)", squareRoot));
    ASSERT_TRUE(math_typeset::measure("root(3,8)", nthRoot));

    EXPECT_GT(nthRoot.width, squareRoot.width);
}

TEST(MathTypeset, IncompleteSquareRootStillRendersAsRadicalBox) {
    math_typeset::LayoutMetrics functionText{};
    math_typeset::LayoutMetrics radical{};

    ASSERT_TRUE(math_typeset::measure("sqrt", functionText));
    ASSERT_TRUE(math_typeset::measure("sqrt()", radical));

    EXPECT_LT(radical.width, functionText.width);
    EXPECT_GT(radical.ascent, 7);
}

TEST(MathTypeset, IncompleteNthRootStillRendersAsRadicalBox) {
    math_typeset::LayoutMetrics emptyRoot{};
    math_typeset::LayoutMetrics partialRoot{};

    ASSERT_TRUE(math_typeset::measure("root(,)", emptyRoot));
    ASSERT_TRUE(math_typeset::measure("root(3,)", partialRoot));

    EXPECT_GT(emptyRoot.width, 0);
    EXPECT_GT(partialRoot.width, emptyRoot.width);
}

TEST(MathTypeset, SquareRootStrokeStretchesForTallRadicand) {
    RectTrackingDisplay simpleRootDisplay;
    RectTrackingDisplay fractionRootDisplay;

    ASSERT_TRUE(math_typeset::draw("sqrt(9)",
                                   simpleRootDisplay,
                                   0,
                                   30,
                                   Display::WHITE));
    ASSERT_TRUE(math_typeset::draw("sqrt(1/2)",
                                   fractionRootDisplay,
                                   0,
                                   30,
                                   Display::WHITE));

    EXPECT_GT(fractionRootDisplay.maxRectHeight,
              simpleRootDisplay.maxRectHeight);
}

TEST(MathTypeset, FractionHasSideBearingBeforeAdjacentAtoms) {
    math_typeset::LayoutMetrics fraction{};
    math_typeset::LayoutMetrics adjacent{};
    math_typeset::LayoutMetrics atom{};

    ASSERT_TRUE(math_typeset::measure("1/2", fraction));
    ASSERT_TRUE(math_typeset::measure("x", atom));
    ASSERT_TRUE(math_typeset::measure("1/2x", adjacent));

    EXPECT_GT(adjacent.width, fraction.width + atom.width);
}

TEST(MathTypeset, PrefixWidthTracksNestedLayout) {
    const char* expression = "(1+2)/3";
    const int prefixWidth = math_typeset::measurePrefixWidth(expression, 5);

    EXPECT_GT(prefixWidth, 0);
    EXPECT_LT(prefixWidth, 320);
}

TEST(MathTypeset, DrawScaledReportsReducedMetrics) {
    NullDisplay display;
    math_typeset::LayoutMetrics normal{};
    math_typeset::LayoutMetrics scaled{};

    ASSERT_TRUE(math_typeset::measure("Ans+5^5^5^5", normal));
    ASSERT_TRUE(math_typeset::drawScaled("Ans+5^5^5^5",
                                         display,
                                         0,
                                         20,
                                         Display::WHITE,
                                         0.5f,
                                         &scaled));

    EXPECT_LT(scaled.width, normal.width);
    EXPECT_LT(scaled.ascent + scaled.descent, normal.ascent + normal.descent);
}

TEST(MathTypeset, CursorTracksFractionNumeratorAndDenominator) {
    math_typeset::CursorMetrics numerator{};
    math_typeset::CursorMetrics denominator{};

    ASSERT_TRUE(math_typeset::cursorMetrics("1/2", 1, 1.0f, numerator));
    ASSERT_TRUE(math_typeset::cursorMetrics("1/2", 3, 1.0f, denominator));

    EXPECT_LT(numerator.baselineOffset, 0);
    EXPECT_GT(denominator.baselineOffset, 0);
    EXPECT_NE(numerator.baselineOffset, denominator.baselineOffset);
}

TEST(MathTypeset, CursorTracksExponentBaseline) {
    math_typeset::CursorMetrics base{};
    math_typeset::CursorMetrics exponent{};

    ASSERT_TRUE(math_typeset::cursorMetrics("5^2", 1, 1.0f, base));
    ASSERT_TRUE(math_typeset::cursorMetrics("5^2", 3, 1.0f, exponent));

    EXPECT_EQ(base.baselineOffset, 0);
    EXPECT_LT(exponent.baselineOffset, 0);
}

TEST(MathTypesetCursor, UpMovesFromDenominatorToNumerator) {
    const char* expression = "12/345";
    const int start = 4; // inside denominator
    const int moved = math_typeset::moveCursor(expression,
                                               start,
                                               math_typeset::CursorMove::Up);

    EXPECT_LE(moved, 2);
    EXPECT_GE(moved, 0);
}

TEST(MathTypesetCursor, DownMovesFromNumeratorToDenominator) {
    const char* expression = "12/345";
    const int start = 1; // inside numerator
    const int moved = math_typeset::moveCursor(expression,
                                               start,
                                               math_typeset::CursorMove::Down);

    EXPECT_GE(moved, 3);
    EXPECT_LE(moved, 6);
}

TEST(MathTypesetCursor, HorizontalCanExitFractionBounds) {
    const char* expression = "a+1/2+b";

    const int leaveRight = math_typeset::moveCursor(expression,
                                                    3,
                                                    math_typeset::CursorMove::Right);
    EXPECT_EQ(leaveRight, 5);

    const int leaveLeft = math_typeset::moveCursor(expression,
                                                   4,
                                                   math_typeset::CursorMove::Left);
    EXPECT_EQ(leaveLeft, 2);
}

TEST(MathTypesetCursor, HorizontalMovesInsideFractionRegionBeforeExiting) {
    const char* expression = "12/345";

    EXPECT_EQ(math_typeset::moveCursor(expression,
                                       5,
                                       math_typeset::CursorMove::Left),
              4);
    EXPECT_EQ(math_typeset::moveCursor(expression,
                                       4,
                                       math_typeset::CursorMove::Left),
              0);
    EXPECT_EQ(math_typeset::moveCursor(expression,
                                       1,
                                       math_typeset::CursorMove::Right),
              2);
    EXPECT_EQ(math_typeset::moveCursor(expression,
                                       2,
                                       math_typeset::CursorMove::Right),
              6);
}

TEST(MathTypeset, ParenthesesGrowWithTallContent) {
    math_typeset::LayoutMetrics flat{};
    math_typeset::LayoutMetrics tall{};

    ASSERT_TRUE(math_typeset::measure("(x)", flat));
    ASSERT_TRUE(math_typeset::measure("(1/2)", tall));

    EXPECT_GT(tall.ascent + tall.descent, flat.ascent + flat.descent);
}
