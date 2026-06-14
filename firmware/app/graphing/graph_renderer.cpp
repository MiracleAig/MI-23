#include "app/graphing/graph_renderer.h"

#include "math/expression.h"

#include <cmath>
#include <cstring>

namespace {

const Color GRAPH_BG = Display::rgb(4, 7, 10);
const Color GRAPH_BORDER = Display::rgb(108, 118, 132);
const Color GRAPH_GRID = Display::rgb(32, 42, 52);
const Color GRAPH_AXIS = Display::rgb(185, 195, 205);
const Color GRAPH_TICK = Display::rgb(130, 144, 158);
const Color GRAPH_CURVE = Display::rgb(255, 230, 95);
const Color GRAPH_ERROR = Display::rgb(255, 110, 110);

constexpr int TICK_HALF_LENGTH = 3;
constexpr int MAX_GRID_LINES_PER_AXIS = 80;
constexpr int OFFSCREEN_MARGIN_MULTIPLIER = 4;
constexpr double MAX_REASONABLE_Y = 1.0e6;

int roundToInt(double value) {
    return static_cast<int>(std::lround(value));
}

bool isFinite(double value) {
    return std::isfinite(value);
}

int clampInt(int value, int minValue, int maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

enum ClipCode {
    INSIDE = 0,
    LEFT = 1,
    RIGHT = 2,
    BOTTOM = 4,
    TOP = 8,
};

int clipCode(int x, int y, int left, int top, int right, int bottom) {
    int code = INSIDE;
    if (x < left) {
        code |= LEFT;
    } else if (x > right) {
        code |= RIGHT;
    }
    if (y < top) {
        code |= TOP;
    } else if (y > bottom) {
        code |= BOTTOM;
    }
    return code;
}

GraphErrorType classifyExpressionError(const ExprResult& result) {
    if (result.ok) {
        return GraphErrorType::None;
    }
    if (!result.error) {
        return GraphErrorType::InvalidExpression;
    }
    if (std::strcmp(result.error, "Empty expression") == 0) {
        return GraphErrorType::EmptyFunction;
    }
    if (std::strcmp(result.error, "Division by zero") == 0) {
        return GraphErrorType::DivideByZero;
    }
    if (std::strstr(result.error, "domain error") != nullptr ||
        std::strstr(result.error, "Domain error") != nullptr) {
        return GraphErrorType::DomainError;
    }
    return GraphErrorType::InvalidExpression;
}

} // namespace

const char* graphErrorText(GraphErrorType error) {
    switch (error) {
        case GraphErrorType::None: return "";
        case GraphErrorType::EmptyFunction: return "Empty function";
        case GraphErrorType::InvalidExpression: return "Invalid expression";
        case GraphErrorType::DomainError: return "Domain error";
        case GraphErrorType::DivideByZero: return "Divide by zero";
        case GraphErrorType::NoEnabledFunctions: return "No enabled functions";
        case GraphErrorType::NoValidPoints: return "No valid points";
        default: return "Graph error";
    }
}

bool GraphViewport::isValid() const {
    return screenWidth > 1 &&
           screenHeight > 1 &&
           xMin < xMax &&
           yMin < yMax &&
           xScale > 0.0 &&
           yScale > 0.0 &&
           isFinite(xMin) &&
           isFinite(xMax) &&
           isFinite(yMin) &&
           isFinite(yMax) &&
           isFinite(xScale) &&
           isFinite(yScale);
}

int GraphViewport::mathToScreenX(double x) const {
    const double t = (x - xMin) / (xMax - xMin);
    return screenX + roundToInt(t * static_cast<double>(screenWidth - 1));
}

int GraphViewport::mathToScreenY(double y) const {
    const double t = (yMax - y) / (yMax - yMin);
    return screenY + roundToInt(t * static_cast<double>(screenHeight - 1));
}

double GraphViewport::screenToMathX(int px) const {
    const double t = static_cast<double>(px - screenX) /
                     static_cast<double>(screenWidth - 1);
    return xMin + t * (xMax - xMin);
}

double GraphViewport::screenToMathY(int py) const {
    const double t = static_cast<double>(py - screenY) /
                     static_cast<double>(screenHeight - 1);
    return yMax - t * (yMax - yMin);
}

bool GraphViewport::containsScreenPoint(int px, int py) const {
    return px >= screenX &&
           px < screenX + screenWidth &&
           py >= screenY &&
           py < screenY + screenHeight;
}

GraphRenderer::GraphRenderer()
    : m_expression(DEFAULT_EXPRESSION) {}

const char* GraphRenderer::expression() const {
    return m_expression;
}

void GraphRenderer::setExpression(const char* expression) {
    m_expression = expression ? expression : "";
}

bool GraphRenderer::evaluateExpression(double x, double& y) const {
    return evaluateExpression(m_expression, x, y);
}

bool GraphRenderer::evaluateExpression(const char* expression,
                                       double x,
                                       double& y,
                                       GraphErrorType* error) const {
    if (error) {
        *error = GraphErrorType::None;
    }
    if (!expression || expression[0] == '\0') {
        if (error) {
            *error = GraphErrorType::EmptyFunction;
        }
        return false;
    }

    const ExprResult result = evaluateWithX(expression, static_cast<float>(x));
    if (!result.ok || !isFinite(result.value)) {
        if (error) {
            *error = classifyExpressionError(result);
        }
        return false;
    }
    y = static_cast<double>(result.value);
    if (!isFinite(y) || std::fabs(y) > MAX_REASONABLE_Y) {
        if (error) {
            *error = GraphErrorType::DomainError;
        }
        return false;
    }
    return true;
}

GraphRenderResult GraphRenderer::render(Display& display,
                                        const GraphViewport& viewport) const {
    const GraphFunction function = {m_expression, true, GRAPH_CURVE};
    return render(display, viewport, &function, 1, {});
}

GraphRenderResult GraphRenderer::render(Display& display,
                                        const GraphViewport& viewport,
                                        const GraphFunction* functions,
                                        int functionCount) const {
    return render(display, viewport, functions, functionCount, {});
}

GraphRenderResult GraphRenderer::render(Display& display,
                                        const GraphViewport& viewport,
                                        const GraphFunction* functions,
                                        int functionCount,
                                        const GraphRenderOptions& options) const {
    if (!viewport.isValid()) {
        return {false, GraphErrorType::InvalidExpression};
    }

    display.fillRect(viewport.screenX,
                     viewport.screenY,
                     viewport.screenWidth,
                     viewport.screenHeight,
                     GRAPH_BG);
    display.drawRect(viewport.screenX,
                     viewport.screenY,
                     viewport.screenWidth,
                     viewport.screenHeight,
                     GRAPH_BORDER);

    drawGridAndAxes(display, viewport, options);

    bool enabledAnyFunction = false;
    bool drewAnyFunction = false;
    GraphErrorType firstError = GraphErrorType::None;

    for (int i = 0; functions && i < functionCount; ++i) {
        if (!functions[i].enabled) {
            continue;
        }
        enabledAnyFunction = true;
        const FunctionRenderResult result = drawExpression(display,
                                                           viewport,
                                                           functions[i].expression,
                                                           functions[i].color,
                                                           options.samplesPerPixel);
        if (result.hasAnyValidPoint) {
            drewAnyFunction = true;
        } else if (firstError == GraphErrorType::None && result.error != GraphErrorType::None) {
            firstError = result.error;
        }
    }

    if (!enabledAnyFunction) {
        display.drawText(graphErrorText(GraphErrorType::NoEnabledFunctions),
                         viewport.screenX + 4,
                         viewport.screenY + 4,
                         GRAPH_ERROR);
        return {false, GraphErrorType::NoEnabledFunctions};
    }

    if (!drewAnyFunction) {
        const GraphErrorType error = firstError == GraphErrorType::None
            ? GraphErrorType::NoValidPoints
            : firstError;
        display.drawText(graphErrorText(error),
                         viewport.screenX + 4,
                         viewport.screenY + 4,
                         GRAPH_ERROR);
        return {false, error};
    }

    return {true, GraphErrorType::None};
}

void GraphRenderer::drawGridAndAxes(Display& display,
                                    const GraphViewport& viewport,
                                    const GraphRenderOptions& options) const {
    const bool hasXAxis = viewport.yMin <= 0.0 && viewport.yMax >= 0.0;
    const bool hasYAxis = viewport.xMin <= 0.0 && viewport.xMax >= 0.0;

    const double firstXGrid = std::ceil(viewport.xMin / viewport.xScale) * viewport.xScale;
    const double firstYGrid = std::ceil(viewport.yMin / viewport.yScale) * viewport.yScale;

    if (options.showGrid) {
        int gridCount = 0;
        for (double value = firstXGrid;
             value <= viewport.xMax + viewport.xScale * 0.5 &&
             gridCount < MAX_GRID_LINES_PER_AXIS;
             value += viewport.xScale, ++gridCount) {
            if (std::fabs(value) < 1e-9) {
                continue;
            }
            const int px = viewport.mathToScreenX(value);
            display.drawVerticalLine(px,
                                     viewport.screenY,
                                     viewport.screenHeight,
                                     GRAPH_GRID);
        }

        gridCount = 0;
        for (double value = firstYGrid;
             value <= viewport.yMax + viewport.yScale * 0.5 &&
             gridCount < MAX_GRID_LINES_PER_AXIS;
             value += viewport.yScale, ++gridCount) {
            if (std::fabs(value) < 1e-9) {
                continue;
            }
            const int py = viewport.mathToScreenY(value);
            display.drawHorizontalLine(viewport.screenX,
                                       py,
                                       viewport.screenWidth,
                                       GRAPH_GRID);
        }
    }

    if (!options.showAxes) {
        return;
    }

    int gridCount = 0;
    int axisY = viewport.screenY + viewport.screenHeight - 1;
    if (hasXAxis) {
        axisY = viewport.mathToScreenY(0.0);
        display.drawHorizontalLine(viewport.screenX,
                                   axisY,
                                   viewport.screenWidth,
                                   GRAPH_AXIS);
    }

    int axisX = viewport.screenX;
    if (hasYAxis) {
        axisX = viewport.mathToScreenX(0.0);
        display.drawVerticalLine(axisX,
                                 viewport.screenY,
                                 viewport.screenHeight,
                                 GRAPH_AXIS);
    }

    gridCount = 0;
    for (double tick = firstXGrid;
         tick <= viewport.xMax + viewport.xScale * 0.5 &&
         gridCount < MAX_GRID_LINES_PER_AXIS;
         tick += viewport.xScale, ++gridCount) {
        if (std::fabs(tick) < 1e-9) {
            continue;
        }
        const int px = viewport.mathToScreenX(tick);
        const int tickTop = clampInt(axisY - TICK_HALF_LENGTH,
                                     viewport.screenY,
                                     viewport.screenY + viewport.screenHeight - 1);
        const int tickBottom = clampInt(axisY + TICK_HALF_LENGTH,
                                        viewport.screenY,
                                        viewport.screenY + viewport.screenHeight - 1);
        display.drawVerticalLine(px, tickTop, tickBottom - tickTop + 1, GRAPH_TICK);
    }

    gridCount = 0;
    for (double tick = firstYGrid;
         tick <= viewport.yMax + viewport.yScale * 0.5 &&
         gridCount < MAX_GRID_LINES_PER_AXIS;
         tick += viewport.yScale, ++gridCount) {
        if (std::fabs(tick) < 1e-9) {
            continue;
        }
        const int py = viewport.mathToScreenY(tick);
        const int tickLeft = clampInt(axisX - TICK_HALF_LENGTH,
                                      viewport.screenX,
                                      viewport.screenX + viewport.screenWidth - 1);
        const int tickRight = clampInt(axisX + TICK_HALF_LENGTH,
                                       viewport.screenX,
                                       viewport.screenX + viewport.screenWidth - 1);
        display.drawHorizontalLine(tickLeft, py, tickRight - tickLeft + 1, GRAPH_TICK);
    }
}

GraphRenderer::FunctionRenderResult GraphRenderer::drawExpression(Display& display,
                                                                  const GraphViewport& viewport,
                                                                  const char* expression,
                                                                  Color color,
                                                                  int samplesPerPixel) const {
    bool hasPrevious = false;
    int previousX = 0;
    int previousY = 0;
    double previousMathY = 0.0;
    bool previousVisible = false;
    bool hasAnyValidPoint = false;
    GraphErrorType firstError = GraphErrorType::None;

    const int offscreenTop = viewport.screenY -
                             viewport.screenHeight * OFFSCREEN_MARGIN_MULTIPLIER;
    const int offscreenBottom = viewport.screenY + viewport.screenHeight - 1 +
                                viewport.screenHeight * OFFSCREEN_MARGIN_MULTIPLIER;
    const int sampleMultiplier = samplesPerPixel <= 0 ? 3 : samplesPerPixel;
    const int sampleCount = viewport.screenWidth > 0
        ? viewport.screenWidth * sampleMultiplier
        : 0;
    const int maxJumpPixels = viewport.screenHeight + viewport.screenHeight / 2;
    const double maxJumpMath = (viewport.yMax - viewport.yMin) * 1.5;

    for (int sample = 0; sample <= sampleCount; ++sample) {
        const double t = static_cast<double>(sample) / static_cast<double>(sampleCount);
        const double x = viewport.xMin + t * (viewport.xMax - viewport.xMin);
        const int px = viewport.mathToScreenX(x);
        double y = 0.0;
        GraphErrorType error = GraphErrorType::None;
        if (!evaluateExpression(expression, x, y, &error)) {
            if (firstError == GraphErrorType::None && error != GraphErrorType::None) {
                firstError = error;
            }
            hasPrevious = false;
            continue;
        }

        const int py = viewport.mathToScreenY(y);
        if (py < offscreenTop || py > offscreenBottom) {
            hasPrevious = false;
            continue;
        }

        hasAnyValidPoint = true;
        const bool visible = viewport.containsScreenPoint(px, py);
        const bool continuousFromPrevious =
            hasPrevious &&
            std::abs(py - previousY) <= maxJumpPixels &&
            std::fabs(y - previousMathY) <= maxJumpMath;

        if (continuousFromPrevious && (previousVisible || visible)) {
            drawClippedLine(display, viewport,
                            previousX, previousY,
                            px, py,
                            color);
        }

        previousX = px;
        previousY = py;
        previousMathY = y;
        previousVisible = visible;
        hasPrevious = true;
    }

    return {hasAnyValidPoint, firstError};
}

bool GraphRenderer::drawClippedLine(Display& display,
                                    const GraphViewport& viewport,
                                    int x0,
                                    int y0,
                                    int x1,
                                    int y1,
                                    Color color) const {
    const int left = viewport.screenX;
    const int top = viewport.screenY;
    const int right = viewport.screenX + viewport.screenWidth - 1;
    const int bottom = viewport.screenY + viewport.screenHeight - 1;

    int code0 = clipCode(x0, y0, left, top, right, bottom);
    int code1 = clipCode(x1, y1, left, top, right, bottom);

    while (true) {
        if ((code0 | code1) == 0) {
            display.drawLine(x0, y0, x1, y1, color);
            return true;
        }
        if ((code0 & code1) != 0) {
            return false;
        }

        const int outsideCode = code0 != 0 ? code0 : code1;
        int x = 0;
        int y = 0;

        if (outsideCode & TOP) {
            if (y1 == y0) {
                return false;
            }
            x = x0 + static_cast<int>(
                (static_cast<long long>(x1 - x0) * (top - y0)) / (y1 - y0));
            y = top;
        } else if (outsideCode & BOTTOM) {
            if (y1 == y0) {
                return false;
            }
            x = x0 + static_cast<int>(
                (static_cast<long long>(x1 - x0) * (bottom - y0)) / (y1 - y0));
            y = bottom;
        } else if (outsideCode & RIGHT) {
            if (x1 == x0) {
                return false;
            }
            y = y0 + static_cast<int>(
                (static_cast<long long>(y1 - y0) * (right - x0)) / (x1 - x0));
            x = right;
        } else {
            if (x1 == x0) {
                return false;
            }
            y = y0 + static_cast<int>(
                (static_cast<long long>(y1 - y0) * (left - x0)) / (x1 - x0));
            x = left;
        }

        if (outsideCode == code0) {
            x0 = x;
            y0 = y;
            code0 = clipCode(x0, y0, left, top, right, bottom);
        } else {
            x1 = x;
            y1 = y;
            code1 = clipCode(x1, y1, left, top, right, bottom);
        }
    }
}
