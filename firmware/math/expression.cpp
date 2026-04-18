//
// Created by Miracle Aigbogun on 3/12/26.
//

#include "math/expression.h"
#include <cmath>
#include <cstring>

namespace {

constexpr float PI = 3.14159265f;
constexpr float EULER = 2.71828183f;
constexpr float EPSILON = 1e-6f;

struct FunctionSpec {
    const char* name;
    int length;
    TokenType type;
};

constexpr FunctionSpec FUNCTION_SPECS[] = {
    {"sqrt", 4, TokenType::OP_SQRT},
    {"root", 4, TokenType::OP_ROOT},
    {"asin", 4, TokenType::OP_ASIN},
    {"acos", 4, TokenType::OP_ACOS},
    {"atan", 4, TokenType::OP_ATAN},
    {"acot", 4, TokenType::OP_ACOT},
    {"asec", 4, TokenType::OP_ASEC},
    {"acsc", 4, TokenType::OP_ACSC},
    {"sin", 3, TokenType::OP_SIN},
    {"cos", 3, TokenType::OP_COS},
    {"tan", 3, TokenType::OP_TAN},
    {"cot", 3, TokenType::OP_COT},
    {"sec", 3, TokenType::OP_SEC},
    {"csc", 3, TokenType::OP_CSC},
    {"log", 3, TokenType::OP_LOG},
    {"ln", 2, TokenType::OP_LN},
};

static bool isUnaryFunction(TokenType type) {
    switch (type) {
        case TokenType::OP_NEGATE:
        case TokenType::OP_SQRT:
        case TokenType::OP_SIN:
        case TokenType::OP_COS:
        case TokenType::OP_TAN:
        case TokenType::OP_COT:
        case TokenType::OP_SEC:
        case TokenType::OP_CSC:
        case TokenType::OP_ASIN:
        case TokenType::OP_ACOS:
        case TokenType::OP_ATAN:
        case TokenType::OP_ACOT:
        case TokenType::OP_ASEC:
        case TokenType::OP_ACSC:
        case TokenType::OP_LN:
            return true;
        default:
            return false;
    }
}

static bool matchFunction(const char* expr, int len, int index,
                          TokenType& type, int& matchLength) {
    for (const FunctionSpec& spec : FUNCTION_SPECS) {
        if (index + spec.length > len) {
            continue;
        }
        if (strncmp(&expr[index], spec.name, spec.length) == 0) {
            type = spec.type;
            matchLength = spec.length;
            return true;
        }
    }
    return false;
}

static bool matchPiConstant(const char* expr, int len, int index, int& matchLength) {
    const unsigned char c = static_cast<unsigned char>(expr[index]);
    if (c == 128) {
        matchLength = 1;
        return true;
    }
    if (index + 2 <= len &&
        (expr[index] == 'p' || expr[index] == 'P') &&
        (expr[index + 1] == 'i' || expr[index + 1] == 'I')) {
        matchLength = 2;
        return true;
    }
    if (index + 2 <= len &&
        static_cast<unsigned char>(expr[index]) == 0xCF &&
        static_cast<unsigned char>(expr[index + 1]) == 0x80) {
        matchLength = 2;
        return true;
    }
    return false;
}

static bool matchEulerConstant(const char* expr, int len, int index, int& matchLength) {
    if (index < len && (expr[index] == 'e' || expr[index] == 'E')) {
        matchLength = 1;
        return true;
    }
    return false;
}

static bool matchAnsConstant(const char* expr, int len, int index, int& matchLength) {
    if (index + 3 <= len &&
        (expr[index] == 'a' || expr[index] == 'A') &&
        (expr[index + 1] == 'n' || expr[index + 1] == 'N') &&
        (expr[index + 2] == 's' || expr[index + 2] == 'S')) {
        matchLength = 3;
        return true;
    }
    return false;
}

static float zeroTinyValue(float value) {
    return std::fabs(value) < EPSILON ? 0.0f : value;
}

static ExprResult evalFactorial(float value) {
    if (value < 0.0f) {
        return {false, 0, "Factorial domain error"};
    }

    const float rounded = std::round(value);
    if (std::fabs(value - rounded) > EPSILON) {
        return {false, 0, "Factorial domain error"};
    }

    const int n = static_cast<int>(rounded);
    float result = 1.0f;
    for (int i = 2; i <= n; ++i) {
        result *= static_cast<float>(i);
        if (!std::isfinite(result)) {
            return {false, 0, "Factorial overflow"};
        }
    }
    return {true, result, nullptr};
}

static ExprResult evalUnary(TokenType type, float value) {
    switch (type) {
        case TokenType::OP_NEGATE:
            return {true, -value, nullptr};
        case TokenType::OP_SQRT:
            if (value < 0.0f) {
                return {false, 0, "Square root domain error"};
            }
            return {true, std::sqrt(value), nullptr};
        case TokenType::OP_SIN:
            return {true, zeroTinyValue(std::sin(value)), nullptr};
        case TokenType::OP_COS:
            return {true, zeroTinyValue(std::cos(value)), nullptr};
        case TokenType::OP_TAN:
            return {true, zeroTinyValue(std::tan(value)), nullptr};
        case TokenType::OP_COT: {
            const float sine = std::sin(value);
            if (std::fabs(sine) < EPSILON) {
                return {false, 0, "Cotangent domain error"};
            }
            return {true, std::cos(value) / sine, nullptr};
        }
        case TokenType::OP_SEC: {
            const float cosine = std::cos(value);
            if (std::fabs(cosine) < EPSILON) {
                return {false, 0, "Secant domain error"};
            }
            return {true, 1.0f / cosine, nullptr};
        }
        case TokenType::OP_CSC: {
            const float sine = std::sin(value);
            if (std::fabs(sine) < EPSILON) {
                return {false, 0, "Cosecant domain error"};
            }
            return {true, 1.0f / sine, nullptr};
        }
        case TokenType::OP_ASIN:
            if (value < -1.0f || value > 1.0f) {
                return {false, 0, "Arcsine domain error"};
            }
            return {true, std::asin(value), nullptr};
        case TokenType::OP_ACOS:
            if (value < -1.0f || value > 1.0f) {
                return {false, 0, "Arccosine domain error"};
            }
            return {true, std::acos(value), nullptr};
        case TokenType::OP_ATAN:
            return {true, std::atan(value), nullptr};
        case TokenType::OP_ACOT:
            return {true, std::atan2(1.0f, value), nullptr};
        case TokenType::OP_ASEC:
            if (std::fabs(value) < 1.0f) {
                return {false, 0, "Arcsecant domain error"};
            }
            return {true, std::acos(1.0f / value), nullptr};
        case TokenType::OP_ACSC:
            if (std::fabs(value) < 1.0f) {
                return {false, 0, "Arccosecant domain error"};
            }
            return {true, std::asin(1.0f / value), nullptr};
        case TokenType::OP_LN:
            if (value <= 0.0f) {
                return {false, 0, "Natural log domain error"};
            }
            return {true, std::log(value), nullptr};
        default:
            return {false, 0, "Invalid expression"};
    }
}

} // namespace

