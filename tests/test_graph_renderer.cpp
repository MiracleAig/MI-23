#include <gtest/gtest.h>

#include "app/graphing/graph_app.h"
#include "app/graphing/graph_renderer.h"

namespace {

class NullDisplay : public Display {
public:
    void init() override {}
    void clear(Color) override {}
    void drawPixel(int, int, Color) override {}
    void fillRect(int, int, int, int, Color) override {}
    void drawText(const char*, int, int, Color) override {}
    void present() override {}
};

} // namespace

TEST(GraphViewport, ConvertsMathAndScreenCoordinates) {
    const GraphViewport viewport = {
        10,
        20,
        101,
        81,
        -10.0,
        10.0,
        -4.0,
        4.0,
        1.0,
        1.0,
    };

    EXPECT_TRUE(viewport.isValid());
    EXPECT_EQ(viewport.mathToScreenX(-10.0), 10);
    EXPECT_EQ(viewport.mathToScreenX(0.0), 60);
    EXPECT_EQ(viewport.mathToScreenX(10.0), 110);

    EXPECT_EQ(viewport.mathToScreenY(4.0), 20);
    EXPECT_EQ(viewport.mathToScreenY(0.0), 60);
    EXPECT_EQ(viewport.mathToScreenY(-4.0), 100);

    EXPECT_DOUBLE_EQ(viewport.screenToMathX(10), -10.0);
    EXPECT_DOUBLE_EQ(viewport.screenToMathX(60), 0.0);
    EXPECT_DOUBLE_EQ(viewport.screenToMathX(110), 10.0);

    EXPECT_DOUBLE_EQ(viewport.screenToMathY(20), 4.0);
    EXPECT_DOUBLE_EQ(viewport.screenToMathY(60), 0.0);
    EXPECT_DOUBLE_EQ(viewport.screenToMathY(100), -4.0);
}

TEST(GraphViewport, RejectsInvalidRangesAndSizes) {
    GraphViewport viewport = {0, 0, 1, 100, -1.0, 1.0, -1.0, 1.0};
    EXPECT_FALSE(viewport.isValid());

    viewport = {0, 0, 100, 100, 1.0, 1.0, -1.0, 1.0, 1.0, 1.0};
    EXPECT_FALSE(viewport.isValid());

    viewport = {0, 0, 100, 100, -1.0, 1.0, 2.0, 2.0, 1.0, 1.0};
    EXPECT_FALSE(viewport.isValid());

    viewport = {0, 0, 100, 100, -1.0, 1.0, -1.0, 1.0, 0.0, 1.0};
    EXPECT_FALSE(viewport.isValid());
}

TEST(GraphRenderer, EvaluatesCurrentExpressionWithX) {
    GraphRenderer renderer;
    double y = 0.0;

    renderer.setExpression("x");
    EXPECT_TRUE(renderer.evaluateExpression(2.0, y));
    EXPECT_DOUBLE_EQ(y, 2.0);

    renderer.setExpression("x+1");
    EXPECT_TRUE(renderer.evaluateExpression(2.0, y));
    EXPECT_DOUBLE_EQ(y, 3.0);
}

TEST(GraphRenderer, EvaluatesLogAndLnWithX) {
    GraphRenderer renderer;
    double y = 0.0;
    GraphErrorType error = GraphErrorType::None;

    EXPECT_TRUE(renderer.evaluateExpression("log(x)", 100.0, y, &error));
    EXPECT_NEAR(y, 2.0, 1e-5);
    EXPECT_EQ(error, GraphErrorType::None);

    EXPECT_TRUE(renderer.evaluateExpression("ln(x)", 2.71828183, y, &error));
    EXPECT_NEAR(y, 1.0, 1e-5);
    EXPECT_EQ(error, GraphErrorType::None);
}

TEST(GraphRenderer, ReportsLogDomainAsGraphDomainError) {
    GraphRenderer renderer;
    double y = 0.0;
    GraphErrorType error = GraphErrorType::None;

    EXPECT_FALSE(renderer.evaluateExpression("log(x)", 0.0, y, &error));
    EXPECT_EQ(error, GraphErrorType::DomainError);

    EXPECT_FALSE(renderer.evaluateExpression("ln(x)", -1.0, y, &error));
    EXPECT_EQ(error, GraphErrorType::DomainError);
}

