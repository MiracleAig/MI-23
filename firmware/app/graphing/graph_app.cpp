#include "app/graphing/graph_app.h"

#include "math/expression.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

const Color COLOR_BG = Display::rgb(8, 10, 14);
const Color COLOR_PANEL = Display::rgb(18, 24, 32);
const Color COLOR_PANEL_SELECTED = Display::rgb(31, 42, 54);
const Color COLOR_TEXT = Display::WHITE;
const Color COLOR_MUTED = Display::rgb(150, 160, 172);
const Color COLOR_ERROR = Display::rgb(255, 110, 110);
const Color COLOR_CURSOR = Display::rgb(255, 230, 95);
const Color COLOR_TRACE = Display::rgb(255, 255, 255);

constexpr GraphWindow DEFAULT_WINDOW = {
    -10.0,
    10.0,
    -10.0,
    10.0,
    1.0,
    1.0,
};

constexpr Color FUNCTION_COLORS[GraphApp::FUNCTION_COUNT] = {
    Display::rgb(255, 230, 95),
    Display::rgb(80, 190, 255),
    Display::rgb(105, 225, 145),
    Display::rgb(255, 145, 95),
    Display::rgb(225, 135, 255),
};

bool isSyntaxError(GraphErrorType error) {
    return error == GraphErrorType::InvalidExpression ||
           error == GraphErrorType::EmptyFunction;
}

double niceGridScale(double minValue, double maxValue) {
    const double range = maxValue - minValue;
    if (!std::isfinite(range) || range <= 0.0) {
        return 1.0;
    }

    const double rawStep = range / 20.0;
    const double magnitude = std::pow(10.0, std::floor(std::log10(rawStep)));
    const double normalized = rawStep / magnitude;

    if (normalized <= 1.0) {
        return magnitude;
    }
    if (normalized <= 2.0) {
        return 2.0 * magnitude;
    }
    if (normalized <= 5.0) {
        return 5.0 * magnitude;
    }
    return 10.0 * magnitude;
}

void copyText(char* destination, const char* source, int capacity) {
    if (capacity <= 0) {
        return;
    }
    std::strncpy(destination, source ? source : "", static_cast<size_t>(capacity - 1));
    destination[capacity - 1] = '\0';
}

void drawTextFit(Display& display,
                 const char* text,
                 int x,
                 int y,
                 int maxWidth,
                 Color color) {
    if (!text || maxWidth <= 0) {
        return;
    }

    char buffer[80];
    const int maxChars = maxWidth / FONT_CHAR_ADVANCE;
    if (maxChars <= 0) {
        return;
    }

    copyText(buffer, text, maxChars < static_cast<int>(sizeof(buffer))
        ? maxChars + 1
        : static_cast<int>(sizeof(buffer)));
    display.drawText(buffer, x, y, color);
}

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
        case Key::X_VAR: return "x";
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

GraphApp::GraphApp(const SettingsState* settings)
    : m_renderer()
    , m_settings(settings)
    , m_mode(GraphMode::View)
    , m_needsRender(true)
    , m_dirtyRegions()
    , m_editHasError(false)
    , m_editError(GraphErrorType::None)
    , m_functions{}
    , m_editBuffer{}
    , m_editLength(0)
    , m_editCursor(0)
    , m_selectedFunction(0)
    , m_window(DEFAULT_WINDOW)
    , m_traceX(0.0)
    , m_traceY(0.0)
    , m_traceHasPoint(false)
    , m_contentBounds{0, 22, DISPLAY_WIDTH, DISPLAY_HEIGHT - 22} {
    copyText(m_functions[0].expression,
             GraphRenderer::DEFAULT_EXPRESSION,
             MAX_EXPRESSION_LENGTH + 1);
    m_functions[0].enabled = true;
    m_renderer.setExpression(m_functions[0].expression);
}

void GraphApp::enter() {
    m_mode = GraphMode::View;
    m_editHasError = false;
    invalidateContent();
    requestRender();
}

