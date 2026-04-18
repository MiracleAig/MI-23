#include <gtest/gtest.h>

#include "../firmware/math/math_typeset.h"

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

TEST(MathTypeset, PrefixWidthTracksNestedLayout) {
    const char* expression = "(1+2)/3";
    const int prefixWidth = math_typeset::measurePrefixWidth(expression, 5);

    EXPECT_GT(prefixWidth, 0);
    EXPECT_LT(prefixWidth, 320);
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

TEST(MathTypeset, ParenthesesGrowWithTallContent) {
    math_typeset::LayoutMetrics flat{};
    math_typeset::LayoutMetrics tall{};

    ASSERT_TRUE(math_typeset::measure("(x)", flat));
    ASSERT_TRUE(math_typeset::measure("(1/2)", tall));

    EXPECT_GT(tall.ascent + tall.descent, flat.ascent + flat.descent);
}