static bool canEndValue(TokenType type) {
    return type == TokenType::NUMBER ||
           type == TokenType::PAREN_CLOSE ||
           type == TokenType::OP_PERCENT ||
           type == TokenType::OP_FACTORIAL;
}

static bool canStartValue(TokenType type) {
    return type == TokenType::NUMBER ||
           type == TokenType::PAREN_OPEN ||
           isUnaryFunction(type);
}


/**
 * @brief Tokenizes an arithmetic expression into a sequence of tokens.
 *
 * This method reads a mathematical expression given as a null-terminated string, parses
 * the input into individual tokens (e.g., numbers, operators, and parentheses), and stores
 * these tokens in the provided token array.
 *
 * @param expr A null-terminated input string containing a mathematical expression to be tokenized.
 *             The expression can include floating-point numbers, operators (+, -, *, /), and
 *             parentheses ().
 * @param tokens An array of Token objects where the parsed tokens will be stored.
 *               The size of this array must be at least MAX_STACK to ensure all tokens fit.
 *
 * @return The number of tokens successfully parsed and stored in the tokens array.
 *         Returns -1 if an error occurs, such as encountering an unsupported character
 *         or exceeding the size of the tokens array.
 */
static int tokenize(const char* expr, Token* tokens, float ansValue) {
    int count = 0;
    int i = 0;
    int len = strlen(expr);

    bool expectUnary = true;
    TokenType previousType = TokenType::OP_PLUS;
    bool hasPreviousToken = false;

    while (i < len) {
        char c = expr[i];
        if (c == ' ') {i++; continue; }

        TokenType functionType;
        int functionLength = 0;
        if (matchFunction(expr, len, i, functionType, functionLength)) {
            if (hasPreviousToken && canEndValue(previousType)) {
                if (count >= MAX_TOKENS) return -1;
                tokens[count++] = { TokenType::OP_MULTIPLY, 0.0f };
            }
            if (count >= MAX_TOKENS) return -1;
            tokens[count++] = { functionType, 0.0f };
            previousType     = functionType;
            hasPreviousToken = true;
            expectUnary      = true;
            i += functionLength;
            continue;
        }

        int piLength = 0;
        if (matchPiConstant(expr, len, i, piLength)) {
            if (hasPreviousToken && canEndValue(previousType)) {
                if (count >= MAX_TOKENS) return -1;
                tokens[count++] = { TokenType::OP_MULTIPLY, 0.0f };
            }
            if (count >= MAX_TOKENS) return -1;
            tokens[count++] = { TokenType::NUMBER, PI };
            previousType     = TokenType::NUMBER;
            hasPreviousToken = true;
            expectUnary      = false;
            i += piLength;
            continue;
        }

        int eLength = 0;
        if (matchEulerConstant(expr, len, i, eLength)) {
            if (hasPreviousToken && canEndValue(previousType)) {
                if (count >= MAX_TOKENS) return -1;
                tokens[count++] = { TokenType::OP_MULTIPLY, 0.0f };
            }
            if (count >= MAX_TOKENS) return -1;
            tokens[count++] = { TokenType::NUMBER, EULER };
            previousType     = TokenType::NUMBER;
            hasPreviousToken = true;
            expectUnary      = false;
            i += eLength;
            continue;
        }

        int ansLength = 0;
        if (matchAnsConstant(expr, len, i, ansLength)) {
            if (hasPreviousToken && canEndValue(previousType)) {
                if (count >= MAX_TOKENS) return -1;
                tokens[count++] = { TokenType::OP_MULTIPLY, 0.0f };
            }
            if (count >= MAX_TOKENS) return -1;
            tokens[count++] = { TokenType::NUMBER, ansValue };
            previousType     = TokenType::NUMBER;
            hasPreviousToken = true;
            expectUnary      = false;
            i += ansLength;
            continue;
        }

        if ((c >= '0' && c <= '9') || c == '.') {
            if (hasPreviousToken && canEndValue(previousType)) {
                if (count >= MAX_TOKENS) return -1;
                tokens[count++] = { TokenType::OP_MULTIPLY, 0.0f };
            }

            float value = 0.0f;
            float decimal = 0.0f;
            bool inDecimal = false;
            float decimalPlace = 0.1f;
            bool sawDigit = false;

            while (i < len) {
                char d = expr[i];
                if (d >= '0' && d <= '9') {
                    sawDigit = true;
                    if (inDecimal) {
                        decimal += (d - '0') * decimalPlace;
                        decimalPlace *= 0.1f;
                    } else {
                        value = value * 10.0f + (d - '0');
                    }
                    i++;
                } else if (d == '.' && !inDecimal) {
                    inDecimal = true;
                    i++;
                } else {
                    break;
                }
            }
            if (!sawDigit) {
                return -1;
            }

            if (count >= MAX_TOKENS) return -1;
            tokens[count++] = { TokenType::NUMBER, value + decimal };
            previousType = TokenType::NUMBER;
            hasPreviousToken = true;
            expectUnary = false;
            continue;

        } else {
            TokenType type;
            switch (c) {
                case '+': type = TokenType::OP_PLUS;     break;
                case '-': type = expectUnary ? TokenType::OP_NEGATE : TokenType::OP_MINUS; break;
                case '*': type = TokenType::OP_MULTIPLY; break;
                case '/': type = TokenType::OP_DIVIDE;   break;
                case '%': type = TokenType::OP_PERCENT;  break;
                case '!': type = TokenType::OP_FACTORIAL; break;
                case '^': type = TokenType::OP_POWER;    break;
                case ',': type = TokenType::COMMA;       break;
                case '(': type = TokenType::PAREN_OPEN;  break;
                case ')': type = TokenType::PAREN_CLOSE; break;
                case '=': return count;
                default:
                    return -1;
            }

            if ((type == TokenType::OP_PERCENT || type == TokenType::OP_FACTORIAL) &&
                (!hasPreviousToken || !canEndValue(previousType))) {
                return -1;
            }

            if (hasPreviousToken && canEndValue(previousType) && canStartValue(type)) {
                if (count >= MAX_TOKENS) return -1;
                tokens[count++] = { TokenType::OP_MULTIPLY, 0.0f };
            }

            if (count >= MAX_TOKENS) return -1;
            tokens[count++] = { type, 0.0f };
            i++;

            previousType = type;
            hasPreviousToken = true;

            expectUnary = !canEndValue(type);
        }

        if (count >= MAX_STACK) {
            return -1;
        }
    }
    return count;
}
static int precedence(TokenType type) {
    switch (type) {
        case TokenType::OP_PLUS:
        case TokenType::OP_MINUS: return 1;
        case TokenType::OP_MULTIPLY:
        case TokenType::OP_DIVIDE: return 2;
        case TokenType::OP_PERCENT:
        case TokenType::OP_FACTORIAL: return 5;
        case TokenType::OP_NEGATE:
        case TokenType::OP_SQRT:
        case TokenType::OP_SIN:
        case TokenType::OP_COS:
        case TokenType::OP_TAN:
        case TokenType::OP_COT:
        case TokenType::OP_SEC:
        case TokenType::OP_CSC:
        case TokenType::OP_ASIN:
        case TokenType::OP_ACOS:
        case TokenType::OP_ATAN:
        case TokenType::OP_ACOT:
        case TokenType::OP_ASEC:
        case TokenType::OP_ACSC:
        case TokenType::OP_LN:
            return 3;
        case TokenType::OP_LOG:
        case TokenType::OP_ROOT: return 4;
        case TokenType::OP_POWER: return 4;
        default: return 0;
    }
}

