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
    double xScale;
    double yScale;

    bool isValid() const;
    int mathToScreenX(double x) const;
    int mathToScreenY(double y) const;
    double screenToMathX(int px) const;
    double screenToMathY(int py) const;
    bool containsScreenPoint(int px, int py) const;
};

enum class GraphErrorType {
    None,
    EmptyFunction,
    InvalidExpression,
    DomainError,
    DivideByZero,
    NoEnabledFunctions,
    NoValidPoints,
};

const char* graphErrorText(GraphErrorType error);

struct GraphFunction {
    const char* expression;
    bool enabled;
    Color color;
};

struct GraphRenderResult {
    bool drewAnyFunction;
    GraphErrorType error;
};

struct GraphRenderOptions {
    bool showGrid = true;
    bool showAxes = true;
    int samplesPerPixel = 3;
};

class GraphRenderer {
public:
    static constexpr const char* DEFAULT_EXPRESSION = "x^2";

    GraphRenderer();

    const char* expression() const;
    void setExpression(const char* expression);
    bool evaluateExpression(double x, double& y) const;
    bool evaluateExpression(const char* expression,
                            double x,
                            double& y,
                            GraphErrorType* error = nullptr) const;
    GraphRenderResult render(Display& display, const GraphViewport& viewport) const;
    GraphRenderResult render(Display& display,
                             const GraphViewport& viewport,
                             const GraphFunction* functions,
                             int functionCount) const;
    GraphRenderResult render(Display& display,
                             const GraphViewport& viewport,
                             const GraphFunction* functions,
                             int functionCount,
                             const GraphRenderOptions& options) const;

private:
    struct FunctionRenderResult {
        bool hasAnyValidPoint;
        GraphErrorType error;
    };

    void drawGridAndAxes(Display& display,
                         const GraphViewport& viewport,
                         const GraphRenderOptions& options) const;
    FunctionRenderResult drawExpression(Display& display,
                                        const GraphViewport& viewport,
                                        const char* expression,
                                        Color color,
                                        int samplesPerPixel) const;
    bool drawClippedLine(Display& display,
                         const GraphViewport& viewport,
                         int x0,
                         int y0,
                         int x1,
                         int y1,
                         Color color) const;

    const char* m_expression;
};
