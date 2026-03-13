//
// Created by Miracle Aigbogun on 3/12/26.
//

#include "core/expression.h"
#include <cstdio>
#include <cstring>

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
static int tokenize(const char* expr, Token* tokens) {
    int count = 0;
    int i = 0;
    int len = strlen(expr);
    printf("Tokenizing string of length %d: [%s]\n", len, expr);

    while (i < len) {
        char c = expr[i];
        printf("Tokenizer seeing char: '%c' (ascii %d)\n", c, (int)c);
        if (c == ' ') {i++; continue; }

        if ((c >= '0' && c <= '9') || c == '.') {
            float value = 0.0f;
            float decimal = 0.0f;
            bool inDecimal = false;
            float decimalPlace = 0.1f;

            while (i < len) {
                char d = expr[i];
                if (d >= '0' && d <= '9') {
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

            if (count >= MAX_TOKENS) return -1;
            tokens[count++] = { TokenType::NUMBER, value + decimal };
            continue;

        } else {
            // Operators and parentheses
            TokenType type;
            switch (c) {
                case '+': type = TokenType::OP_PLUS;     break;
                case '-': type = TokenType::OP_MINUS;    break;
                case '*': type = TokenType::OP_MULTIPLY; break;
                case '/': type = TokenType::OP_DIVIDE;   break;
                case '(': type = TokenType::PAREN_OPEN;  break;
                case ')': type = TokenType::PAREN_CLOSE; break;
                case '=': return count;
                default:
                    printf("Unknown char: '%c' (ascii %d)\n", c, (int)c);
                    return -1;
            }

            if (count >= MAX_TOKENS) return -1;
            tokens[count++] = { type, 0.0f };
            i++;
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
        default: return 0;
    }
}

static bool isOperator(TokenType type) {
    return type == TokenType::OP_PLUS       ||
            type == TokenType::OP_MINUS     ||
            type == TokenType::OP_MULTIPLY  ||
            type == TokenType::OP_DIVIDE;
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
            while (opTop > 0 && isOperator(opStack[opTop - 1].type) && precedence(opStack[opTop - 1].type) >= precedence(token.type)) {
                if (outCount >= MAX_TOKENS) { return -1; }
                postfix[outCount++] = opStack[--opTop];
            }
            if (opTop >= MAX_STACK) { return -1; }
            opStack[opTop++] = token;
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
            if (!foundParen) { return -1; } // Mismatched parentheses
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
    static Token infix[MAX_TOKENS];
    static Token postfix[MAX_TOKENS];

    int infixCount = tokenize(expr, infix);
    printf("Tokenizer produced %d tokens\n", infixCount);
    if (infixCount < 0) return {false, 0, "Invalid expression"};
    if (infixCount == 0) return {false, 0, "Empty expression"};

    int postfixCount = shuntingYard(infix, infixCount, postfix);
    printf("Shunting Yard produced %d tokens\n", postfixCount);
    if (postfixCount < 0) return {false, 0, "Invalid expression"};

    return evalPostfix(postfix, postfixCount);
}