TEST(GraphRenderer, ReportsInvalidExpressionEvaluationFailure) {
    GraphRenderer renderer;
    double y = 0.0;

    renderer.setExpression("sin");
    EXPECT_FALSE(renderer.evaluateExpression(2.0, y));
}

TEST(GraphRenderer, RendersMultipleEnabledFunctions) {
    NullDisplay display;
    GraphRenderer renderer;
    const GraphViewport viewport = {0, 0, 120, 90, -10.0, 10.0, -10.0, 10.0, 1.0, 1.0};
    const GraphFunction functions[] = {
        {"x^2", true, Display::WHITE},
        {"sin(x)", true, Display::GREEN},
        {"log(x)", true, Display::BLUE},
    };

    const GraphRenderResult result = renderer.render(display, viewport, functions, 3);
    EXPECT_TRUE(result.drewAnyFunction);
    EXPECT_EQ(result.error, GraphErrorType::None);
}

TEST(GraphRenderer, ReportsNoEnabledFunctions) {
    NullDisplay display;
    GraphRenderer renderer;
    const GraphViewport viewport = {0, 0, 120, 90, -10.0, 10.0, -10.0, 10.0, 1.0, 1.0};
    const GraphFunction functions[] = {
        {"x^2", false, Display::WHITE},
    };

    const GraphRenderResult result = renderer.render(display, viewport, functions, 1);
    EXPECT_FALSE(result.drewAnyFunction);
    EXPECT_EQ(result.error, GraphErrorType::NoEnabledFunctions);
}

TEST(GraphApp, OpensEditorAndAcceptsEditedExpression) {
    GraphApp app;
    EXPECT_EQ(app.mode(), GraphMode::View);
    EXPECT_STREQ(app.expression(), "x^2");

    app.handleKey(Key::ENTER);
    EXPECT_EQ(app.mode(), GraphMode::EditEquation);

    app.handleKey(Key::CLEAR);
    app.handleKey(Key::CLEAR);
    app.handleKey(Key::CLEAR);
    app.handleKey(Key::X_VAR);
    app.handleKey(Key::PLUS);
    app.handleKey(Key::NUM_1);
    EXPECT_STREQ(app.editExpression(), "x+1");

    app.handleKey(Key::ENTER);
    EXPECT_EQ(app.mode(), GraphMode::View);
    EXPECT_STREQ(app.expression(), "x+1");
}

TEST(GraphApp, RejectsInvalidEditedExpression) {
    GraphApp app;

    app.handleKey(Key::ENTER);
    app.handleKey(Key::CLEAR);
    app.handleKey(Key::CLEAR);
    app.handleKey(Key::CLEAR);
    app.handleKey(Key::SIN);
    app.handleKey(Key::ENTER);

    EXPECT_EQ(app.mode(), GraphMode::EditEquation);
    EXPECT_STREQ(app.expression(), "x^2");
}

TEST(GraphApp, DownAndUpNavigateFunctionFieldsWithoutInsertingX) {
    GraphApp app;

    app.handleKey(Key::ENTER);
    app.handleKey(Key::CURSOR_DOWN);

    EXPECT_EQ(app.selectedFunction(), 1);
    EXPECT_STREQ(app.editExpression(), "");

    app.handleKey(Key::CURSOR_UP);
    EXPECT_EQ(app.selectedFunction(), 0);
    EXPECT_STREQ(app.editExpression(), "x^2");
}

TEST(GraphApp, XKeyInsertsXInEditor) {
    GraphApp app;

    app.handleKey(Key::ENTER);
    app.handleKey(Key::CLEAR);
    app.handleKey(Key::CLEAR);
    app.handleKey(Key::CLEAR);
    app.handleKey(Key::X_VAR);

    EXPECT_STREQ(app.editExpression(), "x");
}

