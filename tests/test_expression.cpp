//
// Created by Miracle Aigbogun on 3/21/26.
//

#include <gtest/gtest.h>
#include <cmath>
#include "../firmware/math/expression.h"
#include "hal/keypad.h"

// ── Helpers ──────────────────────────────────────────────────────────────────

#define EXPECT_EVAL(expr_str, expected) {       \
    ExprResult r = evaluate(expr_str);          \
    EXPECT_TRUE(r.ok) << r.error;              \
    EXPECT_NEAR(r.value, expected, 1e-4f);     \
}

#define EXPECT_EVAL_ERROR(expr_str) {           \
    ExprResult r = evaluate(expr_str);          \
    EXPECT_FALSE(r.ok);                         \
}

// Helper to build an expression string containing the π glyph (byte 128)
// Usage: PI_EXPR("2", "")  → "2\x80"  (2π)
//        PI_EXPR("", "+1") → "\x80+1" (π+1)
static std::string pi_expr(const char* before, const char* after) {
    std::string s = before;
    s += (char)128;   // π glyph byte
    s += after;
    return s;
}

static std::string unicode_pi_expr(const char* before, const char* after) {
    std::string s = before;
    s += "\xCF\x80";  // UTF-8 π
    s += after;
    return s;
}

// ── Original tests (unchanged) ────────────────────────────────────────────────

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

TEST(ExpressionParser, PercentStandalone) {
    EXPECT_EVAL("50%", 0.5f);
}

TEST(ExpressionParser, PercentInMultiplication) {
    EXPECT_EVAL("200*10%", 20.0f);
}

TEST(ExpressionParser, PercentAfterParentheses) {
    EXPECT_EVAL("(20+30)%", 0.5f);
}

TEST(ExpressionParser, PercentThenSubtraction) {
    EXPECT_EVAL("50%-1", -0.5f);
}

TEST(ExpressionParser, FactorialBasic) {
    EXPECT_EVAL("5!", 120.0f);
}

TEST(ExpressionParser, FactorialZero) {
    EXPECT_EVAL("0!", 1.0f);
}

TEST(ExpressionParser, FactorialAfterParentheses) {
    EXPECT_EVAL("(3+2)!", 120.0f);
}

TEST(ExpressionParser, FactorialPrecedenceOverPower) {
    EXPECT_EVAL("2^3!", 64.0f);
}

TEST(ExpressionParser, DoubleFactorial) {
    EXPECT_EVAL("3!!", 720.0f);
}

TEST(ExpressionParser, FactorialDomainErrorForFractionalInput) {
    ExprResult r = evaluate("5.5!");
    EXPECT_FALSE(r.ok);
    EXPECT_STREQ(r.error, "Factorial domain error");
}

TEST(ExpressionParser, EulerConstantAlone) {
    EXPECT_EVAL("e", std::exp(1.0f));
}

TEST(ExpressionParser, UppercaseEulerConstantAlone) {
    EXPECT_EVAL("E", std::exp(1.0f));
}

TEST(ExpressionParser, EulerConstantImplicitMultiplyBefore) {
    EXPECT_EVAL("2e", 2.0f * std::exp(1.0f));
}

TEST(ExpressionParser, EulerConstantImplicitMultiplyAfter) {
    EXPECT_EVAL("e2", 2.0f * std::exp(1.0f));
}

TEST(ExpressionParser, EulerConstantInExpression) {
    EXPECT_EVAL("e+1", std::exp(1.0f) + 1.0f);
}

TEST(ExpressionParser, LogBaseX) {
    EXPECT_EVAL("log(2,8)", 3.0f);
}

TEST(ExpressionParser, NaturalLog) {
    EXPECT_EVAL("ln(e)", 1.0f);
}

TEST(ExpressionParser, AnsConstantUsesProvidedValue) {
    ExprResult r = evaluate("Ans+2", 5.0f);
    EXPECT_TRUE(r.ok) << r.error;
    EXPECT_NEAR(r.value, 7.0f, 1e-4f);
}