void GraphApp::handleKey(Key key) {
    if (key == Key::NONE) {
        return;
    }

    if (m_mode == GraphMode::View) {
        if (key == Key::ENTER) {
            enterEditMode();
        } else if (key == Key::PLUS || key == Key::CURSOR_UP) {
            zoom(0.5);
        } else if (key == Key::MINUS || key == Key::CURSOR_DOWN) {
            zoom(2.0);
        } else if (key == Key::CLEAR) {
            resetWindow();
        } else if (key == Key::CURSOR_LEFT) {
            startTrace(-1);
        } else if (key == Key::CURSOR_RIGHT) {
            startTrace(1);
        }
        return;
    }

    if (m_mode == GraphMode::Trace) {
        if (key == Key::CURSOR_LEFT) {
            moveTrace(-1);
        } else if (key == Key::CURSOR_RIGHT) {
            moveTrace(1);
        } else if (key == Key::CURSOR_UP) {
            cycleTraceFunction(-1);
        } else if (key == Key::CURSOR_DOWN) {
            cycleTraceFunction(1);
        } else if (key == Key::PLUS) {
            zoom(0.5);
            updateTracePoint();
        } else if (key == Key::MINUS) {
            zoom(2.0);
            updateTracePoint();
        } else if (key == Key::CLEAR) {
            resetWindow();
            updateTracePoint();
        } else if (key == Key::ENTER) {
            enterEditMode();
        } else if (key == Key::ESCAPE) {
            m_mode = GraphMode::View;
            requestRender();
        }
        return;
    }

    if (key == Key::ENTER) {
        acceptEdit();
    } else if (key == Key::CLEAR) {
        backspace();
    } else if (key == Key::DELETE_KEY) {
        deleteAtCursor();
    } else if (key == Key::ESCAPE) {
        m_mode = GraphMode::View;
        m_editHasError = false;
        requestRender();
    } else if (key == Key::CURSOR_LEFT) {
        moveCursor(-1);
    } else if (key == Key::CURSOR_RIGHT) {
        moveCursor(1);
    } else if (key == Key::CURSOR_UP) {
        selectEditFunction((m_selectedFunction + FUNCTION_COUNT - 1) % FUNCTION_COUNT);
    } else if (key == Key::CURSOR_DOWN) {
        selectEditFunction((m_selectedFunction + 1) % FUNCTION_COUNT);
    } else if (key == Key::NEGATE) {
        toggleSelectedFunction();
    } else {
        const char* text = textForKey(key);
        if (text) {
            appendText(text);
        }
    }
}

void GraphApp::renderContent(Display& display, int x, int y, int w, int h) {
    m_contentBounds = {x, y, w, h};
    if (m_dirtyRegions.empty()) {
        invalidateRect(m_contentBounds);
    }

    for (int i = 0; i < m_dirtyRegions.count(); ++i) {
        const DisplayRect clip = DirtyRegionList::intersect(m_dirtyRegions.rect(i), m_contentBounds);
        if (clip.isEmpty()) {
            continue;
        }
        display.setClipRect(clip);
        if (m_mode == GraphMode::EditEquation) {
            renderEditor(display, x, y, w, h);
        } else {
            renderGraph(display, x, y, w, h);
        }
    }
    display.clearClipRect();
    m_dirtyRegions.clear();
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
    return m_functions[0].expression;
}

const char* GraphApp::editExpression() const {
    return m_editBuffer;
}

const char* GraphApp::functionExpression(int index) const {
    if (index < 0 || index >= FUNCTION_COUNT) {
        return "";
    }
    return m_functions[index].expression;
}

bool GraphApp::functionEnabled(int index) const {
    return index >= 0 && index < FUNCTION_COUNT && m_functions[index].enabled;
}

int GraphApp::selectedFunction() const {
    return m_selectedFunction;
}

int GraphApp::editCursor() const {
    return m_editCursor;
}

const GraphWindow& GraphApp::window() const {
    return m_window;
}

