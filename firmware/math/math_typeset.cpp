#include "math/math_typeset.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstring>

#include "graphics/font.h"
#include "hal/display.h"

namespace math_typeset {
namespace {

constexpr int MAX_NODES = 192;
constexpr int MAX_ROW_CHILDREN = 320;
constexpr int FONT_ASCENT = FONT_CHAR_HEIGHT - 1;
constexpr int FONT_DESCENT = 1;
constexpr int FRACTION_SIDE_BEARING = 2;
constexpr int FRACTION_BAR_PADDING = 4;

struct Node {
    enum class Type {
        Text,
        Row,
        Fraction,
        Root,
        Superscript,
        Parenthesized,
    } type;

    int textStart;
    int textLength;

    int left;
    int right;

    int childrenStart;
    int childrenCount;
};

struct Context {
    const char* text;
    int length;
    int cursor;

    Node nodes[MAX_NODES];
    int nodeCount;

    int rowChildren[MAX_ROW_CHILDREN];
    int rowChildCount;

    bool parseError;
};

struct ParsedExpression {
    int root;
    bool ok;
};

struct Box {
    enum class Type {
        Glyph,
        Row,
        Fraction,
        Root,
        Superscript,
        Parenthesized,
    } type;

    LayoutMetrics metrics;

    int left;
    int right;

    int childrenStart;
    int childrenCount;
};

struct LayoutContext {
    Box boxes[MAX_NODES];
    int boxCount;

    int rowChildren[MAX_ROW_CHILDREN];
    int rowChildCount;

    bool layoutError;
};

static bool isValueStart(char c) {
    const unsigned char uc = static_cast<unsigned char>(c);
    return std::isdigit(static_cast<unsigned char>(c)) ||
           std::isalpha(static_cast<unsigned char>(c)) ||
           c == '(' || c == '.' || uc == 128;
}

static bool canAllocateNode(Context& ctx) {
    return ctx.nodeCount < MAX_NODES;
}

static int addNode(Context& ctx, Node::Type type) {
    if (!canAllocateNode(ctx)) {
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
    };
    return index;
}

static int makeTextNode(Context& ctx, int start, int length) {
    const int index = addNode(ctx, Node::Type::Text);
    if (index < 0) {
        return -1;
    }
    ctx.nodes[index].textStart = start;
    ctx.nodes[index].textLength = length;
    return index;
}

static int makeRowNode(Context& ctx, const int* children, int childCount) {
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

    const int rowStart = ctx.rowChildCount;
    for (int i = 0; i < childCount; i++) {
        ctx.rowChildren[ctx.rowChildCount++] = children[i];
    }

    ctx.nodes[index].childrenStart = rowStart;
    ctx.nodes[index].childrenCount = childCount;
    return index;
}

static void skipSpaces(Context& ctx) {
    while (ctx.cursor < ctx.length && ctx.text[ctx.cursor] == ' ') {
        ctx.cursor++;
    }
}

static bool atEnd(Context& ctx) {
    skipSpaces(ctx);
    return ctx.cursor >= ctx.length;
}

static int parseExpression(Context& ctx);

static bool matchWordAt(const Context& ctx, const char* word, int length) {
    if (ctx.cursor + length > ctx.length) {
        return false;
    }
    return std::strncmp(&ctx.text[ctx.cursor], word, length) == 0;
}

static int parseExpressionOrEmptyUntil(Context& ctx, char terminator) {
    skipSpaces(ctx);
    if (ctx.cursor < ctx.length && ctx.text[ctx.cursor] == terminator) {
        return makeTextNode(ctx, ctx.cursor, 0);
    }

    return parseExpression(ctx);
}

static int parsePrimary(Context& ctx) {
    skipSpaces(ctx);
    if (ctx.cursor >= ctx.length) {
        ctx.parseError = true;
        return -1;
    }

    const int start = ctx.cursor;
    const char c = ctx.text[ctx.cursor];

    if (matchWordAt(ctx, "sqrt", 4) &&
        ctx.cursor + 4 < ctx.length &&
        ctx.text[ctx.cursor + 4] == '(') {
        ctx.cursor += 5;
        const int radicand = parseExpressionOrEmptyUntil(ctx, ')');
        skipSpaces(ctx);
        if (ctx.cursor >= ctx.length || ctx.text[ctx.cursor] != ')') {
            ctx.parseError = true;
            return -1;
        }
        ctx.cursor++;

        const int root = addNode(ctx, Node::Type::Root);
        if (root < 0) {
            return -1;
        }
        ctx.nodes[root].textStart = start;
        ctx.nodes[root].left = -1;
        ctx.nodes[root].right = radicand;
        return root;
    }

    if (matchWordAt(ctx, "root", 4) &&
        ctx.cursor + 4 < ctx.length &&
        ctx.text[ctx.cursor + 4] == '(') {
        ctx.cursor += 5;
        const int index = parseExpressionOrEmptyUntil(ctx, ',');
        skipSpaces(ctx);
        if (ctx.cursor >= ctx.length || ctx.text[ctx.cursor] != ',') {
            ctx.parseError = true;
            return -1;
        }
        ctx.cursor++;
        const int radicand = parseExpressionOrEmptyUntil(ctx, ')');
        skipSpaces(ctx);
        if (ctx.cursor >= ctx.length || ctx.text[ctx.cursor] != ')') {
            ctx.parseError = true;
            return -1;
        }
        ctx.cursor++;

        const int root = addNode(ctx, Node::Type::Root);
        if (root < 0) {
            return -1;
        }
        ctx.nodes[root].textStart = start;
        ctx.nodes[root].left = index;
        ctx.nodes[root].right = radicand;
        return root;
    }

    if (c == '(') {
        ctx.cursor++;
        const int inner = parseExpression(ctx);
        skipSpaces(ctx);
        if (ctx.cursor >= ctx.length || ctx.text[ctx.cursor] != ')') {
            ctx.parseError = true;
            return -1;
        }
        ctx.cursor++;

        const int group = addNode(ctx, Node::Type::Parenthesized);
        if (group < 0) {
            return -1;
        }
        ctx.nodes[group].left = inner;
        return group;
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
        int children[2] = {
            makeTextNode(ctx, start, 1),
            rhs,
        };
        return makeRowNode(ctx, children, 2);
    }

    ctx.parseError = true;
    return -1;
}

static int parsePower(Context& ctx) {
    int base = parsePrimary(ctx);
    skipSpaces(ctx);

    if (ctx.cursor < ctx.length && ctx.text[ctx.cursor] == '^') {
        ctx.cursor++;
        int exponent = parsePower(ctx);
        const int node = addNode(ctx, Node::Type::Superscript);
        if (node < 0) {
            return -1;
        }
        ctx.nodes[node].left = base;
        ctx.nodes[node].right = exponent;
        return node;
    }

    return base;
}

static int parseTerm(Context& ctx) {
    int lhs = parsePower(ctx);
    while (!ctx.parseError) {
        skipSpaces(ctx);
        if (ctx.cursor >= ctx.length) {
            break;
        }

        const char c = ctx.text[ctx.cursor];
        if (c == '*') {
            ctx.cursor++;
            const int rhs = parsePower(ctx);
            const int op = makeTextNode(ctx, ctx.cursor - 1, 1);
            int children[3] = { lhs, op, rhs };
            lhs = makeRowNode(ctx, children, 3);
            continue;
        }

        if (c == '/') {
            const int slashIndex = ctx.cursor;
            ctx.cursor++;
            const int rhs = parsePower(ctx);
            const int fraction = addNode(ctx, Node::Type::Fraction);
            if (fraction < 0) {
                return -1;
            }
            ctx.nodes[fraction].textStart = slashIndex;
            ctx.nodes[fraction].left = lhs;
            ctx.nodes[fraction].right = rhs;
            lhs = fraction;
            continue;
        }

        if (isValueStart(c)) {
            const int rhs = parsePower(ctx);
            int children[2] = { lhs, rhs };
            lhs = makeRowNode(ctx, children, 2);
            continue;
        }

        break;
    }

    return lhs;
}

static int parseExpression(Context& ctx) {
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
        int children[3] = { lhs, op, rhs };
        lhs = makeRowNode(ctx, children, 3);
    }

