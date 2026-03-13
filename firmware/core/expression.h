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
    OP_POWER,
    OP_NEGATE,
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