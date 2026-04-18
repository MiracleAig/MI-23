//
// Created by Miracle Aigbogun on 3/12/26.
//

#pragma once
#include <cstdint>

static const int MAX_TOKENS = 64;
static const int MAX_STACK = 64;

enum class TokenType {
    NUMBER,
    OP_PLUS,
    OP_MINUS,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_PERCENT,
    OP_FACTORIAL,
    OP_POWER,
    OP_NEGATE,
    OP_SQRT,
    OP_SIN,
    OP_COS,
    OP_TAN,
    OP_ASIN,
    OP_ACOS,
    OP_ATAN,
    OP_SEC,
    OP_COT,
    OP_CSC,
    OP_ASEC,
    OP_ACOT,
    OP_ACSC,
    OP_LOG,
    OP_LN,
    OP_ROOT,
    COMMA,
    PAREN_OPEN,
    PAREN_CLOSE,
};

struct Token {
    TokenType type;
    float value;
};

struct ExprResult {
    bool ok; // false if there was a parsing or math error
    float value; // the return result of the expression
    const char* error; // human-readable error message
};

ExprResult evaluate(const char* expr);
ExprResult evaluate(const char* expr, float ansValue);