    return lhs;
}

static bool canAllocateBox(LayoutContext& layout) {
    return layout.boxCount < MAX_NODES;
}

static int layoutNode(const Context& parseCtx, int nodeIndex, LayoutContext& layout);

static int addBox(LayoutContext& layout, Box::Type type) {
    if (!canAllocateBox(layout)) {
        layout.layoutError = true;
        return -1;
    }

    const int index = layout.boxCount++;
    layout.boxes[index] = Box{type, {0, 0, 0}, -1, -1, 0, 0};
    return index;
}

static LayoutMetrics textMetrics(int length) {
    if (length <= 0) {
        return {0, FONT_ASCENT, FONT_DESCENT};
    }
    return {length * FONT_CHAR_ADVANCE, FONT_ASCENT, FONT_DESCENT};
}

static LayoutMetrics scaleMetrics(LayoutMetrics metrics, float scale) {
    return {
        scaleLength(metrics.width, scale),
        scaleLength(metrics.ascent, scale),
        scaleLength(metrics.descent, scale),
    };
}

static bool needsRowGap(Box::Type type) {
    return type == Box::Type::Fraction;
}

static int rowGapBetween(Box::Type left, Box::Type right) {
    return (needsRowGap(left) || needsRowGap(right)) ? FRACTION_SIDE_BEARING : 0;
}

static int nodeTextStart(const Context& parseCtx, int nodeIndex) {
    if (nodeIndex < 0 || nodeIndex >= parseCtx.nodeCount) {
        return parseCtx.length;
    }

    const Node& node = parseCtx.nodes[nodeIndex];
    if (node.type == Node::Type::Text) {
        return node.textStart;
    }
    if (node.type == Node::Type::Row) {
        int start = parseCtx.length;
        for (int i = 0; i < node.childrenCount; i++) {
            const int child = parseCtx.rowChildren[node.childrenStart + i];
            start = std::min(start, nodeTextStart(parseCtx, child));
        }
        return start;
    }
    if (node.type == Node::Type::Parenthesized) {
        return nodeTextStart(parseCtx, node.left);
    }
    if (node.type == Node::Type::Root) {
        if (node.left >= 0) {
            return std::min(nodeTextStart(parseCtx, node.left),
                            nodeTextStart(parseCtx, node.right));
        }
        return nodeTextStart(parseCtx, node.right);
    }
    if (node.type == Node::Type::Fraction ||
        node.type == Node::Type::Superscript) {
        return std::min(nodeTextStart(parseCtx, node.left),
                        nodeTextStart(parseCtx, node.right));
    }
    return parseCtx.length;
}

static int nodeTextEnd(const Context& parseCtx, int nodeIndex) {
    if (nodeIndex < 0 || nodeIndex >= parseCtx.nodeCount) {
        return 0;
    }

    const Node& node = parseCtx.nodes[nodeIndex];
    if (node.type == Node::Type::Text) {
        return node.textStart + node.textLength;
    }
    if (node.type == Node::Type::Row) {
        int end = 0;
        for (int i = 0; i < node.childrenCount; i++) {
            const int child = parseCtx.rowChildren[node.childrenStart + i];
            end = std::max(end, nodeTextEnd(parseCtx, child));
        }
        return end;
    }
    if (node.type == Node::Type::Parenthesized) {
        return nodeTextEnd(parseCtx, node.left);
    }
    if (node.type == Node::Type::Root) {
        if (node.left >= 0) {
            return std::max(nodeTextEnd(parseCtx, node.left),
                            nodeTextEnd(parseCtx, node.right));
        }
        return nodeTextEnd(parseCtx, node.right);
    }
    if (node.type == Node::Type::Fraction ||
        node.type == Node::Type::Superscript) {
        return std::max(nodeTextEnd(parseCtx, node.left),
                        nodeTextEnd(parseCtx, node.right));
    }
    return 0;
}

static int layoutNode(const Context& parseCtx, int nodeIndex, LayoutContext& layout) {
    if (nodeIndex < 0 || nodeIndex >= parseCtx.nodeCount) {
        layout.layoutError = true;
        return -1;
    }

    const Node& node = parseCtx.nodes[nodeIndex];

    if (node.type == Node::Type::Text) {
        const int box = addBox(layout, Box::Type::Glyph);
        if (box < 0) {
            return -1;
        }
        layout.boxes[box].metrics = textMetrics(node.textLength);
        return box;
    }

    if (node.type == Node::Type::Row) {
        const int box = addBox(layout, Box::Type::Row);
        if (box < 0) {
            return -1;
        }

        const int childStart = layout.rowChildCount;
        int width = 0;
        int ascent = 0;
        int descent = 0;

        int previousChildBox = -1;
        for (int i = 0; i < node.childrenCount; i++) {
            const int childNodeIndex = parseCtx.rowChildren[node.childrenStart + i];
            const int childBox = layoutNode(parseCtx, childNodeIndex, layout);
            if (childBox < 0) {
                return -1;
            }
            if (layout.rowChildCount >= MAX_ROW_CHILDREN) {
                layout.layoutError = true;
                return -1;
            }
            layout.rowChildren[layout.rowChildCount++] = childBox;
            const LayoutMetrics childMetrics = layout.boxes[childBox].metrics;
            if (previousChildBox >= 0) {
                width += rowGapBetween(layout.boxes[previousChildBox].type,
                                       layout.boxes[childBox].type);
            }
            width += childMetrics.width;
            ascent = std::max(ascent, childMetrics.ascent);
            descent = std::max(descent, childMetrics.descent);
            previousChildBox = childBox;
        }

        layout.boxes[box].childrenStart = childStart;
        layout.boxes[box].childrenCount = node.childrenCount;
        layout.boxes[box].metrics = {width, ascent, descent};
        return box;
    }

    if (node.type == Node::Type::Fraction) {
        const int numerator = layoutNode(parseCtx, node.left, layout);
        const int denominator = layoutNode(parseCtx, node.right, layout);

        const int box = addBox(layout, Box::Type::Fraction);
        if (box < 0) {
            return -1;
        }

        const LayoutMetrics numeratorMetrics = layout.boxes[numerator].metrics;
        const LayoutMetrics denominatorMetrics = layout.boxes[denominator].metrics;
        const int numeratorHeight = numeratorMetrics.ascent + numeratorMetrics.descent;
        const int denominatorHeight = denominatorMetrics.ascent + denominatorMetrics.descent;
        const int barWidth = std::max(numeratorMetrics.width, denominatorMetrics.width)
            + FRACTION_BAR_PADDING;

        layout.boxes[box].left = numerator;
        layout.boxes[box].right = denominator;
        layout.boxes[box].metrics = {
            barWidth,
            numeratorHeight + 1,
            1 + denominatorHeight,
        };
        return box;
    }

    if (node.type == Node::Type::Root) {
        const int index = (node.left >= 0) ? layoutNode(parseCtx, node.left, layout) : -1;
        const int radicand = layoutNode(parseCtx, node.right, layout);

        const int box = addBox(layout, Box::Type::Root);
        if (box < 0) {
            return -1;
        }

        const LayoutMetrics radicandMetrics = layout.boxes[radicand].metrics;
        const LayoutMetrics indexMetrics = (index >= 0)
            ? layout.boxes[index].metrics
            : LayoutMetrics{0, 0, 0};
        const int radicalWidth = FONT_CHAR_ADVANCE + 2;
        const int indexWidth = (index >= 0) ? std::max(0, indexMetrics.width - 2) : 0;

        layout.boxes[box].left = index;
        layout.boxes[box].right = radicand;
        layout.boxes[box].metrics = {
            indexWidth + radicalWidth + radicandMetrics.width + 2,
            std::max(radicandMetrics.ascent + 2,
                     indexMetrics.ascent + indexMetrics.descent + 2),
            radicandMetrics.descent,
        };
        return box;
    }

    if (node.type == Node::Type::Superscript) {
        const int base = layoutNode(parseCtx, node.left, layout);
        const int exponent = layoutNode(parseCtx, node.right, layout);

        const int box = addBox(layout, Box::Type::Superscript);
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

        const int box = addBox(layout, Box::Type::Parenthesized);
        if (box < 0) {
            return -1;
        }

        const LayoutMetrics innerMetrics = layout.boxes[inner].metrics;
        const int parenWidth = FONT_CHAR_ADVANCE;
        layout.boxes[box].left = inner;
        layout.boxes[box].metrics = {
            innerMetrics.width + parenWidth * 2,
            std::max(innerMetrics.ascent, FONT_ASCENT),
            std::max(innerMetrics.descent, FONT_DESCENT),
        };
        return box;
    }

    layout.layoutError = true;
    return -1;
}

static void drawCharScaled(Display& display,
                           unsigned char c,
                           int x,
                           int y,
                           uint16_t color,
                           float scale) {
    const uint8_t* glyph = &FONT_DATA[c * FONT_CHAR_WIDTH];
    for (int col = 0; col < FONT_CHAR_WIDTH; col++) {
        const uint8_t columnBits = glyph[col];
        for (int row = 0; row < FONT_CHAR_HEIGHT; row++) {
            if ((columnBits >> row) & 1U) {
                const int scaledX = x + scaleLength(col, scale);
                const int scaledY = y + scaleLength(row, scale);
                display.drawPixel(scaledX, scaledY, color);
            }
        }
    }
}

static void drawChar(Display& display, unsigned char c, int x, int y, uint16_t color) {
    drawCharScaled(display, c, x, y, color, 1.0f);
}

static void drawTallParen(Display& display,
                          bool leftParen,
                          int x,
                          int top,
                          int height,
                          uint16_t color,
                          float scale) {
    if (height <= scaleLength(FONT_CHAR_HEIGHT, scale) + 1) {
        drawCharScaled(display,
                       static_cast<unsigned char>(leftParen ? '(' : ')'),
                       x,
                       top,
                       color,
                       scale);
        return;
    }

    const int strokeX = x + scaleLength(leftParen ? 1 : FONT_CHAR_WIDTH - 2, scale);
    const int capInnerX = x + scaleLength(leftParen ? 2 : FONT_CHAR_WIDTH - 3, scale);
    const int bottom = top + height - 1;
    display.drawRect(strokeX, top + 1, 1, std::max(1, height - 2), color);
    display.drawPixel(capInnerX, top, color);
    display.drawPixel(capInnerX, bottom, color);
}

static void drawRootSymbol(Display& display,
                           int x,
                           int top,
                           int baselineY,
                           int bottom,
                           int width,
                           uint16_t color) {
    const int hookBottom = std::max(bottom, baselineY);
    const int stemX = x + 4;
    const int barY = top;
    display.drawPixel(x, hookBottom - 2, color);
    display.drawPixel(x + 1, hookBottom - 1, color);
    display.drawPixel(x + 2, hookBottom, color);
    display.drawPixel(x + 3, hookBottom - 1, color);
    display.drawRect(stemX, barY + 1, 1, std::max(1, hookBottom - barY), color);
    display.drawRect(stemX, barY, std::max(1, width - (stemX - x)), 1, color);
}

static void drawTextSlice(Display& display,
                          const char* text,
                          int start,
                          int length,
                          int x,
                          int baselineY,
                          uint16_t color,
                          float scale) {
    int cursorX = x;
    const int top = baselineY - scaleLength(FONT_ASCENT, scale);

    for (int i = 0; i < length; i++) {
        const unsigned char c = static_cast<unsigned char>(text[start + i]);
        drawCharScaled(display, c, cursorX, top, color, scale);
        cursorX += scaleLength(FONT_CHAR_ADVANCE, scale);
    }
}

static void drawBoxRecursive(const Context& parseCtx,
                             const LayoutContext& layout,
                             int nodeIndex,
                             int boxIndex,
                             Display& display,
                             int x,
                             int baselineY,
                             uint16_t color,
                             float scale) {
    const Node& node = parseCtx.nodes[nodeIndex];
    const Box& box = layout.boxes[boxIndex];

    if (node.type == Node::Type::Text) {
        drawTextSlice(display,
                      parseCtx.text,
                      node.textStart,
                      node.textLength,
                      x,
                      baselineY,
                      color,
                      scale);
        return;
    }

    if (node.type == Node::Type::Row) {
        int cursorX = x;
        for (int i = 0; i < box.childrenCount; i++) {
            const int childNode = parseCtx.rowChildren[node.childrenStart + i];
            const int childBox = layout.rowChildren[box.childrenStart + i];
            if (i > 0) {
                const int previousChildBox = layout.rowChildren[box.childrenStart + i - 1];
                cursorX += scaleLength(rowGapBetween(layout.boxes[previousChildBox].type,
                                                     layout.boxes[childBox].type),
                                       scale);
            }
            drawBoxRecursive(parseCtx, layout, childNode, childBox,
                             display, cursorX, baselineY, color, scale);
            cursorX += scaleLength(layout.boxes[childBox].metrics.width, scale);
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
        const LayoutMetrics scaledNumMetrics = scaleMetrics(numMetrics, scale);
        const LayoutMetrics scaledDenMetrics = scaleMetrics(denMetrics, scale);
        const int scaledBoxWidth = scaleLength(box.metrics.width, scale);
        const int numHeight = scaledNumMetrics.ascent + scaledNumMetrics.descent;

        const int numeratorX = x + (scaledBoxWidth - scaledNumMetrics.width) / 2;
        const int denominatorX = x + (scaledBoxWidth - scaledDenMetrics.width) / 2;

        const int numeratorTop = baselineY - 1 - numHeight;
        const int numeratorBaseline = numeratorTop + scaledNumMetrics.ascent;
        const int denominatorTop = baselineY + 1;
        const int denominatorBaseline = denominatorTop + scaledDenMetrics.ascent;

        drawBoxRecursive(parseCtx, layout, numeratorNode, numeratorBox,
                         display, numeratorX, numeratorBaseline, color, scale);
        display.drawRect(x, baselineY, std::max(1, scaledBoxWidth), 1, color);
        drawBoxRecursive(parseCtx, layout, denominatorNode, denominatorBox,
                         display, denominatorX, denominatorBaseline, color, scale);
        return;
    }

    if (node.type == Node::Type::Root) {
        const int indexNode = node.left;
        const int radicandNode = node.right;
        const int indexBox = box.left;
        const int radicandBox = box.right;
        const LayoutMetrics radicandMetrics = layout.boxes[radicandBox].metrics;
        const LayoutMetrics scaledRadicandMetrics = scaleMetrics(radicandMetrics, scale);
        const int radicalWidth = scaleLength(FONT_CHAR_ADVANCE + 2, scale);
        const int indexWidth = (indexBox >= 0)
            ? std::max(0, scaleLength(layout.boxes[indexBox].metrics.width - 2, scale))
            : 0;
        const int radicalX = x + indexWidth;
        const int radicandX = radicalX + radicalWidth;
        const int top = baselineY - scaledRadicandMetrics.ascent - 2;
        const int bottom = baselineY + scaledRadicandMetrics.descent;

        if (indexBox >= 0) {
            const int indexBaseline = top + scaleLength(FONT_ASCENT, scale) - 1;
            drawBoxRecursive(parseCtx, layout, indexNode, indexBox,
                             display, x, indexBaseline, color, scale);
        }

        drawRootSymbol(display,
                       radicalX,
                       top,
                       baselineY,
                       bottom,
                       radicalWidth + scaledRadicandMetrics.width + 1,
                       color);
        drawBoxRecursive(parseCtx, layout, radicandNode, radicandBox,
                         display, radicandX, baselineY, color, scale);
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
                         display, x, baselineY, color, scale);
        drawBoxRecursive(parseCtx, layout, expNode, expBox,
                         display,
                         x + scaleLength(baseMetrics.width, scale),
                         baselineY - scaleLength(scriptShift, scale),
                         color,
                         scale);
        return;
    }

    if (node.type == Node::Type::Parenthesized) {
        const int innerNode = node.left;
        const int innerBox = box.left;
        const int parenWidth = scaleLength(FONT_CHAR_ADVANCE, scale);

        const LayoutMetrics scaledMetrics = scaleMetrics(box.metrics, scale);
        const int top = baselineY - scaledMetrics.ascent;
        const int height = scaledMetrics.ascent + scaledMetrics.descent;

        drawTallParen(display, true, x, top, height, color, scale);
        drawBoxRecursive(parseCtx, layout, innerNode, innerBox,
                         display, x + parenWidth, baselineY, color, scale);
        drawTallParen(display,
                      false,
                      x + parenWidth + scaleLength(layout.boxes[innerBox].metrics.width, scale),
                      top,
                      height,
                      color,
                      scale);
    }
}

static CursorMetrics cursorAt(int x,
                              int baselineOffset,
                              const LayoutMetrics& metrics,
                              float scale) {
    return {
        x,
        baselineOffset,
        std::max(1, scaleLength(metrics.ascent, scale)),
        std::max(1, scaleLength(metrics.descent, scale)),
    };
}

static bool cursorMetricsRecursive(const Context& parseCtx,
                                   const LayoutContext& layout,
                                   int nodeIndex,
                                   int boxIndex,
                                   int cursorIndex,
                                   int x,
                                   int baselineOffset,
                                   float scale,
                                   CursorMetrics& outMetrics) {
    const Node& node = parseCtx.nodes[nodeIndex];
    const Box& box = layout.boxes[boxIndex];

    if (node.type == Node::Type::Text) {
        const int offset = std::max(0, std::min(cursorIndex - node.textStart,
                                                node.textLength));
        outMetrics = cursorAt(x + scaleLength(offset * FONT_CHAR_ADVANCE, scale),
                              baselineOffset,
                              box.metrics,
                              scale);
        return true;
    }

    if (node.type == Node::Type::Row) {
        int cursorX = x;
        for (int i = 0; i < box.childrenCount; i++) {
            const int childNode = parseCtx.rowChildren[node.childrenStart + i];
            const int childBox = layout.rowChildren[box.childrenStart + i];
            const int childEnd = nodeTextEnd(parseCtx, childNode);
            const bool isLastChild = (i == box.childrenCount - 1);

            if (i > 0) {
                const int previousChildBox = layout.rowChildren[box.childrenStart + i - 1];
                cursorX += scaleLength(rowGapBetween(layout.boxes[previousChildBox].type,
                                                     layout.boxes[childBox].type),
                                       scale);
            }

            if (cursorIndex <= childEnd || isLastChild) {
                return cursorMetricsRecursive(parseCtx,
                                              layout,
                                              childNode,
                                              childBox,
                                              cursorIndex,
                                              cursorX,
                                              baselineOffset,
                                              scale,
                                              outMetrics);
            }

            cursorX += scaleLength(layout.boxes[childBox].metrics.width, scale);
        }

        outMetrics = cursorAt(x + scaleLength(box.metrics.width, scale),
                              baselineOffset,
                              box.metrics,
                              scale);
        return true;
    }

    if (node.type == Node::Type::Fraction) {
        const int numeratorNode = node.left;
        const int denominatorNode = node.right;
        const int numeratorBox = box.left;
        const int denominatorBox = box.right;

        const LayoutMetrics numMetrics = layout.boxes[numeratorBox].metrics;
        const LayoutMetrics denMetrics = layout.boxes[denominatorBox].metrics;
        const LayoutMetrics scaledNumMetrics = scaleMetrics(numMetrics, scale);
        const LayoutMetrics scaledDenMetrics = scaleMetrics(denMetrics, scale);
        const int scaledBoxWidth = scaleLength(box.metrics.width, scale);
        const int numeratorHeight = scaledNumMetrics.ascent + scaledNumMetrics.descent;

        const int numeratorX = x + (scaledBoxWidth - scaledNumMetrics.width) / 2;
        const int denominatorX = x + (scaledBoxWidth - scaledDenMetrics.width) / 2;

        const int numeratorBaselineOffset = baselineOffset - 1 - numeratorHeight
            + scaledNumMetrics.ascent;
        const int denominatorBaselineOffset = baselineOffset + 1
            + scaledDenMetrics.ascent;

        if (cursorIndex >= nodeTextStart(parseCtx, denominatorNode)) {
            return cursorMetricsRecursive(parseCtx,
                                          layout,
                                          denominatorNode,
                                          denominatorBox,
                                          cursorIndex,
                                          denominatorX,
                                          denominatorBaselineOffset,
                                          scale,
                                          outMetrics);
        }

        return cursorMetricsRecursive(parseCtx,
                                      layout,
                                      numeratorNode,
                                      numeratorBox,
                                      cursorIndex,
                                      numeratorX,
                                      numeratorBaselineOffset,
                                      scale,
                                      outMetrics);
    }

    if (node.type == Node::Type::Superscript) {
        const int baseNode = node.left;
        const int expNode = node.right;
        const int baseBox = box.left;
        const int expBox = box.right;
        const LayoutMetrics baseMetrics = layout.boxes[baseBox].metrics;
        const int exponentX = x + scaleLength(baseMetrics.width, scale);
        const int exponentBaselineOffset = baselineOffset
            - scaleLength(std::max(2, baseMetrics.ascent / 2), scale);

        if (cursorIndex >= nodeTextStart(parseCtx, expNode)) {
            return cursorMetricsRecursive(parseCtx,
                                          layout,
                                          expNode,
                                          expBox,
                                          cursorIndex,
                                          exponentX,
                                          exponentBaselineOffset,
                                          scale,
                                          outMetrics);
        }

        return cursorMetricsRecursive(parseCtx,
                                      layout,
                                      baseNode,
                                      baseBox,
                                      cursorIndex,
                                      x,
                                      baselineOffset,
                                      scale,
                                      outMetrics);
    }

    if (node.type == Node::Type::Root) {
        const int indexNode = node.left;
        const int radicandNode = node.right;
        const int indexBox = box.left;
        const int radicandBox = box.right;
        const LayoutMetrics radicandMetrics = layout.boxes[radicandBox].metrics;
        const int radicalWidth = scaleLength(FONT_CHAR_ADVANCE + 2, scale);
        const int indexWidth = (indexBox >= 0)
            ? std::max(0, scaleLength(layout.boxes[indexBox].metrics.width - 2, scale))
            : 0;
        const int radicandX = x + indexWidth + radicalWidth;

        if (indexBox >= 0 && cursorIndex <= nodeTextEnd(parseCtx, indexNode)) {
            const int top = baselineOffset - scaleLength(radicandMetrics.ascent, scale) - 2;
            const int indexBaselineOffset = top + scaleLength(FONT_ASCENT, scale) - 1;
            return cursorMetricsRecursive(parseCtx,
                                          layout,
                                          indexNode,
                                          indexBox,
                                          cursorIndex,
                                          x,
                                          indexBaselineOffset,
                                          scale,
                                          outMetrics);
        }

        return cursorMetricsRecursive(parseCtx,
                                      layout,
                                      radicandNode,
                                      radicandBox,
                                      cursorIndex,
                                      radicandX,
                                      baselineOffset,
                                      scale,
                                      outMetrics);
    }

    if (node.type == Node::Type::Parenthesized) {
        const int innerNode = node.left;
        const int innerBox = box.left;
        const int parenWidth = scaleLength(FONT_CHAR_ADVANCE, scale);
        const int innerStart = nodeTextStart(parseCtx, innerNode);
        const int innerEnd = nodeTextEnd(parseCtx, innerNode);

        if (cursorIndex < innerStart) {
            outMetrics = cursorAt(x, baselineOffset, box.metrics, scale);
            return true;
        }
        if (cursorIndex > innerEnd) {
            outMetrics = cursorAt(x + scaleLength(box.metrics.width, scale),
                                  baselineOffset,
                                  box.metrics,
                                  scale);
            return true;
        }

        return cursorMetricsRecursive(parseCtx,
                                      layout,
                                      innerNode,
                                      innerBox,
                                      cursorIndex,
                                      x + parenWidth,
                                      baselineOffset,
                                      scale,
                                      outMetrics);
    }

    return false;
}

static int nearestCursorInNode(const Context& parseCtx,
                               const LayoutContext& layout,
                               int nodeIndex,
                               int boxIndex,
                               int desiredX) {
    const int start = nodeTextStart(parseCtx, nodeIndex);
    const int end = nodeTextEnd(parseCtx, nodeIndex);
    int bestIndex = start;
    int bestDistance = 0x7fffffff;

    for (int index = start; index <= end; index++) {
        CursorMetrics metrics{};
        if (!cursorMetricsRecursive(parseCtx,
                                    layout,
                                    nodeIndex,
                                    boxIndex,
                                    index,
                                    0,
                                    0,
                                    1.0f,
                                    metrics)) {
            continue;
        }

        const int distance = std::abs(metrics.x - desiredX);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = index;
        }
    }