static bool isOperator(TokenType type) {
    return type == TokenType::OP_PLUS       ||
            type == TokenType::OP_MINUS     ||
            type == TokenType::OP_MULTIPLY  ||
            type == TokenType::OP_DIVIDE    ||
            type == TokenType::OP_PERCENT   ||
            type == TokenType::OP_FACTORIAL ||
            type == TokenType::OP_LOG       ||
            type == TokenType::OP_ROOT      ||
            type == TokenType::OP_POWER     ||
            isUnaryFunction(type);
}

static bool isRightAssociative(TokenType type) {
    return type == TokenType::OP_POWER ||
           isUnaryFunction(type);
}

static bool isUnaryEvalOperator(TokenType type) {
    return isUnaryFunction(type) ||
           type == TokenType::OP_PERCENT ||
           type == TokenType::OP_FACTORIAL;
}

/**
 * @brief Converts an infix expression into a postfix representation using the Shunting Yard algorithm.
 *
 * This method processes an array of tokens representing an infix mathematical expression and converts
 * it into a corresponding postfix expression. The algorithm handles operators, parentheses, and numbers,
 * respecting operator precedence and associativity.
 *
 * @param infix A pointer to an array of tokens representing the infix expression. The array must contain
 *              valid tokens with supported types such as operators, numbers, and parentheses.
 * @param infixCount The number of tokens in the infix array. This indicates how many tokens should
 *                   be processed in the conversion.
 * @param postfix A pointer to an array where the resulting postfix expression tokens will be stored.
 *                The size of this array must be sufficient to hold all tokens of the output expression.
 *
 * @return The number of tokens in the resulting postfix expression if the conversion is successful.
 *         Returns -1 if an error occurs, such as mismatched parentheses, exceeding token or stack limitations,
 *         or encountering unsupported input.
 */
