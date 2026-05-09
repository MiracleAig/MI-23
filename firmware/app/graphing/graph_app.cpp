#include "app/graphing/graph_app.h"

#include "math/expression.h"

#include <cstring>

namespace {

const Color COLOR_BG = Display::rgb(8, 10, 14);
const Color COLOR_PANEL = Display::rgb(18, 24, 32);
const Color COLOR_TEXT = Display::WHITE;
const Color COLOR_MUTED = Display::rgb(150, 160, 172);
const Color COLOR_ERROR = Display::rgb(255, 110, 110);
const Color COLOR_CURSOR = Display::rgb(255, 230, 95);

const char* textForKey(Key key) {
    switch (key) {
        case Key::SIN: return "sin(";
        case Key::COS: return "cos(";
        case Key::TAN: return "tan(";
        case Key::COT: return "cot(";
        case Key::SEC: return "sec(";
        case Key::CSC: return "csc(";
        case Key::ASIN: return "asin(";
        case Key::ACOS: return "acos(";
        case Key::ATAN: return "atan(";
        case Key::ACOT: return "acot(";
        case Key::ASEC: return "asec(";
        case Key::ACSC: return "acsc(";
        case Key::LOG: return "log(";
        case Key::LN: return "ln(";
        case Key::SQRT: return "sqrt(";
        case Key::ROOT: return "root(";
        case Key::CURSOR_DOWN: return "x";
        default: break;
    }

    if (isPrintable(key)) {
        static char buffer[2] = {};
        buffer[0] = toChar(key);
        buffer[1] = '\0';
        return buffer;
    }

    return nullptr;
}

} // namespace

GraphApp::GraphApp()
    : m_mode(GraphMode::View)
    , m_needsRender(true)
    , m_editHasError(false)
    , m_expression{}
    , m_editBuffer{}
    , m_editLength(0) {
    std::strncpy(m_expression, GraphRenderer::DEFAULT_EXPRESSION, MAX_EXPRESSION_LENGTH);
    m_expression[MAX_EXPRESSION_LENGTH] = '\0';
    m_renderer.setExpression(m_expression);
}

void GraphApp::enter() {
    m_mode = GraphMode::View;
    m_editHasError = false;
    requestRender();
}

void GraphApp::handleKey(Key key) {
    if (key == Key::NONE) {
        return;
    }

    if (m_mode == GraphMode::View) {
        if (key == Key::ENTER) {
            enterEditMode();
        }
        return;
    }

    if (key == Key::ENTER) {
        acceptEdit();
    } else if (key == Key::CLEAR) {
        backspace();
    } else if (key == Key::ESCAPE) {
        m_mode = GraphMode::View;
        m_editHasError = false;
        requestRender();
    } else {
        const char* text = textForKey(key);
        if (text) {
            appendText(text);
        }
    }
}

void GraphApp::renderContent(Display& display, int x, int y, int w, int h) {
    if (m_mode == GraphMode::EditEquation) {
        renderEditor(display, x, y, w, h);
    } else {
        renderGraph(display, x, y, w, h);
    }
    m_needsRender = false;
}

void GraphApp::requestRender() {
    m_needsRender = true;
}

bool GraphApp::needsRender() const {
    return m_needsRender;
}

GraphMode GraphApp::mode() const {
    return m_mode;
}

const char* GraphApp::expression() const {
    return m_expression;
}

const char* GraphApp::editExpression() const {
    return m_editBuffer;
}

void GraphApp::enterEditMode() {
    std::strncpy(m_editBuffer, m_expression, MAX_EXPRESSION_LENGTH);
    m_editBuffer[MAX_EXPRESSION_LENGTH] = '\0';
    m_editLength = static_cast<int>(std::strlen(m_editBuffer));
    m_mode = GraphMode::EditEquation;
    m_editHasError = false;
    requestRender();
}

void GraphApp::acceptEdit() {
    const ExprResult result = evaluateWithX(m_editBuffer, 1.0f);
    if (!result.ok) {
        m_editHasError = true;
        requestRender();
        return;
    }

    std::strncpy(m_expression, m_editBuffer, MAX_EXPRESSION_LENGTH);
    m_expression[MAX_EXPRESSION_LENGTH] = '\0';
    m_renderer.setExpression(m_expression);
    m_mode = GraphMode::View;
    m_editHasError = false;
    requestRender();
}

void GraphApp::appendText(const char* text) {
    if (!text) {
        return;
    }

    const int length = static_cast<int>(std::strlen(text));
    if (length <= 0 || m_editLength + length > MAX_EXPRESSION_LENGTH) {
        return;
    }

    std::memcpy(&m_editBuffer[m_editLength], text, static_cast<size_t>(length));
    m_editLength += length;
    m_editBuffer[m_editLength] = '\0';
    m_editHasError = false;
    requestRender();
}

void GraphApp::backspace() {
    if (m_editLength <= 0) {
        return;
    }
    m_editLength--;
    m_editBuffer[m_editLength] = '\0';
    m_editHasError = false;
    requestRender();
}

void GraphApp::renderGraph(Display& display, int x, int y, int w, int h) {
    display.fillRect(x, y, w, h, COLOR_BG);

    const int footerHeight = 24;
    if (w < 40 || h < footerHeight + 32) {
        display.drawText("Graph layout error", x + 4, y + 8, COLOR_ERROR);
        return;
    }

    const GraphViewport viewport = {
        x + 6,
        y + 6,
        w - 12,
        h - footerHeight - 8,
        -10.0,
        10.0,
        -2.0,
        10.0,
    };

    m_renderer.render(display, viewport);
    display.drawText("Y1=", x + 8, y + h - 20, COLOR_MUTED);
    display.drawText(m_expression, x + 26, y + h - 20, COLOR_MUTED);
    display.drawText("Enter edit", x + 8, y + h - 10, COLOR_MUTED);
}

void GraphApp::renderEditor(Display& display, int x, int y, int w, int h) {
    display.fillRect(x, y, w, h, COLOR_BG);
    display.drawText("Y= Editor", x + 8, y + 10, COLOR_TEXT);
    display.drawText("Y1=", x + 8, y + 38, COLOR_MUTED);

    const int expressionX = x + 32;
    const int expressionY = y + 38;
    display.fillRect(x + 6, y + 32, w - 12, 22, COLOR_PANEL);
    display.drawText("Y1=", x + 10, expressionY, COLOR_MUTED);
    display.drawText(m_editBuffer, expressionX, expressionY, COLOR_TEXT);

    const int cursorX = expressionX + Display::textWidth(m_editBuffer);
    display.fillRect(cursorX, expressionY, 2, 9, COLOR_CURSOR);

    display.drawText("Enter save", x + 8, y + h - 22, COLOR_MUTED);
    display.drawText("Clear backspace", x + 8, y + h - 12, COLOR_MUTED);

    if (m_editHasError) {
        display.drawText("Invalid expression", x + 8, y + 66, COLOR_ERROR);
    } else {
        display.drawText("Down key inserts x", x + 8, y + 66, COLOR_MUTED);
    }
}