    return bestIndex;
}

static bool moveWithinFraction(const Context& parseCtx,
                               const LayoutContext& layout,
                               int nodeIndex,
                               int boxIndex,
                               int cursorIndex,
                               CursorMove move,
                               int desiredX,
                               int& movedIndex) {
    const Node& node = parseCtx.nodes[nodeIndex];
    const Box& box = layout.boxes[boxIndex];

    if (node.type == Node::Type::Fraction) {
        const int numeratorNode = node.left;
        const int denominatorNode = node.right;
        const int numeratorBox = box.left;
        const int denominatorBox = box.right;
        const int numeratorStart = nodeTextStart(parseCtx, numeratorNode);
        const int numeratorEnd = nodeTextEnd(parseCtx, numeratorNode);
        const int denominatorStart = nodeTextStart(parseCtx, denominatorNode);
        const int denominatorEnd = nodeTextEnd(parseCtx, denominatorNode);

        if (move == CursorMove::Up &&
            cursorIndex >= denominatorStart &&
            cursorIndex <= denominatorEnd) {
            movedIndex = nearestCursorInNode(parseCtx,
                                             layout,
                                             numeratorNode,
                                             numeratorBox,
                                             desiredX);
            return true;
        }

        if (move == CursorMove::Down &&
            cursorIndex >= numeratorStart &&
            cursorIndex <= numeratorEnd) {
            movedIndex = nearestCursorInNode(parseCtx,
                                             layout,
                                             denominatorNode,
                                             denominatorBox,
                                             desiredX);
            return true;
        }

        if (move == CursorMove::Left &&
            cursorIndex >= denominatorStart &&
            cursorIndex <= denominatorStart + 1) {
            movedIndex = numeratorStart;
            return true;
        }

        if (move == CursorMove::Right &&
            cursorIndex == node.textStart) {
            movedIndex = denominatorEnd;
            return true;
        }
    }

    if (node.type == Node::Type::Row) {
        for (int i = 0; i < box.childrenCount; i++) {
            const int childNode = parseCtx.rowChildren[node.childrenStart + i];
            const int childBox = layout.rowChildren[box.childrenStart + i];
            if (moveWithinFraction(parseCtx,
                                   layout,
                                   childNode,
                                   childBox,
                                   cursorIndex,
                                   move,
                                   desiredX,
                                   movedIndex)) {
                return true;
            }
        }
    } else if (node.type == Node::Type::Parenthesized) {
        return moveWithinFraction(parseCtx,
                                  layout,
                                  node.left,
                                  box.left,
                                  cursorIndex,
                                  move,
                                  desiredX,
                                  movedIndex);
    } else if (node.type == Node::Type::Superscript) {
        if (moveWithinFraction(parseCtx,
                               layout,
                               node.left,
                               box.left,
                               cursorIndex,
                               move,
                               desiredX,
                               movedIndex)) {
            return true;
        }
        return moveWithinFraction(parseCtx,
                                  layout,
                                  node.right,
                                  box.right,
                                  cursorIndex,
                                  move,
                                  desiredX,
                                  movedIndex);
    } else if (node.type == Node::Type::Root) {
        if (node.left >= 0 &&
            moveWithinFraction(parseCtx,
                               layout,
                               node.left,
                               box.left,
                               cursorIndex,
                               move,
                               desiredX,
                               movedIndex)) {
            return true;
        }
        return moveWithinFraction(parseCtx,
                                  layout,
                                  node.right,
                                  box.right,
                                  cursorIndex,
                                  move,
                                  desiredX,
                                  movedIndex);
    }

    return false;
}