TEST(ExpressionParser, AnsImplicitMultiplyBefore) {
    ExprResult r = evaluate("2Ans", 5.0f);
    EXPECT_TRUE(r.ok) << r.error;
    EXPECT_NEAR(r.value, 10.0f, 1e-4f);
}

TEST(ExpressionParser, AnsDefaultsToZero) {
    EXPECT_EVAL("Ans", 0.0f);
}

TEST(ExpressionParser, LogImplicitMultiplyBeforeFunction) {
    EXPECT_EVAL("2log(2,8)", 6.0f);
}

TEST(ExpressionParser, LnImplicitMultiplyBeforeFunction) {
    EXPECT_EVAL("2ln(e)", 2.0f);
}

TEST(ExpressionParser, LogDomainError) {
    ExprResult r = evaluate("log(1,10)");
    EXPECT_FALSE(r.ok);
    EXPECT_STREQ(r.error, "Log domain error");
}

TEST(ExpressionParser, LogArgumentDomainError) {
    ExprResult r = evaluate("log(2,0)");
    EXPECT_FALSE(r.ok);
    EXPECT_STREQ(r.error, "Log domain error");
}

TEST(ExpressionParser, LnDomainError) {
    ExprResult r = evaluate("ln(-1)");
    EXPECT_FALSE(r.ok);
    EXPECT_STREQ(r.error, "Natural log domain error");
}

TEST(ExpressionParser, UnaryNegate) {
    EXPECT_EVAL("-5+3", -2.0f);
}

TEST(ExpressionParser, NestedParentheses) {
    EXPECT_EVAL("((2+3))*4", 20.0f);
}

TEST(ExpressionParser, DivisionByZero) {
    ExprResult r = evaluate("1/0");
    EXPECT_TRUE(!r.ok || std::isinf(r.value));
}

// ── Pi constant tests ─────────────────────────────────────────────────────────

// π alone should evaluate to 3.14159...
TEST(PiConstant, PiAlone) {
    EXPECT_EVAL(pi_expr("", "").c_str(), 3.14159265f);
}

// π as right operand: 2π → 6.28318...
TEST(PiConstant, ImplicitMultiplyBefore) {
    EXPECT_EVAL(pi_expr("2", "").c_str(), 2.0f * 3.14159265f);
}

// π as left operand: π2 → 6.28318...
TEST(PiConstant, ImplicitMultiplyAfter) {
    EXPECT_EVAL(pi_expr("", "2").c_str(), 2.0f * 3.14159265f);
}

// Both sides: 2π3 → 2 * π * 3 = 18.8495...
TEST(PiConstant, ImplicitMultiplyBothSides) {
    EXPECT_EVAL(pi_expr("2", "3").c_str(), 2.0f * 3.14159265f * 3.0f);
}

// π in an arithmetic expression: π+1
TEST(PiConstant, PiPlusOne) {
    EXPECT_EVAL(pi_expr("", "+1").c_str(), 3.14159265f + 1.0f);
}

// π as exponent base: π^2
TEST(PiConstant, PiSquared) {
    EXPECT_EVAL(pi_expr("", "^2").c_str(), 3.14159265f * 3.14159265f);
}

// π in parentheses: (π+1)*2
TEST(PiConstant, PiInParentheses) {
    EXPECT_EVAL(pi_expr("(", "+1)*2").c_str(), (3.14159265f + 1.0f) * 2.0f);
}

// Explicit multiply still works: 3*π
TEST(PiConstant, ExplicitMultiplyPi) {
    EXPECT_EVAL(pi_expr("3*", "").c_str(), 3.0f * 3.14159265f);
}

// Decimal before π: 0.5π
TEST(PiConstant, DecimalBeforePi) {
    EXPECT_EVAL(pi_expr("0.5", "").c_str(), 0.5f * 3.14159265f);
}

// Negative π: -π
TEST(PiConstant, NegativePi) {
    EXPECT_EVAL(pi_expr("-", "").c_str(), -3.14159265f);
}