TEST(GraphApp, CursorInsertionBackspaceAndDeleteEditAtCursor) {
    GraphApp app;

    app.handleKey(Key::ENTER);
    app.handleKey(Key::CLEAR);
    app.handleKey(Key::CLEAR);
    app.handleKey(Key::CLEAR);
    app.handleKey(Key::X_VAR);
    app.handleKey(Key::PLUS);
    app.handleKey(Key::NUM_1);
    app.handleKey(Key::CURSOR_LEFT);
    app.handleKey(Key::NUM_2);
    EXPECT_STREQ(app.editExpression(), "x+21");
    EXPECT_EQ(app.editCursor(), 3);

    app.handleKey(Key::CLEAR);
    EXPECT_STREQ(app.editExpression(), "x+1");
    EXPECT_EQ(app.editCursor(), 2);

    app.handleKey(Key::DELETE_KEY);
    EXPECT_STREQ(app.editExpression(), "x+");
}

TEST(GraphApp, SupportsMultipleFunctionsThroughEditorSelection) {
    GraphApp app;

    app.handleKey(Key::ENTER);
    app.handleKey(Key::CURSOR_DOWN);
    EXPECT_EQ(app.selectedFunction(), 1);
    app.handleKey(Key::SIN);
    app.handleKey(Key::X_VAR);
    app.handleKey(Key::CLOSE_PAREN);
    app.handleKey(Key::ENTER);

    EXPECT_EQ(app.mode(), GraphMode::View);
    EXPECT_TRUE(app.functionEnabled(0));
    EXPECT_TRUE(app.functionEnabled(1));
    EXPECT_STREQ(app.functionExpression(1), "sin(x)");
}

TEST(GraphApp, FunctionCanBeDisabledWithoutDeletingExpression) {
    GraphApp app;

    app.handleKey(Key::ENTER);
    app.handleKey(Key::NEGATE);
    app.handleKey(Key::ENTER);

    EXPECT_FALSE(app.functionEnabled(0));
    EXPECT_STREQ(app.functionExpression(0), "x^2");
}

TEST(GraphApp, ZoomPreservesCenterAndResetRestoresDefaultWindow) {
    GraphApp app;
    const GraphWindow before = app.window();

    app.handleKey(Key::PLUS);
    const GraphWindow zoomed = app.window();
    EXPECT_DOUBLE_EQ((before.xMin + before.xMax) * 0.5,
                     (zoomed.xMin + zoomed.xMax) * 0.5);
    EXPECT_DOUBLE_EQ((before.yMin + before.yMax) * 0.5,
                     (zoomed.yMin + zoomed.yMax) * 0.5);
    EXPECT_LT(zoomed.xMax - zoomed.xMin, before.xMax - before.xMin);
    EXPECT_LT(zoomed.xScale, before.xScale);

    app.handleKey(Key::MINUS);
    const GraphWindow zoomedOut = app.window();
    EXPECT_DOUBLE_EQ(zoomedOut.xMin, before.xMin);
    EXPECT_DOUBLE_EQ(zoomedOut.xMax, before.xMax);
    EXPECT_DOUBLE_EQ(zoomedOut.yMin, before.yMin);
    EXPECT_DOUBLE_EQ(zoomedOut.yMax, before.yMax);
    EXPECT_DOUBLE_EQ(zoomedOut.xScale, before.xScale);

    app.handleKey(Key::CLEAR);
    EXPECT_DOUBLE_EQ(app.window().xMin, -10.0);
    EXPECT_DOUBLE_EQ(app.window().xMax, 10.0);
    EXPECT_DOUBLE_EQ(app.window().yMin, -10.0);
    EXPECT_DOUBLE_EQ(app.window().yMax, 10.0);
}

TEST(GraphApp, TraceModeMovesAlongEnabledFunction) {
    GraphApp app;

    app.handleKey(Key::CURSOR_RIGHT);
    EXPECT_EQ(app.mode(), GraphMode::Trace);

    app.handleKey(Key::CURSOR_RIGHT);
    EXPECT_EQ(app.mode(), GraphMode::Trace);
}