void GraphApp::enterEditMode() {
    copyText(m_editBuffer,
             m_functions[m_selectedFunction].expression,
             MAX_EXPRESSION_LENGTH + 1);
    m_editLength = static_cast<int>(std::strlen(m_editBuffer));
    m_editCursor = m_editLength;
    m_mode = GraphMode::EditEquation;
    m_editHasError = false;
    m_editError = GraphErrorType::None;
    invalidateContent();
    requestRender();
}

void GraphApp::acceptEdit() {
    commitEditBuffer(true);
}

bool GraphApp::commitEditBuffer(bool exitOnSuccess) {
    GraphErrorType error = GraphErrorType::None;
    if (!isAcceptableExpression(m_editBuffer, error)) {
        m_editHasError = true;
        m_editError = error;
        invalidateContent();
        requestRender();
        return false;
    }

    const bool expressionChanged =
        std::strcmp(m_functions[m_selectedFunction].expression, m_editBuffer) != 0;

    copyText(m_functions[m_selectedFunction].expression,
             m_editBuffer,
             MAX_EXPRESSION_LENGTH + 1);
    if (m_editLength <= 0) {
        m_functions[m_selectedFunction].enabled = false;
    } else if (expressionChanged) {
        m_functions[m_selectedFunction].enabled = true;
    }
    m_renderer.setExpression(m_functions[0].expression);
    if (exitOnSuccess) {
        m_mode = GraphMode::View;
    }
    m_editHasError = false;
    m_editError = GraphErrorType::None;
    invalidateContent();
    requestRender();
    return true;
}

bool GraphApp::isAcceptableExpression(const char* expression, GraphErrorType& error) const {
    error = GraphErrorType::None;
    if (!expression || expression[0] == '\0') {
        return true;
    }

    constexpr double SAMPLE_X_VALUES[] = {-2.0, -1.0, -0.5, 0.5, 1.0, 2.0};
    GraphErrorType lastError = GraphErrorType::None;
    for (double xValue : SAMPLE_X_VALUES) {
        double yValue = 0.0;
        GraphErrorType sampleError = GraphErrorType::None;
        if (m_renderer.evaluateExpression(expression, xValue, yValue, &sampleError)) {
            return true;
        }
        lastError = sampleError;
        if (isSyntaxError(sampleError)) {
            error = sampleError;
            return false;
        }
    }

    error = lastError == GraphErrorType::None ? GraphErrorType::InvalidExpression : lastError;
    return !isSyntaxError(error);
}

void GraphApp::selectEditFunction(int index) {
    if (index < 0 || index >= FUNCTION_COUNT || index == m_selectedFunction) {
        return;
    }
    if (!commitEditBuffer(false)) {
        return;
    }

    m_selectedFunction = index;
    copyText(m_editBuffer,
             m_functions[m_selectedFunction].expression,
             MAX_EXPRESSION_LENGTH + 1);
    m_editLength = static_cast<int>(std::strlen(m_editBuffer));
    m_editCursor = m_editLength;
    m_editHasError = false;
    m_editError = GraphErrorType::None;
    invalidateContent();
    requestRender();
}

void GraphApp::toggleSelectedFunction() {
    if (!commitEditBuffer(false)) {
        return;
    }
    if (m_editLength <= 0) {
        m_functions[m_selectedFunction].enabled = false;
    } else {
        m_functions[m_selectedFunction].enabled = !m_functions[m_selectedFunction].enabled;
    }
    invalidateContent();
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

    std::memmove(&m_editBuffer[m_editCursor + length],
                 &m_editBuffer[m_editCursor],
                 static_cast<size_t>(m_editLength - m_editCursor + 1));
    std::memcpy(&m_editBuffer[m_editCursor], text, static_cast<size_t>(length));
    m_editLength += length;
    m_editCursor += length;
    m_editBuffer[m_editLength] = '\0';
    m_editHasError = false;
    m_editError = GraphErrorType::None;
    invalidateContent();
    requestRender();
}

void GraphApp::backspace() {
    if (m_editCursor <= 0) {
        return;
    }
    std::memmove(&m_editBuffer[m_editCursor - 1],
                 &m_editBuffer[m_editCursor],
                 static_cast<size_t>(m_editLength - m_editCursor + 1));
    m_editLength--;
    m_editCursor--;
    m_editBuffer[m_editLength] = '\0';
    m_editHasError = false;
    m_editError = GraphErrorType::None;
    invalidateContent();
    requestRender();
}

