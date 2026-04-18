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