static bool isFractionBoundaryStop(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '(' || c == ')';
}

static bool moveAcrossSourceFractionBoundary(const char* expression,
                                             int length,
                                             int cursorIndex,
                                             CursorMove move,
                                             int& movedIndex) {
    if (move == CursorMove::Right &&
        cursorIndex < length &&
        expression[cursorIndex] == '/') {
        int denominatorEnd = cursorIndex + 1;
        while (denominatorEnd < length &&
               !isFractionBoundaryStop(expression[denominatorEnd])) {
            denominatorEnd++;
        }
        movedIndex = denominatorEnd;
        return true;
    }

    if (move == CursorMove::Left &&
        cursorIndex > 0 &&
        expression[cursorIndex - 1] == '/') {
        int numeratorStart = cursorIndex - 2;
        while (numeratorStart > 0 &&
               !isFractionBoundaryStop(expression[numeratorStart - 1])) {
            numeratorStart--;
        }
        movedIndex = std::max(0, numeratorStart);
        return true;
    }

    return false;
}

static bool buildLayout(const char* expression,
                        Context& parseCtx,
                        ParsedExpression& parsed,
                        LayoutContext& layout,
                        int& rootBox) {
    parseCtx = {};
    parseCtx.text = expression;
    parseCtx.length = static_cast<int>(std::strlen(expression));

    if (parseCtx.length == 0) {
        return false;
    }

    rootBox = -1;
    parsed.root = parseExpression(parseCtx);
    parsed.ok = !parseCtx.parseError && atEnd(parseCtx) && parsed.root >= 0;
    if (!parsed.ok) {
        return false;
    }

    layout = {};
    rootBox = layoutNode(parseCtx, parsed.root, layout);
    if (layout.layoutError || rootBox < 0) {
        return false;
    }

    return true;
}

} // namespace

