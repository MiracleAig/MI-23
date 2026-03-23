//
// Created by Miracle Aigbogun on 3/21/26.
//

#include <gtest/gtest.h>
#include <cmath>
#include "core/expression.h"


#define EXPECT_EVAL(expr_str, expected) {       \
ExprResult r = evaluate(expr_str);          \
EXPECT_TRUE(r.ok) << r.error;              \
EXPECT_NEAR(r.value, expected, 1e-4f);     \
}

// Helper for tests expected to fail (error cases)
#define EXPECT_EVAL_ERROR(expr_str) {           \
ExprResult r = evaluate(expr_str);          \
EXPECT_FALSE(r.ok);                         \
}

TEST(ExpressionParser, BasicAddition) {
    EXPECT_EVAL("2+3", 5.0f);
}

TEST(ExpressionParser, OrderOfOperations) {
    EXPECT_EVAL("2+3*4", 14.0f);
}

TEST(ExpressionParser, Parentheses) {
    EXPECT_EVAL("(2+3)*4", 20.0f);
}

TEST(ExpressionParser, NegativeResult) {
    EXPECT_EVAL("3-10", -7.0f);
}

TEST(ExpressionParser, Division) {
    EXPECT_EVAL("10/4", 2.5f);
}

TEST(ExpressionParser, FractionSyntax) {
    EXPECT_EVAL("(1)/(2)", 0.5f);
}

TEST(ExpressionParser, Power) {
    EXPECT_EVAL("2^3", 8.0f);
}

TEST(ExpressionParser, UnaryNegate) {
    EXPECT_EVAL("-5+3", -2.0f);
}

TEST(ExpressionParser, NestedParentheses) {
    EXPECT_EVAL("((2+3))*4", 20.0f);
}

TEST(ExpressionParser, DivisionByZero) {
    // Your parser either returns ok=false OR an inf value — check which
    ExprResult r = evaluate("1/0");
    EXPECT_TRUE(!r.ok || std::isinf(r.value));
}