void GraphApp::deleteAtCursor() {
    if (m_editCursor >= m_editLength) {
        return;
    }
    std::memmove(&m_editBuffer[m_editCursor],
                 &m_editBuffer[m_editCursor + 1],
                 static_cast<size_t>(m_editLength - m_editCursor));
    m_editLength--;
    m_editBuffer[m_editLength] = '\0';
    m_editHasError = false;
    m_editError = GraphErrorType::None;
    invalidateContent();
    requestRender();
}

void GraphApp::moveCursor(int delta) {
    const int next = m_editCursor + delta;
    if (next < 0 || next > m_editLength) {
        return;
    }
    m_editCursor = next;
    invalidateContent();
    requestRender();
}

void GraphApp::zoom(double factor) {
    if (factor <= 0.0 || !std::isfinite(factor)) {
        return;
    }

    const double centerX = (m_window.xMin + m_window.xMax) * 0.5;
    const double centerY = (m_window.yMin + m_window.yMax) * 0.5;
    const double halfX = (m_window.xMax - m_window.xMin) * factor * 0.5;
    const double halfY = (m_window.yMax - m_window.yMin) * factor * 0.5;

    if (halfX <= 1e-6 || halfY <= 1e-6 || halfX > 1e6 || halfY > 1e6) {
        return;
    }

    m_window.xMin = centerX - halfX;
    m_window.xMax = centerX + halfX;
    m_window.yMin = centerY - halfY;
    m_window.yMax = centerY + halfY;
    refreshWindowScales();
    invalidateGraphViewportOnly();
    requestRender();
}

void GraphApp::resetWindow() {
    m_window = DEFAULT_WINDOW;
    refreshWindowScales();
    invalidateGraphViewportOnly();
    requestRender();
}

void GraphApp::refreshWindowScales() {
    m_window.xScale = niceGridScale(m_window.xMin, m_window.xMax);
    m_window.yScale = niceGridScale(m_window.yMin, m_window.yMax);
}

void GraphApp::startTrace(int direction) {
    const int enabled = nextEnabledFunction(m_selectedFunction, 1);
    if (enabled < 0) {
        m_traceHasPoint = false;
        m_mode = GraphMode::Trace;
        invalidateGraphViewportOnly();
        requestRender();
        return;
    }
    m_selectedFunction = enabled;
    m_traceX = (m_window.xMin + m_window.xMax) * 0.5;
    m_mode = GraphMode::Trace;
    updateTracePoint();
    if (!m_traceHasPoint) {
        moveTrace(direction >= 0 ? 1 : -1);
    }
    invalidateGraphViewportOnly();
    requestRender();
}

void GraphApp::moveTrace(int direction) {
    if (direction == 0) {
        return;
    }
    const double step = (m_window.xMax - m_window.xMin) / 160.0;
    const double delta = direction > 0 ? step : -step;
    for (int attempt = 0; attempt < 180; ++attempt) {
        const double nextX = m_traceX + delta;
        if (nextX < m_window.xMin || nextX > m_window.xMax) {
            break;
        }
        m_traceX = nextX;
        updateTracePoint();
        if (m_traceHasPoint) {
            break;
        }
    }
    invalidateGraphViewportOnly();
    requestRender();
}

void GraphApp::cycleTraceFunction(int direction) {
    const int next = nextEnabledFunction(m_selectedFunction, direction);
    if (next < 0) {
        return;
    }
    m_selectedFunction = next;
    updateTracePoint();
    if (!m_traceHasPoint) {
        moveTrace(direction >= 0 ? 1 : -1);
    }
    invalidateGraphViewportOnly();
    requestRender();
}

