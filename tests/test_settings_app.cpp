#include <gtest/gtest.h>

#include "app/settings/settings_app.h"
#include "app/settings/settings_state.h"

#include "hal/display.h"

class SettingsNullDisplay : public Display {
public:
    void init() override {}
    void clear(Color) override {}
    void drawPixel(int, int, Color) override {}
    void fillRect(int, int, int, int, Color) override {}
    void drawText(const char*, int, int, Color) override {}
    void present() override {}
};

TEST(SettingsState, DefaultsMatchRequestedValues) {
    SettingsState settings;

    EXPECT_EQ(settings.angleMode, AngleMode::Radians);
    EXPECT_TRUE(settings.graphGrid);
    EXPECT_TRUE(settings.graphAxes);
    EXPECT_EQ(settings.graphResolution, GraphResolution::Medium);
    EXPECT_EQ(settings.theme, ThemeMode::Dark);
    EXPECT_EQ(settings.uiScale, UiScaleMode::Normal);
    EXPECT_EQ(settings.calculatorPrecision, 6);
    EXPECT_FALSE(settings.developer.showTouchRegions);
    EXPECT_FALSE(settings.developer.showCursorPosition);
    EXPECT_FALSE(settings.developer.showGraphBounds);
    EXPECT_FALSE(settings.developer.parserLogs);
    EXPECT_FALSE(settings.developer.inputEventLogs);
}

TEST(SettingsState, ResetRestoresDefaults) {
    SettingsState settings;
    settings.angleMode = AngleMode::Degrees;
    settings.graphGrid = false;
    settings.graphAxes = false;
    settings.graphResolution = GraphResolution::High;
    settings.theme = ThemeMode::Classic;
    settings.uiScale = UiScaleMode::Large;
    settings.calculatorPrecision = 12;
    settings.developer.showTouchRegions = true;

    settings.resetToDefaults();

    EXPECT_EQ(settings.angleMode, AngleMode::Radians);
    EXPECT_TRUE(settings.graphGrid);
    EXPECT_TRUE(settings.graphAxes);
    EXPECT_EQ(settings.graphResolution, GraphResolution::Medium);
    EXPECT_EQ(settings.theme, ThemeMode::Dark);
    EXPECT_EQ(settings.uiScale, UiScaleMode::Normal);
    EXPECT_EQ(settings.calculatorPrecision, 6);
    EXPECT_FALSE(settings.developer.showTouchRegions);
}

TEST(SettingsApp, CyclesMainSettingsAndConfirmsReset) {
    SettingsNullDisplay display;
    SettingsState settings;
    SettingsApp app(display, settings, "Simulator");

    app.enter();
    app.handleKey(Key::ENTER); // Angle Mode
    EXPECT_EQ(settings.angleMode, AngleMode::Degrees);

    app.handleKey(Key::CURSOR_DOWN);
    app.handleKey(Key::ENTER); // Graph Grid
    EXPECT_FALSE(settings.graphGrid);

    for (int i = 0; i < 6; ++i) {
        app.handleKey(Key::CURSOR_DOWN);
    }
    app.handleKey(Key::ENTER); // Reset Settings confirmation
    EXPECT_FALSE(settings.graphGrid);
    app.handleKey(Key::ENTER); // Confirm reset
    EXPECT_TRUE(settings.graphGrid);
    EXPECT_EQ(settings.angleMode, AngleMode::Radians);
}

TEST(SettingsApp, DeveloperOptionsToggleInSubmenu) {
    SettingsNullDisplay display;
    SettingsState settings;
    SettingsApp app(display, settings, "Simulator");

    app.enter();
    for (int i = 0; i < 8; ++i) {
        app.handleKey(Key::CURSOR_DOWN);
    }
    app.handleKey(Key::ENTER); // Developer Options
    app.handleKey(Key::ENTER); // Show touch regions
    EXPECT_TRUE(settings.developer.showTouchRegions);

    app.handleKey(Key::CURSOR_DOWN);
    app.handleKey(Key::ENTER); // Show cursor position
    EXPECT_TRUE(settings.developer.showCursorPosition);

    app.handleKey(Key::CLEAR); // Back to main settings
    EXPECT_TRUE(app.handleKey(Key::CLEAR)); // return-to-home request for caller
}