static int shuntingYard(const Token* infix, int infixCount, Token* postfix) {
    Token opStack[MAX_STACK];
    int opTop = 0;
    int outCount = 0;

    for (int i = 0; i < infixCount; i++) {
        const Token& token = infix[i];

        if (token.type == TokenType::NUMBER) {
            if (outCount >= MAX_TOKENS) { return -1; }
            postfix[outCount++] = token;
        } else if (isOperator(token.type)) {
            while (opTop > 0 && isOperator(opStack[opTop - 1].type)) {
                TokenType topType = opStack[opTop - 1].type;
                bool shouldPop = isRightAssociative(token.type)
                    ? precedence(topType) > precedence(token.type)
                    : precedence(topType) >= precedence(token.type);

                if (!shouldPop) {
                    break;
                }

                if (outCount >= MAX_TOKENS) { return -1; }
                postfix[outCount++] = opStack[--opTop];
            }
            if (opTop >= MAX_STACK) { return -1; }
            opStack[opTop++] = token;
        } else if (token.type == TokenType::COMMA) {
            bool foundParen = false;
            while (opTop > 0) {
                Token top = opStack[opTop - 1];
                if (top.type == TokenType::PAREN_OPEN) {
                    foundParen = true;
                    break;
                }
                if (outCount >= MAX_TOKENS) { return -1; }
                postfix[outCount++] = opStack[--opTop];
            }
            if (!foundParen) { return -1; }
        } else if (token.type == TokenType::PAREN_OPEN) {
            if (opTop >= MAX_STACK) { return -1; }
            opStack[opTop++] = token;
        } else if (token.type == TokenType::PAREN_CLOSE) {
            bool foundParen = false;
            while (opTop > 0) {
                Token top = opStack[--opTop];
                if (top.type == TokenType::PAREN_OPEN) {
                    foundParen = true;
                    break;
                }
                if (outCount >= MAX_TOKENS) { return -1; }
                postfix[outCount++] = top;
            }
            if (!foundParen) { return -1; }
        }
    }

    while (opTop > 0) {
        Token top = opStack[--opTop];
        if (top.type == TokenType::PAREN_OPEN) { return -1; }
        if (outCount >= MAX_TOKENS) { return -1; }
        postfix[outCount++] = top;
    }

    return outCount;
}

