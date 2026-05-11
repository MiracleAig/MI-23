#include <gtest/gtest.h>

#include "app/graphing/graph_app.h"
#include "app/graphing/graph_renderer.h"

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

    viewport = {0, 0, 100, 100, 1.0, 1.0, -1.0, 1.0};
    EXPECT_FALSE(viewport.isValid());

    viewport = {0, 0, 100, 100, -1.0, 1.0, 2.0, 2.0};
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

TEST(GraphRenderer, ReportsInvalidExpressionEvaluationFailure) {
    GraphRenderer renderer;
    double y = 0.0;

    renderer.setExpression("sin");
    EXPECT_FALSE(renderer.evaluateExpression(2.0, y));
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

TEST(GraphApp, DownKeyCanInsertXInEditor) {
    GraphApp app;

    app.handleKey(Key::ENTER);
    app.handleKey(Key::CLEAR);
    app.handleKey(Key::CLEAR);
    app.handleKey(Key::CLEAR);
    app.handleKey(Key::CURSOR_DOWN);

    EXPECT_STREQ(app.editExpression(), "x");
}
