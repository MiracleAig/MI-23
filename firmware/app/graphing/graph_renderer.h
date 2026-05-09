#pragma once

#include "hal/display.h"

struct GraphViewport {
    int screenX;
    int screenY;
    int screenWidth;
    int screenHeight;
    double xMin;
    double xMax;
    double yMin;
    double yMax;

    bool isValid() const;
    int mathToScreenX(double x) const;
    int mathToScreenY(double y) const;
    double screenToMathX(int px) const;
    double screenToMathY(int py) const;
    bool containsScreenPoint(int px, int py) const;
};

class GraphRenderer {
public:
    static constexpr const char* DEFAULT_EXPRESSION = "x^2";

    GraphRenderer();

    const char* expression() const;
    void setExpression(const char* expression);
    bool evaluateExpression(double x, double& y) const;
    void render(Display& display, const GraphViewport& viewport) const;

private:
    void drawAxes(Display& display, const GraphViewport& viewport) const;
    bool drawExpression(Display& display, const GraphViewport& viewport) const;
    bool drawClippedLine(Display& display,
                         const GraphViewport& viewport,
                         int x0,
                         int y0,
                         int x1,
                         int y1,
                         Color color) const;

    const char* m_expression;
};
