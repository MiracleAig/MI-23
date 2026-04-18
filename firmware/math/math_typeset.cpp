#include "math/math_typeset.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "graphics/font.h"
#include "hal/display.h"

namespace math_typeset {
namespace {

constexpr int MAX_NODES = 192;
constexpr int MAX_ROW_CHILDREN = 320;
constexpr int MAX_ROW_GAPS = 320;
constexpr int FONT_ASCENT = FONT_CHAR_HEIGHT - 1;
constexpr int FONT_DESCENT = 1;

struct Node {
    enum class Type {
        Text,
        Row,
        Fraction,
        Superscript,
        Parenthesized,
    } type;

    int textStart;
    int textLength;

    int left;
    int right;

    int childrenStart;
    int childrenCount;

    int spanStart;
    int spanEnd;
    int opPos;
};

struct ParseContext {
    const char* text;
    int length;
    int cursor;

    Node nodes[MAX_NODES];
    int nodeCount;

    int rowChildren[MAX_ROW_CHILDREN];
    int rowChildCount;

    bool parseError;
};

struct Box {
    enum class Type {
        Glyph,
        Row,
        Fraction,
        Superscript,
        Parenthesized,
    } type;

    LayoutMetrics metrics;

    int left;
    int right;

    int childrenStart;
    int childrenCount;
    int gapStart;

    int cachedSpanStart;
    int cachedSpanEnd;
};

struct LayoutContext {
    Box boxes[MAX_NODES];
    int boxCount;

    int rowChildren[MAX_ROW_CHILDREN];
    int rowChildCount;
    int rowGaps[MAX_ROW_GAPS];
    int rowGapCount;

    bool layoutError;
};

struct ParsedRoot {
    ParseContext parse;
    int rootNode;
    bool ok;
};

static bool isValueStart(char c) {
    const unsigned char uc = static_cast<unsigned char>(c);
    return std::isdigit(uc) || std::isalpha(uc) || c == '(' || c == '.' || uc == 128;
}

static int addNode(ParseContext& ctx, Node::Type type) {
    if (ctx.nodeCount >= MAX_NODES) {
        ctx.parseError = true;
        return -1;
    }

    const int index = ctx.nodeCount++;
    ctx.nodes[index] = Node{
        type,
        0,
        0,
        -1,
        -1,
        0,
        0,
        0,
        0,
        -1,
    };
    return index;
}

static int makeTextNode(ParseContext& ctx, int start, int length) {
    const int index = addNode(ctx, Node::Type::Text);
    if (index < 0) {
        return -1;
    }

    Node& node = ctx.nodes[index];
    node.textStart = start;
    node.textLength = length;
    node.spanStart = start;
    node.spanEnd = start + length;
    return index;
}

static int makeRowNode(ParseContext& ctx, const int* children, int childCount) {
    if (childCount <= 0) {
        return -1;
    }
    if (childCount == 1) {
        return children[0];
    }
    if (ctx.rowChildCount + childCount > MAX_ROW_CHILDREN) {
        ctx.parseError = true;
        return -1;
    }

    const int index = addNode(ctx, Node::Type::Row);
    if (index < 0) {
        return -1;
    }

    const int start = ctx.rowChildCount;
    for (int i = 0; i < childCount; i++) {
        ctx.rowChildren[ctx.rowChildCount++] = children[i];
    }

    Node& row = ctx.nodes[index];
    row.childrenStart = start;
    row.childrenCount = childCount;
    row.spanStart = ctx.nodes[children[0]].spanStart;
    row.spanEnd = ctx.nodes[children[childCount - 1]].spanEnd;
    return index;
}

static void skipSpaces(ParseContext& ctx) {
    while (ctx.cursor < ctx.length && ctx.text[ctx.cursor] == ' ') {
        ctx.cursor++;
    }
}

static bool atEnd(ParseContext& ctx) {
    skipSpaces(ctx);
    return ctx.cursor >= ctx.length;
}

static int parseExpression(ParseContext& ctx);

static int parsePrimary(ParseContext& ctx) {
    skipSpaces(ctx);
    if (ctx.cursor >= ctx.length) {
        ctx.parseError = true;
        return -1;
    }

    const int start = ctx.cursor;
    const char c = ctx.text[ctx.cursor];

    if (c == '(') {
        const int openPos = ctx.cursor;
        ctx.cursor++;
        const int inner = parseExpression(ctx);
        skipSpaces(ctx);
        if (ctx.cursor >= ctx.length || ctx.text[ctx.cursor] != ')') {
            ctx.parseError = true;
            return -1;
        }
        const int closePos = ctx.cursor;
        ctx.cursor++;

        const int nodeIndex = addNode(ctx, Node::Type::Parenthesized);
        if (nodeIndex < 0) {
            return -1;
        }

        Node& node = ctx.nodes[nodeIndex];
        node.left = inner;
        node.spanStart = openPos;
        node.spanEnd = closePos + 1;
        return nodeIndex;
    }

    if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
        while (ctx.cursor < ctx.length &&
               (std::isdigit(static_cast<unsigned char>(ctx.text[ctx.cursor])) ||
                ctx.text[ctx.cursor] == '.')) {
            ctx.cursor++;
        }
        return makeTextNode(ctx, start, ctx.cursor - start);
    }