TEST(PiConstant, AsciiPi) {
    EXPECT_EVAL("pi", 3.14159265f);
}

TEST(PiConstant, UppercaseAsciiPi) {
    EXPECT_EVAL("PI", 3.14159265f);
}

TEST(PiConstant, UnicodePi) {
    EXPECT_EVAL(unicode_pi_expr("", "").c_str(), 3.14159265f);
}

TEST(SquareRoot, BasicSquareRoot) {
    EXPECT_EVAL("sqrt(9)", 3.0f);
}

TEST(SquareRoot, NestedExpression) {
    EXPECT_EVAL("sqrt(2*8)", 4.0f);
}

TEST(SquareRoot, ImplicitMultiplyBeforeFunction) {
    EXPECT_EVAL("2sqrt(9)", 6.0f);
}

TEST(SquareRoot, NthRootBasic) {
    EXPECT_EVAL("root(3,8)", 2.0f);
}

TEST(SquareRoot, NthRootNegativeOddIndex) {
    EXPECT_EVAL("root(3,-8)", -2.0f);
}

TEST(SquareRoot, NthRootDomainErrorForEvenNegative) {
    ExprResult r = evaluate("root(2,-4)");
    EXPECT_FALSE(r.ok);
    EXPECT_STREQ(r.error, "Root domain error");
}

TEST(SquareRoot, DomainErrorForNegativeInput) {
    ExprResult r = evaluate("sqrt(-1)");
    EXPECT_FALSE(r.ok);
    EXPECT_STREQ(r.error, "Square root domain error");
}

TEST(TrigFunctions, SineOfZero) {
    EXPECT_EVAL("sin(0)", 0.0f);
}

TEST(TrigFunctions, SineOfPiWithoutParentheses) {
    EXPECT_EVAL(pi_expr("sin", "").c_str(), 0.0f);
}

TEST(TrigFunctions, SineOfAsciiPiWithoutParentheses) {
    EXPECT_EVAL("sin pi", 0.0f);
}

TEST(TrigFunctions, SineOfAsciiPiInParentheses) {
    EXPECT_EVAL("sin(pi)", 0.0f);
}

TEST(TrigFunctions, CosineOfZero) {
    EXPECT_EVAL("cos(0)", 1.0f);
}

TEST(TrigFunctions, TangentOfZero) {
    EXPECT_EVAL("tan(0)", 0.0f);
}

TEST(TrigFunctions, CotangentOfPiOverFour) {
    EXPECT_EVAL(pi_expr("cot(", "/4)").c_str(), 1.0f);
}

TEST(TrigFunctions, SecantOfZero) {
    EXPECT_EVAL("sec(0)", 1.0f);
}

TEST(TrigFunctions, CosecantOfPiOverTwo) {
    EXPECT_EVAL(pi_expr("csc(", "/2)").c_str(), 1.0f);
}

TEST(TrigFunctions, InverseSine) {
    EXPECT_EVAL("asin(1)", 3.14159265f / 2.0f);
}

TEST(TrigFunctions, InverseCosine) {
    EXPECT_EVAL("acos(1)", 0.0f);
}

TEST(TrigFunctions, InverseTangent) {
    EXPECT_EVAL("atan(1)", 3.14159265f / 4.0f);
}

TEST(TrigFunctions, InverseCotangent) {
    EXPECT_EVAL("acot(1)", 3.14159265f / 4.0f);
}

TEST(TrigFunctions, InverseSecant) {
    EXPECT_EVAL("asec(2)", 3.14159265f / 3.0f);
}

TEST(TrigFunctions, InverseCosecant) {
    EXPECT_EVAL("acsc(2)", 3.14159265f / 6.0f);
}

TEST(TrigFunctions, ImplicitMultiplyBeforeTrigFunction) {
    EXPECT_EVAL(pi_expr("2sin(", ")").c_str(), 0.0f);
}

TEST(TrigFunctions, NestedTrigExpression) {
    EXPECT_EVAL("sin(acos(0))", 1.0f);
}