void GraphApp::updateTracePoint() {
    if (m_selectedFunction < 0 ||
        m_selectedFunction >= FUNCTION_COUNT ||
        !m_functions[m_selectedFunction].enabled) {
        m_traceHasPoint = false;
        return;
    }

    GraphErrorType error = GraphErrorType::None;
    double yValue = 0.0;
    m_traceHasPoint = m_renderer.evaluateExpression(m_functions[m_selectedFunction].expression,
                                                    m_traceX,
                                                    yValue,
                                                    &error);
    if (m_traceHasPoint) {
        m_traceY = yValue;
    }
}

int GraphApp::nextEnabledFunction(int startIndex, int direction) const {
    const int step = direction >= 0 ? 1 : -1;
    for (int offset = 0; offset < FUNCTION_COUNT; ++offset) {
        int index = startIndex + offset * step;
        while (index < 0) {
            index += FUNCTION_COUNT;
        }
        index %= FUNCTION_COUNT;
        if (m_functions[index].enabled) {
            return index;
        }
    }
    return -1;
}

GraphViewport GraphApp::makeViewport(int x, int y, int w, int h, int footerHeight) const {
    return {
        x + 6,
        y + 6,
        w - 12,
        h - footerHeight - 8,
        m_window.xMin,
        m_window.xMax,
        m_window.yMin,
        m_window.yMax,
        m_window.xScale,
        m_window.yScale,
    };
}

void GraphApp::buildRenderFunctions(GraphFunction* functions) const {
    for (int i = 0; i < FUNCTION_COUNT; ++i) {
        functions[i] = {
            m_functions[i].expression,
            m_functions[i].enabled,
            FUNCTION_COLORS[i],
        };
    }
}

void GraphApp::renderGraph(Display& display, int x, int y, int w, int h) {
    display.fillRect(x, y, w, h, COLOR_BG);

    const int footerHeight = 36;
    if (w < 40 || h < footerHeight + 32) {
        display.drawText("Graph layout error", x + 4, y + 8, COLOR_ERROR);
        return;
    }

    GraphFunction functions[FUNCTION_COUNT];
    buildRenderFunctions(functions);

    const GraphViewport viewport = makeViewport(x, y, w, h, footerHeight);
    GraphRenderOptions options{};
    if (m_settings) {
        options.showGrid = m_settings->graphGrid;
        options.showAxes = m_settings->graphAxes;
        options.samplesPerPixel = m_settings->graphSamplesPerPixel();
    }
    const GraphRenderResult result = m_renderer.render(display,
                                                       viewport,
                                                       functions,
                                                       FUNCTION_COUNT,
                                                       options);
    if (m_mode == GraphMode::Trace) {
        renderTraceOverlay(display, viewport, x, y, w, h);
    }

    if (m_settings && m_settings->developer.showGraphBounds) {
        char bounds[80];
        std::snprintf(bounds,
                      sizeof(bounds),
                      "x[%.2g,%.2g] y[%.2g,%.2g]",
                      m_window.xMin,
                      m_window.xMax,
                      m_window.yMin,
                      m_window.yMax);
        display.drawText(bounds, x + 8, y + 8, COLOR_TRACE);
    }

    char summary[96];
    std::snprintf(summary,
                  sizeof(summary),
                  "Y%d%s=%s",
                  m_selectedFunction + 1,
                  m_functions[m_selectedFunction].enabled ? "" : "(off)",
                  m_functions[m_selectedFunction].expression);
    drawTextFit(display, summary, x + 8, y + h - 32, w - 16, FUNCTION_COLORS[m_selectedFunction]);

    if (!result.drewAnyFunction && result.error != GraphErrorType::None) {
        display.drawText(graphErrorText(result.error), x + 8, y + h - 20, COLOR_ERROR);
    } else if (m_mode == GraphMode::Trace) {
        display.drawText("Trace: left/right, up/down Y", x + 8, y + h - 20, COLOR_MUTED);
    } else {
        display.drawText("Enter edit  +/- zoom  Clear reset", x + 8, y + h - 20, COLOR_MUTED);
    }
    display.drawText("Arrows trace/zoom", x + 8, y + h - 10, COLOR_MUTED);
}