    if (std::isalpha(static_cast<unsigned char>(c)) ||
        static_cast<unsigned char>(c) == 128) {
        while (ctx.cursor < ctx.length &&
               (std::isalpha(static_cast<unsigned char>(ctx.text[ctx.cursor])) ||
                std::isdigit(static_cast<unsigned char>(ctx.text[ctx.cursor])) ||
                static_cast<unsigned char>(ctx.text[ctx.cursor]) == 128)) {
            ctx.cursor++;
        }
        return makeTextNode(ctx, start, ctx.cursor - start);
    }

    if (c == '+' || c == '-') {
        ctx.cursor++;
        const int rhs = parsePrimary(ctx);
        int children[2] = { makeTextNode(ctx, start, 1), rhs };
        return makeRowNode(ctx, children, 2);
    }

    ctx.parseError = true;
    return -1;
}

static int parsePower(ParseContext& ctx) {
    int base = parsePrimary(ctx);
    skipSpaces(ctx);

    if (ctx.cursor < ctx.length && ctx.text[ctx.cursor] == '^') {
        const int opPos = ctx.cursor;
        ctx.cursor++;
        const int exponent = parsePower(ctx);

        const int nodeIndex = addNode(ctx, Node::Type::Superscript);
        if (nodeIndex < 0) {
            return -1;
        }

        Node& node = ctx.nodes[nodeIndex];
        node.left = base;
        node.right = exponent;
        node.opPos = opPos;
        node.spanStart = ctx.nodes[base].spanStart;
        node.spanEnd = ctx.nodes[exponent].spanEnd;
        return nodeIndex;
    }

    return base;
}

static int parseTerm(ParseContext& ctx) {
    int lhs = parsePower(ctx);

    while (!ctx.parseError) {
        skipSpaces(ctx);
        if (ctx.cursor >= ctx.length) {
            break;
        }

        const char c = ctx.text[ctx.cursor];
        if (c == '*') {
            const int opPos = ctx.cursor;
            ctx.cursor++;
            const int rhs = parsePower(ctx);
            const int op = makeTextNode(ctx, opPos, 1);
            int children[3] = {lhs, op, rhs};
            lhs = makeRowNode(ctx, children, 3);
            continue;
        }

        if (c == '/') {
            const int opPos = ctx.cursor;
            ctx.cursor++;
            const int rhs = parsePower(ctx);

            const int fraction = addNode(ctx, Node::Type::Fraction);
            if (fraction < 0) {
                return -1;
            }
            Node& node = ctx.nodes[fraction];
            node.left = lhs;
            node.right = rhs;
            node.opPos = opPos;
            node.spanStart = ctx.nodes[lhs].spanStart;
            node.spanEnd = ctx.nodes[rhs].spanEnd;
            lhs = fraction;
            continue;
        }

        if (isValueStart(c)) {
            const int rhs = parsePower(ctx);
            int children[2] = {lhs, rhs};
            lhs = makeRowNode(ctx, children, 2);
            continue;
        }

        break;
    }

    return lhs;
}

static int parseExpression(ParseContext& ctx) {
    int lhs = parseTerm(ctx);

    while (!ctx.parseError) {
        skipSpaces(ctx);
        if (ctx.cursor >= ctx.length) {
            break;
        }

        const char c = ctx.text[ctx.cursor];
        if (c != '+' && c != '-') {
            break;
        }

        const int opStart = ctx.cursor;
        ctx.cursor++;
        const int rhs = parseTerm(ctx);
        const int op = makeTextNode(ctx, opStart, 1);
        int children[3] = {lhs, op, rhs};
        lhs = makeRowNode(ctx, children, 3);
    }

    return lhs;
}

static ParsedRoot parse(const char* expression) {
    ParsedRoot parsed{};
    parsed.parse.text = expression;
    parsed.parse.length = static_cast<int>(std::strlen(expression));

    if (parsed.parse.length == 0) {
        parsed.ok = false;
        return parsed;
    }

    parsed.rootNode = parseExpression(parsed.parse);
    parsed.ok = !parsed.parse.parseError && parsed.rootNode >= 0 && atEnd(parsed.parse);
    return parsed;
}

static bool isOperatorText(const ParseContext& parseCtx, int nodeIndex) {
    const Node& node = parseCtx.nodes[nodeIndex];
    if (node.type != Node::Type::Text || node.textLength != 1) {
        return false;
    }

    const char c = parseCtx.text[node.textStart];
    return c == '+' || c == '-' || c == '*' || c == '/';
}

static int interAtomGap(const ParseContext& parseCtx, int leftNode, int rightNode) {
    const Node::Type leftType = parseCtx.nodes[leftNode].type;
    const Node::Type rightType = parseCtx.nodes[rightNode].type;

    if (isOperatorText(parseCtx, leftNode) || isOperatorText(parseCtx, rightNode)) {
        return 1;
    }

    if (leftType == Node::Type::Fraction || rightType == Node::Type::Fraction) {
        return 2;
    }

    if (leftType == Node::Type::Parenthesized || rightType == Node::Type::Parenthesized) {
        return 1;
    }

    return 0;
}

static int addBox(LayoutContext& layout, Box::Type type, int spanStart, int spanEnd) {
    if (layout.boxCount >= MAX_NODES) {
        layout.layoutError = true;
        return -1;
    }

    const int index = layout.boxCount++;
    layout.boxes[index] = Box{
        type,
        {0, 0, 0},
        -1,
        -1,
        0,
        0,
        0,
        spanStart,
        spanEnd,
    };
    return index;
}

static int layoutNode(const ParseContext& parseCtx, int nodeIndex, LayoutContext& layout);

static LayoutMetrics textMetrics(int length) {
    if (length <= 0) {
        return {0, FONT_ASCENT, FONT_DESCENT};
    }
    return {length * FONT_CHAR_ADVANCE, FONT_ASCENT, FONT_DESCENT};
}

static int layoutNode(const ParseContext& parseCtx, int nodeIndex, LayoutContext& layout) {
    if (nodeIndex < 0 || nodeIndex >= parseCtx.nodeCount) {
        layout.layoutError = true;
        return -1;
    }

    const Node& node = parseCtx.nodes[nodeIndex];

    if (node.type == Node::Type::Text) {
        const int box = addBox(layout, Box::Type::Glyph, node.spanStart, node.spanEnd);
        if (box < 0) {
            return -1;
        }
        layout.boxes[box].metrics = textMetrics(node.textLength);
        return box;
    }

    if (node.type == Node::Type::Row) {
        const int box = addBox(layout, Box::Type::Row, node.spanStart, node.spanEnd);
        if (box < 0) {
            return -1;
        }

        const int childStart = layout.rowChildCount;
        const int gapStart = layout.rowGapCount;
        int width = 0;
        int ascent = 0;
        int descent = 0;

        for (int i = 0; i < node.childrenCount; i++) {
            const int childNode = parseCtx.rowChildren[node.childrenStart + i];
            const int childBox = layoutNode(parseCtx, childNode, layout);
            if (childBox < 0) {
                return -1;
            }

            if (layout.rowChildCount >= MAX_ROW_CHILDREN) {
                layout.layoutError = true;
                return -1;
            }
            layout.rowChildren[layout.rowChildCount++] = childBox;

            const LayoutMetrics childMetrics = layout.boxes[childBox].metrics;
            width += childMetrics.width;
            ascent = std::max(ascent, childMetrics.ascent);
            descent = std::max(descent, childMetrics.descent);

            if (i < node.childrenCount - 1) {
                if (layout.rowGapCount >= MAX_ROW_GAPS) {
                    layout.layoutError = true;
                    return -1;
                }
                const int nextNode = parseCtx.rowChildren[node.childrenStart + i + 1];
                const int gap = interAtomGap(parseCtx, childNode, nextNode);
                layout.rowGaps[layout.rowGapCount++] = gap;
                width += gap;
            }
        }

        layout.boxes[box].childrenStart = childStart;
        layout.boxes[box].childrenCount = node.childrenCount;
        layout.boxes[box].gapStart = gapStart;
        layout.boxes[box].metrics = {width, ascent, descent};
        return box;
    }

    if (node.type == Node::Type::Fraction) {
        const int numerator = layoutNode(parseCtx, node.left, layout);
        const int denominator = layoutNode(parseCtx, node.right, layout);

        const int box = addBox(layout, Box::Type::Fraction, node.spanStart, node.spanEnd);
        if (box < 0) {
            return -1;
        }

        const LayoutMetrics numMetrics = layout.boxes[numerator].metrics;
        const LayoutMetrics denMetrics = layout.boxes[denominator].metrics;
        const int numHeight = numMetrics.ascent + numMetrics.descent;
        const int denHeight = denMetrics.ascent + denMetrics.descent;
        const int barWidth = std::max(numMetrics.width, denMetrics.width) + 4;

        layout.boxes[box].left = numerator;
        layout.boxes[box].right = denominator;
        layout.boxes[box].metrics = {
            barWidth,
            numHeight + 2,
            2 + denHeight,
        };
        return box;
    }

    if (node.type == Node::Type::Superscript) {
        const int base = layoutNode(parseCtx, node.left, layout);
        const int exponent = layoutNode(parseCtx, node.right, layout);

        const int box = addBox(layout, Box::Type::Superscript, node.spanStart, node.spanEnd);
        if (box < 0) {
            return -1;
        }

        const LayoutMetrics baseMetrics = layout.boxes[base].metrics;
        const LayoutMetrics expMetrics = layout.boxes[exponent].metrics;
        const int scriptShift = std::max(2, baseMetrics.ascent / 2);

        layout.boxes[box].left = base;
        layout.boxes[box].right = exponent;
        layout.boxes[box].metrics = {
            baseMetrics.width + expMetrics.width,
            std::max(baseMetrics.ascent, expMetrics.ascent + scriptShift),
            std::max(baseMetrics.descent, expMetrics.descent - scriptShift),
        };
        return box;
    }

    if (node.type == Node::Type::Parenthesized) {
        const int inner = layoutNode(parseCtx, node.left, layout);

        const int box = addBox(layout, Box::Type::Parenthesized, node.spanStart, node.spanEnd);
        if (box < 0) {
            return -1;
        }

        const LayoutMetrics innerMetrics = layout.boxes[inner].metrics;
        const int delimiterWidth = FONT_CHAR_ADVANCE;
        layout.boxes[box].left = inner;
        layout.boxes[box].metrics = {
            innerMetrics.width + delimiterWidth * 2,
            std::max(innerMetrics.ascent, FONT_ASCENT),
            std::max(innerMetrics.descent, FONT_DESCENT),
        };
        return box;
    }

    layout.layoutError = true;
    return -1;
}

static void drawChar(Display& display, unsigned char c, int x, int y, uint16_t color) {
    const uint8_t* glyph = &FONT_DATA[c * FONT_CHAR_WIDTH];
    for (int col = 0; col < FONT_CHAR_WIDTH; col++) {
        const uint8_t columnBits = glyph[col];
        for (int row = 0; row < FONT_CHAR_HEIGHT; row++) {
            if ((columnBits >> row) & 1U) {
                display.drawPixel(x + col, y + row, color);
            }
        }
    }
}

static void drawTallParenthesis(Display& display,
                                bool left,
                                int x,
                                int top,
                                int height,
                                uint16_t color) {
    const int stemX = left ? x + FONT_CHAR_WIDTH - 2 : x + 1;
    if (height <= FONT_CHAR_HEIGHT) {
        drawChar(display,
                 static_cast<unsigned char>(left ? '(' : ')'),
                 x,
                 top,
                 color);
        return;
    }

    display.drawPixel(stemX, top, color);
    display.drawPixel(stemX + (left ? 1 : -1), top + 1, color);

    const int stemTop = top + 2;
    const int stemHeight = std::max(0, height - 4);
    if (stemHeight > 0) {
        display.drawRect(stemX, stemTop, 1, stemHeight, color);
    }

    const int bottom = top + height - 1;
    display.drawPixel(stemX + (left ? 1 : -1), bottom - 1, color);
    display.drawPixel(stemX, bottom, color);
}

static void drawTextSlice(Display& display,
                          const char* text,
                          int start,
                          int length,
                          int x,
                          int baselineY,
                          uint16_t color) {
    int cursorX = x;
    const int top = baselineY - FONT_ASCENT;

    for (int i = 0; i < length; i++) {
        const unsigned char c = static_cast<unsigned char>(text[start + i]);
        drawChar(display, c, cursorX, top, color);
        cursorX += FONT_CHAR_ADVANCE;
    }
}

static void drawBoxRecursive(const ParseContext& parseCtx,
                             const LayoutContext& layout,
                             int nodeIndex,
                             int boxIndex,
                             Display& display,
                             int x,
                             int baselineY,
                             uint16_t color) {
    const Node& node = parseCtx.nodes[nodeIndex];
    const Box& box = layout.boxes[boxIndex];

    if (node.type == Node::Type::Text) {
        drawTextSlice(display,
                      parseCtx.text,
                      node.textStart,
                      node.textLength,
                      x,
                      baselineY,
                      color);
        return;
    }

    if (node.type == Node::Type::Row) {
        int cursorX = x;
        for (int i = 0; i < box.childrenCount; i++) {
            const int childNode = parseCtx.rowChildren[node.childrenStart + i];
            const int childBox = layout.rowChildren[box.childrenStart + i];

            drawBoxRecursive(parseCtx, layout, childNode, childBox,
                             display, cursorX, baselineY, color);
            cursorX += layout.boxes[childBox].metrics.width;
            if (i < box.childrenCount - 1) {
                cursorX += layout.rowGaps[box.gapStart + i];
            }
        }
        return;
    }

    if (node.type == Node::Type::Fraction) {
        const int numeratorNode = node.left;
        const int denominatorNode = node.right;
        const int numeratorBox = box.left;
        const int denominatorBox = box.right;

        const LayoutMetrics numMetrics = layout.boxes[numeratorBox].metrics;
        const LayoutMetrics denMetrics = layout.boxes[denominatorBox].metrics;
        const int numHeight = numMetrics.ascent + numMetrics.descent;

        const int numeratorX = x + (box.metrics.width - numMetrics.width) / 2;
        const int denominatorX = x + (box.metrics.width - denMetrics.width) / 2;

        const int numeratorTop = baselineY - 2 - numHeight;
        const int numeratorBaseline = numeratorTop + numMetrics.ascent;
        const int denominatorTop = baselineY + 2;
        const int denominatorBaseline = denominatorTop + denMetrics.ascent;

        drawBoxRecursive(parseCtx, layout, numeratorNode, numeratorBox,
                         display, numeratorX, numeratorBaseline, color);
        display.drawRect(x, baselineY, box.metrics.width, 1, color);
        drawBoxRecursive(parseCtx, layout, denominatorNode, denominatorBox,
                         display, denominatorX, denominatorBaseline, color);
        return;
    }

    if (node.type == Node::Type::Superscript) {
        const int baseNode = node.left;
        const int expNode = node.right;
        const int baseBox = box.left;
        const int expBox = box.right;

        const LayoutMetrics baseMetrics = layout.boxes[baseBox].metrics;
        const int scriptShift = std::max(2, baseMetrics.ascent / 2);

        drawBoxRecursive(parseCtx, layout, baseNode, baseBox,
                         display, x, baselineY, color);
        drawBoxRecursive(parseCtx, layout, expNode, expBox,
                         display, x + baseMetrics.width, baselineY - scriptShift, color);
        return;
    }

    if (node.type == Node::Type::Parenthesized) {
        const int innerNode = node.left;
        const int innerBox = box.left;
        const int delimiterWidth = FONT_CHAR_ADVANCE;

        const int top = baselineY - box.metrics.ascent;
        const int height = box.metrics.ascent + box.metrics.descent;

        drawTallParenthesis(display, true, x, top, height, color);
        drawBoxRecursive(parseCtx, layout, innerNode, innerBox,
                         display, x + delimiterWidth, baselineY, color);
        drawTallParenthesis(display,
                            false,
                            x + delimiterWidth + layout.boxes[innerBox].metrics.width,
                            top,
                            height,
                            color);
    }
}

static bool buildLayout(const char* expression,
                        ParseContext& parseCtx,
                        int& rootNode,
                        LayoutContext& layout,
                        int& rootBox) {
    ParsedRoot parsed = parse(expression);
    if (!parsed.ok) {
        return false;
    }

    parseCtx = parsed.parse;
    rootNode = parsed.rootNode;

    layout = {};
    rootBox = layoutNode(parseCtx, rootNode, layout);
    if (layout.layoutError || rootBox < 0) {
        return false;
    }

    return true;
}

static int clampCursor(int cursor, int expressionLength) {
    if (cursor < 0) {
        return 0;
    }
    if (cursor > expressionLength) {
        return expressionLength;
    }
    return cursor;
}

static int nearestCursorInRange(const char* expression,
                                int rangeStart,
                                int rangeEnd,
                                int preferredX) {
    int best = rangeStart;
    int bestDistance = 1 << 30;

    for (int i = rangeStart; i <= rangeEnd; i++) {
        const int x = measurePrefixWidth(expression, i);
        const int distance = std::abs(x - preferredX);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

struct FractionHit {
    int fractionNode;
    bool inNumerator;
    bool inDenominator;
};

static bool cursorInSpan(const Node& node, int cursor) {
    return cursor >= node.spanStart && cursor <= node.spanEnd;
}

static void findFractionAtCursor(const ParseContext& parseCtx,
                                 int nodeIndex,
                                 int cursor,
                                 FractionHit& hit) {
    const Node& node = parseCtx.nodes[nodeIndex];
    if (!cursorInSpan(node, cursor)) {
        return;
    }

    if (node.type == Node::Type::Fraction) {
        const Node& num = parseCtx.nodes[node.left];
        const Node& den = parseCtx.nodes[node.right];

        const bool inNum = cursor <= node.opPos && cursorInSpan(num, cursor);
        const bool inDen = cursor >= node.opPos + 1 && cursorInSpan(den, cursor);
        if (inNum || inDen) {
            hit.fractionNode = nodeIndex;
            hit.inNumerator = inNum;
            hit.inDenominator = inDen;
        }
    }

    if (node.type == Node::Type::Row) {
        for (int i = 0; i < node.childrenCount; i++) {
            const int child = parseCtx.rowChildren[node.childrenStart + i];
            findFractionAtCursor(parseCtx, child, cursor, hit);
        }
    } else if (node.type == Node::Type::Fraction ||
               node.type == Node::Type::Superscript) {
        findFractionAtCursor(parseCtx, node.left, cursor, hit);
        findFractionAtCursor(parseCtx, node.right, cursor, hit);
    } else if (node.type == Node::Type::Parenthesized) {
        findFractionAtCursor(parseCtx, node.left, cursor, hit);
    }
}

static int moveCursorByStructure(const char* expression,
                                 const ParseContext& parseCtx,
                                 int rootNode,
                                 int cursor,
                                 CursorMove move) {
    const int length = static_cast<int>(std::strlen(expression));
    cursor = clampCursor(cursor, length);

    FractionHit hit{-1, false, false};
    findFractionAtCursor(parseCtx, rootNode, cursor, hit);

    if (move == CursorMove::Up || move == CursorMove::Down) {
        if (hit.fractionNode >= 0) {
            const Node& fraction = parseCtx.nodes[hit.fractionNode];
            const Node& numerator = parseCtx.nodes[fraction.left];
            const Node& denominator = parseCtx.nodes[fraction.right];
            const int preferredX = measurePrefixWidth(expression, cursor);

            if (move == CursorMove::Up && hit.inDenominator) {
                return nearestCursorInRange(expression,
                                            numerator.spanStart,
                                            numerator.spanEnd,
                                            preferredX);
            }
            if (move == CursorMove::Down && hit.inNumerator) {
                return nearestCursorInRange(expression,
                                            denominator.spanStart,
                                            denominator.spanEnd,
                                            preferredX);
            }
        }
        return cursor;
    }

    if (move == CursorMove::Left) {
        if (hit.fractionNode >= 0) {
            const Node& fraction = parseCtx.nodes[hit.fractionNode];
            const Node& numerator = parseCtx.nodes[fraction.left];
            const Node& denominator = parseCtx.nodes[fraction.right];
            if (hit.inNumerator && cursor <= numerator.spanStart) {
                return fraction.spanStart;
            }
            if (hit.inDenominator && cursor <= denominator.spanStart) {
                return fraction.spanStart;
            }
        }
        return clampCursor(cursor - 1, length);
    }

    if (move == CursorMove::Right) {
        if (hit.fractionNode >= 0) {
            const Node& fraction = parseCtx.nodes[hit.fractionNode];
            const Node& numerator = parseCtx.nodes[fraction.left];
            const Node& denominator = parseCtx.nodes[fraction.right];
            if (hit.inNumerator && cursor >= numerator.spanEnd) {
                return fraction.spanEnd;
            }
            if (hit.inDenominator && cursor >= denominator.spanEnd) {
                return fraction.spanEnd;
            }
        }
        return clampCursor(cursor + 1, length);
    }

    return cursor;
}

} // namespace

bool measure(const char* expression, LayoutMetrics& outMetrics) {
    ParseContext parseCtx{};
    LayoutContext layout{};
    int rootNode = -1;
    int rootBox = -1;

    if (!buildLayout(expression, parseCtx, rootNode, layout, rootBox)) {
        return false;
    }

    outMetrics = layout.boxes[rootBox].metrics;
    return true;
}

int measurePrefixWidth(const char* expression, int prefixLength) {
    if (prefixLength <= 0) {
        return 0;
    }

    char prefix[128] = {};
    int copied = 0;
    while (copied < prefixLength && copied < 127 && expression[copied] != '\0') {
        prefix[copied] = expression[copied];
        copied++;
    }
    prefix[copied] = '\0';

    LayoutMetrics metrics{};
    if (measure(prefix, metrics)) {
        return metrics.width;
    }

    return copied * FONT_CHAR_ADVANCE;
}

int moveCursor(const char* expression, int cursorPosition, CursorMove move) {
    ParsedRoot parsed = parse(expression);
    const int length = static_cast<int>(std::strlen(expression));

    if (!parsed.ok) {
        if (move == CursorMove::Left) {
            return clampCursor(cursorPosition - 1, length);
        }
        if (move == CursorMove::Right) {
            return clampCursor(cursorPosition + 1, length);
        }
        return clampCursor(cursorPosition, length);
    }

    return moveCursorByStructure(expression,
                                 parsed.parse,
                                 parsed.rootNode,
                                 cursorPosition,
                                 move);
}

bool draw(const char* expression,
          Display& display,
          int x,
          int baselineY,
          uint16_t color,
          LayoutMetrics* outMetrics) {
    ParseContext parseCtx{};
    LayoutContext layout{};
    int rootNode = -1;
    int rootBox = -1;

    if (!buildLayout(expression, parseCtx, rootNode, layout, rootBox)) {
        return false;
    }

    drawBoxRecursive(parseCtx, layout, rootNode, rootBox,
                     display, x, baselineY, color);

    if (outMetrics != nullptr) {
        *outMetrics = layout.boxes[rootBox].metrics;
    }
    return true;
}

} // namespace math_typeset