bool measure(const char* expression, LayoutMetrics& outMetrics) {
    Context parseCtx{};
    ParsedExpression parsed{};
    LayoutContext layout{};
    int rootBox = -1;

    if (!buildLayout(expression, parseCtx, parsed, layout, rootBox)) {
        return false;
    }

    outMetrics = layout.boxes[rootBox].metrics;
    return true;
}

int scaleLength(int value, float scale) {
    if (value <= 0) {
        return 0;
    }
    return std::max(1, static_cast<int>(std::ceil(static_cast<float>(value) * scale)));
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

bool cursorMetrics(const char* expression,
                   int cursorIndex,
                   float scale,
                   CursorMetrics& outMetrics) {
    Context parseCtx{};
    ParsedExpression parsed{};
    LayoutContext layout{};
    int rootBox = -1;

    if (!buildLayout(expression, parseCtx, parsed, layout, rootBox)) {
        return false;
    }

    const int clampedCursor = std::max(0, std::min(cursorIndex, parseCtx.length));
    return cursorMetricsRecursive(parseCtx,
                                  layout,
                                  parsed.root,
                                  rootBox,
                                  clampedCursor,
                                  0,
                                  0,
                                  scale,
                                  outMetrics);
}

int moveCursor(const char* expression, int cursorIndex, CursorMove move) {
    const int length = static_cast<int>(std::strlen(expression));
    const int clampedCursor = std::max(0, std::min(cursorIndex, length));

    Context parseCtx{};
    ParsedExpression parsed{};
    LayoutContext layout{};
    int rootBox = -1;

    if (!buildLayout(expression, parseCtx, parsed, layout, rootBox)) {
        if (move == CursorMove::Left) {
            return std::max(0, clampedCursor - 1);
        }
        if (move == CursorMove::Right) {
            return std::min(length, clampedCursor + 1);
        }
        return clampedCursor;
    }

    CursorMetrics metrics{};
    const int desiredX = cursorMetricsRecursive(parseCtx,
                                                layout,
                                                parsed.root,
                                                rootBox,
                                                clampedCursor,
                                                0,
                                                0,
                                                1.0f,
                                                metrics)
        ? metrics.x
        : 0;

    int movedIndex = clampedCursor;
    if (moveWithinFraction(parseCtx,
                           layout,
                           parsed.root,
                           rootBox,
                           clampedCursor,
                           move,
                           desiredX,
                           movedIndex)) {
        return std::max(0, std::min(movedIndex, length));
    }

    if (moveAcrossSourceFractionBoundary(expression,
                                         length,
                                         clampedCursor,
                                         move,
                                         movedIndex)) {
        return std::max(0, std::min(movedIndex, length));
    }

    if (move == CursorMove::Left) {
        return std::max(0, clampedCursor - 1);
    }
    if (move == CursorMove::Right) {
        return std::min(length, clampedCursor + 1);
    }

    return clampedCursor;
}

bool draw(const char* expression,
          Display& display,
          int x,
          int baselineY,
          uint16_t color,
          LayoutMetrics* outMetrics) {
    return drawScaled(expression, display, x, baselineY, color, 1.0f, outMetrics);
}

bool drawScaled(const char* expression,
                Display& display,
                int x,
                int baselineY,
                uint16_t color,
                float scale,
                LayoutMetrics* outMetrics) {
    Context parseCtx{};
    ParsedExpression parsed{};
    LayoutContext layout{};
    int rootBox = -1;

    if (!buildLayout(expression, parseCtx, parsed, layout, rootBox)) {
        return false;
    }

    drawBoxRecursive(parseCtx, layout, parsed.root, rootBox,
                     display, x, baselineY, color, scale);

    if (outMetrics != nullptr) {
        *outMetrics = scaleMetrics(layout.boxes[rootBox].metrics, scale);
    }
    return true;
}

} // namespace math_typeset
