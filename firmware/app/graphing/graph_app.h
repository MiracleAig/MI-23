#pragma once

#include "app/graphing/graph_renderer.h"
#include "hal/keypad.h"

enum class GraphMode {
    View,
    EditEquation,
};

class GraphApp {
public:
    GraphApp();

    void enter();
    void handleKey(Key key);
    void renderContent(Display& display, int x, int y, int w, int h);
    void requestRender();
    bool needsRender() const;

    GraphMode mode() const;
    const char* expression() const;
    const char* editExpression() const;

private:
    static constexpr int MAX_EXPRESSION_LENGTH = 63;

    GraphRenderer m_renderer;
    GraphMode m_mode;
    bool m_needsRender;
    bool m_editHasError;
    char m_expression[MAX_EXPRESSION_LENGTH + 1];
    char m_editBuffer[MAX_EXPRESSION_LENGTH + 1];
    int m_editLength;

    void enterEditMode();
    void acceptEdit();
    void appendText(const char* text);
    void backspace();
    void renderGraph(Display& display, int x, int y, int w, int h);
    void renderEditor(Display& display, int x, int y, int w, int h);
};