/**
 * @brief Evaluates a mathematical expression given in postfix notation.
 *
 * This method processes a sequence of tokens representing a mathematical expression in
 * postfix (Reverse Polish Notation) format. It uses a stack to compute the result of
 * the expression, supporting operators such as addition, subtraction, multiplication,
 * and division, alongside numeric operands.
 *
 * @param postfix A pointer to the array of Token objects representing the postfix expression.
 *                Each token can represent a number or an operator.
 * @param count The number of tokens in the postfix array.
 *
 * @return An ExprResult structure containing the result of the evaluation. If the evaluation
 *         is successful, the `ok` field is true, and the computed value can be found in the
 *         `value` field. If an error occurs, such as insufficient operands or division by zero,
 *         the `ok` field is false, and the `error` field contains a human-readable error message.
 */
static ExprResult evalPostfix(const Token* postfix, int count) {
    float valStack[MAX_STACK];
    int valTop = 0;

    for (int i = 0; i < count; i++) {
        const Token& token = postfix[i];
        if (token.type == TokenType::NUMBER) {
            if (valTop >= MAX_STACK) return {false, 0, "Stack overflow"};
            valStack[valTop++] = token.value;
        } else if (isUnaryEvalOperator(token.type)) {
            if (valTop < 1) return {false, 0, "Not enough operands"};
            if (token.type == TokenType::OP_PERCENT) {
                valStack[valTop - 1] /= 100.0f;
            } else if (token.type == TokenType::OP_FACTORIAL) {
                const ExprResult factorialResult = evalFactorial(valStack[valTop - 1]);
                if (!factorialResult.ok) {
                    return factorialResult;
                }
                valStack[valTop - 1] = factorialResult.value;
            } else {
                const ExprResult unaryResult = evalUnary(token.type, valStack[valTop - 1]);
                if (!unaryResult.ok) {
                    return unaryResult;
                }
                valStack[valTop - 1] = unaryResult.value;
            }
        } else if (isOperator(token.type)) {
            if (valTop < 2) return {false, 0, "Not enough operands"};
            float b = valStack[--valTop];
            float a = valStack[--valTop];
            float result = 0.0f;

            switch (token.type) {
                case TokenType::OP_PLUS: result = a + b; break;
                case TokenType::OP_MINUS: result = a - b; break;
                case TokenType::OP_MULTIPLY: result = a * b; break;
                case TokenType::OP_DIVIDE:
                    if (b == 0.0f) return {false, 0, "Division by zero"};
                    result = a / b;
                    break;
                case TokenType::OP_POWER:
                    result = std::pow(a, b);
                    break;
                case TokenType::OP_LOG:
                    if (a <= 0.0f || a == 1.0f || b <= 0.0f) {
                        return {false, 0, "Log domain error"};
                    }
                    result = std::log(b) / std::log(a);
                    break;
                case TokenType::OP_ROOT: {
                    if (a == 0.0f) {
                        return {false, 0, "Root domain error"};
                    }
                    if (b < 0.0f) {
                        const float rounded = std::round(a);
                        if (std::fabs(a - rounded) > EPSILON ||
                            (static_cast<int>(std::fabs(rounded)) % 2) == 0) {
                            return {false, 0, "Root domain error"};
                        }
                        result = -std::pow(-b, 1.0f / a);
                    } else {
                        result = std::pow(b, 1.0f / a);
                    }
                    break;
                }
                default: break;
            }
            valStack[valTop++] = result;
        }
    }

    if (valTop != 1) return {false, 0, "Invalid expression"};
    return {true, valStack[0], nullptr};
}