void GraphApp::renderEditor(Display& display, int x, int y, int w, int h) {
    display.fillRect(x, y, w, h, COLOR_BG);
    display.drawText("Y= Editor", x + 8, y + 8, COLOR_TEXT);

    const int rowStartY = y + 28;
    const int rowHeight = 20;
    for (int i = 0; i < FUNCTION_COUNT; ++i) {
        const int rowY = rowStartY + i * rowHeight;
        const bool selected = i == m_selectedFunction;
        display.fillRect(x + 6,
                         rowY - 4,
                         w - 12,
                         rowHeight,
                         selected ? COLOR_PANEL_SELECTED : COLOR_PANEL);

        char label[8];
        std::snprintf(label, sizeof(label), "Y%d", i + 1);
        display.drawText(label, x + 10, rowY, FUNCTION_COLORS[i]);

        const char* expression = selected ? m_editBuffer : m_functions[i].expression;
        const bool hasExpression = expression && expression[0] != '\0';
        const bool showPlaceholder = !selected && !hasExpression;
        const int expressionX = x + 42;

        display.drawText("=", x + 24, rowY, COLOR_MUTED);
        if (showPlaceholder) {
            drawTextFit(display, "off", expressionX, rowY,
                        w - (expressionX - x) - 12, COLOR_MUTED);
        } else if (hasExpression) {
            drawTextFit(display, expression, expressionX, rowY,
                        w - (expressionX - x) - 12, COLOR_TEXT);
        }

        if (selected) {
            const int cursorX = expressionX + m_editCursor * FONT_CHAR_ADVANCE;
            display.fillRect(cursorX, rowY, 2, 9, COLOR_CURSOR);
        }
    }

    display.drawText("Enter save  Up next Y", x + 8, y + h - 32, COLOR_MUTED);
    display.drawText("Left/right cursor", x + 8, y + h - 22, COLOR_MUTED);
    display.drawText("Clear backspace  x key inserts x", x + 8, y + h - 12, COLOR_MUTED);

    if (m_editHasError) {
        display.drawText(graphErrorText(m_editError), x + 8, y + 132, COLOR_ERROR);
    }
}

void GraphApp::renderTraceOverlay(Display& display,
                                  const GraphViewport& viewport,
                                  int x,
                                  int y,
                                  int w,
                                  int h) {
    (void)h;
    if (!m_traceHasPoint) {
        display.drawText("Trace invalid", x + 8, y + 8, COLOR_ERROR);
        return;
    }

    const int px = viewport.mathToScreenX(m_traceX);
    const int py = viewport.mathToScreenY(m_traceY);
    if (viewport.containsScreenPoint(px, py)) {
        display.drawHorizontalLine(px - 4, py, 9, COLOR_TRACE);
        display.drawVerticalLine(px, py - 4, 9, COLOR_TRACE);
    }

    char label[80];
    std::snprintf(label,
                  sizeof(label),
                  "Y%d x=%.3g y=%.3g",
                  m_selectedFunction + 1,
                  m_traceX,
                  m_traceY);
    display.fillRect(x + 6, y + 6, w - 12, 14, COLOR_PANEL);
    drawTextFit(display, label, x + 10, y + 9, w - 20, COLOR_TEXT);
}

DisplayRect GraphApp::contentBounds() const {
    return m_contentBounds;
}

DisplayRect GraphApp::graphViewportRect() const {
    const DisplayRect bounds = contentBounds();
    const int footerHeight = 36;
    return {bounds.x + 6, bounds.y + 6, bounds.w - 12, bounds.h - footerHeight - 8};
}

DisplayRect GraphApp::graphFooterRect() const {
    const DisplayRect bounds = contentBounds();
    return {bounds.x, bounds.y + bounds.h - 36, bounds.w, 36};
}

void GraphApp::invalidateRect(DisplayRect rect) {
    m_dirtyRegions.add(rect);
}

void GraphApp::invalidateContent() {
    invalidateRect(contentBounds());
}

void GraphApp::invalidateGraphViewportOnly() {
    invalidateRect(graphViewportRect());
    invalidateRect(graphFooterRect());
}