TEST(TrigFunctions, CotangentDomainError) {
    ExprResult r = evaluate("cot(0)");
    EXPECT_FALSE(r.ok);
    EXPECT_STREQ(r.error, "Cotangent domain error");
}

TEST(TrigFunctions, SecantDomainError) {
    ExprResult r = evaluate(pi_expr("sec(", "/2)").c_str());
    EXPECT_FALSE(r.ok);
    EXPECT_STREQ(r.error, "Secant domain error");
}

TEST(TrigFunctions, CosecantDomainError) {
    ExprResult r = evaluate("csc(0)");
    EXPECT_FALSE(r.ok);
    EXPECT_STREQ(r.error, "Cosecant domain error");
}

TEST(TrigFunctions, ArcsineDomainError) {
    ExprResult r = evaluate("asin(2)");
    EXPECT_FALSE(r.ok);
    EXPECT_STREQ(r.error, "Arcsine domain error");
}

TEST(TrigFunctions, ArccosineDomainError) {
    ExprResult r = evaluate("acos(-2)");
    EXPECT_FALSE(r.ok);
    EXPECT_STREQ(r.error, "Arccosine domain error");
}

TEST(TrigFunctions, ArcsecantDomainError) {
    ExprResult r = evaluate("asec(0.5)");
    EXPECT_FALSE(r.ok);
    EXPECT_STREQ(r.error, "Arcsecant domain error");
}

TEST(TrigFunctions, ArccosecantDomainError) {
    ExprResult r = evaluate("acsc(0.5)");
    EXPECT_FALSE(r.ok);
    EXPECT_STREQ(r.error, "Arccosecant domain error");
}

// ── Implicit multiplication tests (general) ───────────────────────────────────

// These existed before but weren't explicitly tested
TEST(ImplicitMultiply, NumberThenParen) {
    EXPECT_EVAL("2(3+1)", 8.0f);
}

TEST(ImplicitMultiply, ParenThenNumber) {
    EXPECT_EVAL("(3+1)2", 8.0f);
}

TEST(ImplicitMultiply, ParenThenParen) {
    EXPECT_EVAL("(2+1)(3+1)", 12.0f);
}

// ── Key enum tests ────────────────────────────────────────────────────────────

// PI must not collide with any action key
TEST(KeyEnum, PiNotCollidingWithActionKeys) {
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::ENTER));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::CLEAR));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::ESCAPE));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::CURSOR_LEFT));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::CURSOR_RIGHT));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::SIN));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::COS));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::TAN));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::COT));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::SEC));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::CSC));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::ASIN));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::ACOS));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::ATAN));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::ACOT));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::ASEC));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::ACSC));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::LOG));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::LN));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::ANS));
    EXPECT_NE(static_cast<int>(Key::PI), static_cast<int>(Key::ROOT));
}

// PI must be considered printable
TEST(KeyEnum, PiIsPrintable) {
    EXPECT_TRUE(isPrintable(Key::PI));
}

// Action keys must NOT be printable
TEST(KeyEnum, ActionKeysNotPrintable) {
    EXPECT_FALSE(isPrintable(Key::ENTER));
    EXPECT_FALSE(isPrintable(Key::CLEAR));
    EXPECT_FALSE(isPrintable(Key::ESCAPE));
    EXPECT_FALSE(isPrintable(Key::CURSOR_LEFT));
    EXPECT_FALSE(isPrintable(Key::CURSOR_RIGHT));
    EXPECT_FALSE(isPrintable(Key::SIN));
    EXPECT_FALSE(isPrintable(Key::COS));
    EXPECT_FALSE(isPrintable(Key::TAN));
    EXPECT_FALSE(isPrintable(Key::COT));
    EXPECT_FALSE(isPrintable(Key::SEC));
    EXPECT_FALSE(isPrintable(Key::CSC));
    EXPECT_FALSE(isPrintable(Key::ASIN));
    EXPECT_FALSE(isPrintable(Key::ACOS));
    EXPECT_FALSE(isPrintable(Key::ATAN));
    EXPECT_FALSE(isPrintable(Key::ACOT));
    EXPECT_FALSE(isPrintable(Key::ASEC));
    EXPECT_FALSE(isPrintable(Key::ACSC));
    EXPECT_FALSE(isPrintable(Key::LOG));
    EXPECT_FALSE(isPrintable(Key::LN));
    EXPECT_FALSE(isPrintable(Key::ANS));
    EXPECT_FALSE(isPrintable(Key::ROOT));
    EXPECT_FALSE(isPrintable(Key::NONE));
}

