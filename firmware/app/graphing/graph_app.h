#pragma once

#include "app/graphing/graph_renderer.h"
#include "app/settings/settings_state.h"
#include "hal/keypad.h"

enum class GraphMode {
    View,
    EditEquation,
    Trace,
};

struct GraphWindow {
    double xMin;
    double xMax;
    double yMin;
    double yMax;
    double xScale;
    double yScale;
};

class GraphApp {
public:
    static constexpr int FUNCTION_COUNT = 5;

    explicit GraphApp(const SettingsState* settings = nullptr);

    void enter();
    void handleKey(Key key);
    void renderContent(Display& display, int x, int y, int w, int h);
    void requestRender();
    bool needsRender() const;

    GraphMode mode() const;
    const char* expression() const;
    const char* editExpression() const;
    const char* functionExpression(int index) const;
    bool functionEnabled(int index) const;
    int selectedFunction() const;
    int editCursor() const;
    const GraphWindow& window() const;

private:
    static constexpr int MAX_EXPRESSION_LENGTH = 63;

    struct StoredFunction {
        char expression[MAX_EXPRESSION_LENGTH + 1];
        bool enabled;
    };

    GraphRenderer m_renderer;
    const SettingsState* m_settings;
    GraphMode m_mode;
    bool m_needsRender;
    bool m_editHasError;
    GraphErrorType m_editError;
    StoredFunction m_functions[FUNCTION_COUNT];
    char m_editBuffer[MAX_EXPRESSION_LENGTH + 1];
    int m_editLength;
    int m_editCursor;
    int m_selectedFunction;
    GraphWindow m_window;
    double m_traceX;
    double m_traceY;
    bool m_traceHasPoint;

    void enterEditMode();
    void acceptEdit();
    bool commitEditBuffer(bool exitOnSuccess);
    bool isAcceptableExpression(const char* expression, GraphErrorType& error) const;
    void selectEditFunction(int index);
    void toggleSelectedFunction();
    void appendText(const char* text);
    void backspace();
    void deleteAtCursor();
    void moveCursor(int delta);
    void zoom(double factor);
    void resetWindow();
    void refreshWindowScales();
    void startTrace(int direction);
    void moveTrace(int direction);
    void cycleTraceFunction(int direction);
    void updateTracePoint();
    int nextEnabledFunction(int startIndex, int direction) const;
    GraphViewport makeViewport(int x, int y, int w, int h, int footerHeight) const;
    void buildRenderFunctions(GraphFunction* functions) const;
    void renderGraph(Display& display, int x, int y, int w, int h);
    void renderEditor(Display& display, int x, int y, int w, int h);
    void renderTraceOverlay(Display& display, const GraphViewport& viewport, int x, int y, int w, int h);
};
