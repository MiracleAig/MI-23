#include "math/math_typeset.h"

#include <algorithm>
#include <cctype>
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

static int parsePrimary(Context& ctx) {
    skipSpaces(ctx);
    if (ctx.cursor >= ctx.length) {
        ctx.parseError = true;
        return -1;
    }

    const int start = ctx.cursor;
    const char c = ctx.text[ctx.cursor];

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
            ctx.cursor++;
            const int rhs = parsePower(ctx);
            const int fraction = addNode(ctx, Node::Type::Fraction);
            if (fraction < 0) {
                return -1;
            }
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
            width += childMetrics.width;
            ascent = std::max(ascent, childMetrics.ascent);
            descent = std::max(descent, childMetrics.descent);
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
        const int barWidth = std::max(numeratorMetrics.width, denominatorMetrics.width) + 2;

        layout.boxes[box].left = numerator;
        layout.boxes[box].right = denominator;
        layout.boxes[box].metrics = {
            barWidth,
            numeratorHeight + 1,
            1 + denominatorHeight,
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

static void drawBoxRecursive(const Context& parseCtx,
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

        const int numeratorTop = baselineY - 1 - numHeight;
        const int numeratorBaseline = numeratorTop + numMetrics.ascent;
        const int denominatorTop = baselineY + 1;
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
        const int parenWidth = FONT_CHAR_ADVANCE;

        drawChar(display, static_cast<unsigned char>('('), x, baselineY - FONT_ASCENT, color);
        drawBoxRecursive(parseCtx, layout, innerNode, innerBox,
                         display, x + parenWidth, baselineY, color);
        drawChar(display, static_cast<unsigned char>(')'),
                 x + parenWidth + layout.boxes[innerBox].metrics.width,
                 baselineY - FONT_ASCENT,
                 color);
    }
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

bool draw(const char* expression,
          Display& display,
          int x,
          int baselineY,
          uint16_t color,
          LayoutMetrics* outMetrics) {
    Context parseCtx{};
    ParsedExpression parsed{};
    LayoutContext layout{};
    int rootBox = -1;

    if (!buildLayout(expression, parseCtx, parsed, layout, rootBox)) {
        return false;
    }

    drawBoxRecursive(parseCtx, layout, parsed.root, rootBox,
                     display, x, baselineY, color);

    if (outMetrics != nullptr) {
        *outMetrics = layout.boxes[rootBox].metrics;
    }
    return true;
}

} // namespace math_typeset