// toChar(PI) must produce byte 128 — the π font slot
TEST(KeyEnum, PiToCharProduces128) {
    EXPECT_EQ((unsigned char)toChar(Key::PI), 128u);
}

// All digit keys must produce the correct ASCII character
TEST(KeyEnum, DigitKeysToChar) {
    EXPECT_EQ(toChar(Key::NUM_0), '0');
    EXPECT_EQ(toChar(Key::NUM_1), '1');
    EXPECT_EQ(toChar(Key::NUM_2), '2');
    EXPECT_EQ(toChar(Key::NUM_3), '3');
    EXPECT_EQ(toChar(Key::NUM_4), '4');
    EXPECT_EQ(toChar(Key::NUM_5), '5');
    EXPECT_EQ(toChar(Key::NUM_6), '6');
    EXPECT_EQ(toChar(Key::NUM_7), '7');
    EXPECT_EQ(toChar(Key::NUM_8), '8');
    EXPECT_EQ(toChar(Key::NUM_9), '9');
}

// Operator keys must produce correct ASCII characters
TEST(KeyEnum, OperatorKeysToChar) {
    EXPECT_EQ(toChar(Key::PLUS),        '+');
    EXPECT_EQ(toChar(Key::MINUS),       '-');
    EXPECT_EQ(toChar(Key::MULTIPLY),    '*');
    EXPECT_EQ(toChar(Key::DIVIDE),      '/');
    EXPECT_EQ(toChar(Key::POWER),       '^');
    EXPECT_EQ(toChar(Key::PERCENT),     '%');
    EXPECT_EQ(toChar(Key::FACTORIAL),   '!');
    EXPECT_EQ(toChar(Key::E_CONST),     'e');
    EXPECT_EQ(toChar(Key::COMMA),       ',');
    EXPECT_EQ(toChar(Key::OPEN_PAREN),  '(');
    EXPECT_EQ(toChar(Key::CLOSE_PAREN), ')');
    EXPECT_EQ(toChar(Key::DOT),         '.');
}

// ── Edge case tests ───────────────────────────────────────────────────────────

// Empty expression should fail gracefully
TEST(EdgeCases, EmptyExpression) {
    EXPECT_EVAL_ERROR("");
}

// Lone operator should fail gracefully
TEST(EdgeCases, LoneOperator) {
    EXPECT_EVAL_ERROR("+");
}

// Mismatched parentheses should fail
TEST(EdgeCases, MismatchedParens) {
    EXPECT_EVAL_ERROR("(2+3");
}

// Very long chain of operations
TEST(EdgeCases, LongExpression) {
    EXPECT_EVAL("1+2+3+4+5+6+7+8+9+10", 55.0f);
}

// Chained exponents — right-associative: 2^3^2 = 2^(3^2) = 2^9 = 512
TEST(EdgeCases, RightAssociativePower) {
    EXPECT_EVAL("2^3^2", 512.0f);
}

// Double negation: --5 = 5
TEST(EdgeCases, DoubleNegation) {
    EXPECT_EVAL("--5", 5.0f);
}

// Result of zero
TEST(EdgeCases, ZeroResult) {
    EXPECT_EVAL("5-5", 0.0f);
}

// Floating point precision
TEST(EdgeCases, FloatingPoint) {
    EXPECT_EVAL("0.1+0.2", 0.3f);
}
