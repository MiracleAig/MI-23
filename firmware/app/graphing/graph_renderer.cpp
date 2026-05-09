#include "app/graphing/graph_renderer.h"

#include "math/expression.h"

#include <cmath>

namespace {

const Color GRAPH_BG = Display::rgb(4, 7, 10);
const Color GRAPH_BORDER = Display::rgb(108, 118, 132);
const Color GRAPH_AXIS = Display::rgb(185, 195, 205);
const Color GRAPH_TICK = Display::rgb(130, 144, 158);
const Color GRAPH_CURVE = Display::rgb(255, 230, 95);
const Color GRAPH_ERROR = Display::rgb(255, 110, 110);

constexpr int TICK_HALF_LENGTH = 3;
constexpr int MAX_TICKS_PER_AXIS = 64;
constexpr int OFFSCREEN_MARGIN_MULTIPLIER = 4;

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

} // namespace

bool GraphViewport::isValid() const {
    return screenWidth > 1 &&
           screenHeight > 1 &&
           xMin < xMax &&
           yMin < yMax &&
           isFinite(xMin) &&
           isFinite(xMax) &&
           isFinite(yMin) &&
           isFinite(yMax);
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
    const ExprResult result = evaluateWithX(m_expression, static_cast<float>(x));
    if (!result.ok || !isFinite(result.value)) {
        return false;
    }
    y = static_cast<double>(result.value);
    return isFinite(y);
}

void GraphRenderer::render(Display& display, const GraphViewport& viewport) const {
    if (!viewport.isValid()) {
        return;
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

    drawAxes(display, viewport);
    if (!drawExpression(display, viewport)) {
        display.drawText("Graph error",
                         viewport.screenX + 4,
                         viewport.screenY + 4,
                         GRAPH_ERROR);
    }
}

void GraphRenderer::drawAxes(Display& display, const GraphViewport& viewport) const {
    const bool hasXAxis = viewport.yMin <= 0.0 && viewport.yMax >= 0.0;
    const bool hasYAxis = viewport.xMin <= 0.0 && viewport.xMax >= 0.0;

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

    const int firstXTick = static_cast<int>(std::ceil(viewport.xMin));
    const int lastXTick = static_cast<int>(std::floor(viewport.xMax));
    int tickCount = 0;
    for (int tick = firstXTick;
         tick <= lastXTick && tickCount < MAX_TICKS_PER_AXIS;
         tick++, tickCount++) {
        if (tick == 0) {
            continue;
        }
        const int px = viewport.mathToScreenX(static_cast<double>(tick));
        const int tickTop = clampInt(axisY - TICK_HALF_LENGTH,
                                     viewport.screenY,
                                     viewport.screenY + viewport.screenHeight - 1);
        const int tickBottom = clampInt(axisY + TICK_HALF_LENGTH,
                                        viewport.screenY,
                                        viewport.screenY + viewport.screenHeight - 1);
        display.drawVerticalLine(px, tickTop, tickBottom - tickTop + 1, GRAPH_TICK);
    }

    const int firstYTick = static_cast<int>(std::ceil(viewport.yMin));
    const int lastYTick = static_cast<int>(std::floor(viewport.yMax));
    tickCount = 0;
    for (int tick = firstYTick;
         tick <= lastYTick && tickCount < MAX_TICKS_PER_AXIS;
         tick++, tickCount++) {
        if (tick == 0) {
            continue;
        }
        const int py = viewport.mathToScreenY(static_cast<double>(tick));
        const int tickLeft = clampInt(axisX - TICK_HALF_LENGTH,
                                      viewport.screenX,
                                      viewport.screenX + viewport.screenWidth - 1);
        const int tickRight = clampInt(axisX + TICK_HALF_LENGTH,
                                       viewport.screenX,
                                       viewport.screenX + viewport.screenWidth - 1);
        display.drawHorizontalLine(tickLeft, py, tickRight - tickLeft + 1, GRAPH_TICK);
    }
}

bool GraphRenderer::drawExpression(Display& display, const GraphViewport& viewport) const {
    bool hasPrevious = false;
    int previousX = 0;
    int previousY = 0;
    bool previousVisible = false;
    bool hasAnyValidPoint = false;

    const int offscreenTop = viewport.screenY -
                             viewport.screenHeight * OFFSCREEN_MARGIN_MULTIPLIER;
    const int offscreenBottom = viewport.screenY + viewport.screenHeight - 1 +
                                viewport.screenHeight * OFFSCREEN_MARGIN_MULTIPLIER;

    for (int px = viewport.screenX;
         px < viewport.screenX + viewport.screenWidth;
         px++) {
        const double x = viewport.screenToMathX(px);
        double y = 0.0;
        if (!evaluateExpression(x, y)) {
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

        if (hasPrevious && (previousVisible || visible)) {
            drawClippedLine(display, viewport,
                            previousX, previousY,
                            px, py,
                            GRAPH_CURVE);
        }

        previousX = px;
        previousY = py;
        previousVisible = visible;
        hasPrevious = true;
    }

    return hasAnyValidPoint;
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