/**
 * @brief Evaluates a mathematical expression and returns the result.
 *
 * This method processes a null-terminated string containing a mathematical expression,
 * tokenizes it into individual components, converts the tokens into postfix notation using
 * the shunting yard algorithm, and evaluates the postfix expression to compute the result.
 *
 * @param expr A null-terminated input string representing the mathematical expression to evaluate.
 *             The expression can contain numbers, operators (+, -, *, /), and parentheses ().
 *
 * @return An ExprResult struct containing the following:
 *         - 'ok': Indicates whether the computation was successful (true) or if there was an error (false).
 *         - 'value': The computed result of the expression if 'ok' is true.
 *         - 'error': A human-readable error message if 'ok' is false.
 */
ExprResult evaluate(const char* expr) {
    return evaluate(expr, 0.0f);
}

ExprResult evaluate(const char* expr, float ansValue) {

    static Token infix[MAX_TOKENS];
    static Token postfix[MAX_TOKENS];

    int infixCount = tokenize(expr, infix, ansValue);
    if (infixCount < 0) return {false, 0, "Invalid expression"};
    if (infixCount == 0) return {false, 0, "Empty expression"};

    int postfixCount = shuntingYard(infix, infixCount, postfix);
    if (postfixCount < 0) return {false, 0, "Invalid expression"};

    return evalPostfix(postfix, postfixCount);
}
