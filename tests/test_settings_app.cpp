#include <gtest/gtest.h>

#include "app/settings/settings_app.h"
#include "app/settings/settings_state.h"
#include "platform/host/axiom_fs_host.h"

#include "hal/display.h"

#include <filesystem>

#ifndef MI23_ENABLE_DEVELOPER_OPTIONS
#define MI23_ENABLE_DEVELOPER_OPTIONS 0
#endif

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
#if MI23_ENABLE_DEVELOPER_OPTIONS
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
#else
    GTEST_SKIP() << "Developer Options menu is compile-gated off.";
#endif
}

TEST(SettingsApp, FormatStorageInitializesFilesystemLayout) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "mi23_settings_storage_tests";
    std::error_code error;
    std::filesystem::remove_all(root, error);

    SettingsNullDisplay display;
    SettingsState settings;
    HostAxiomFSBackend backend(root);
    AxiomFS::FileSystem fs(backend);
    ASSERT_EQ(fs.mount(), AxiomFS::Status::Ok);
    ASSERT_EQ(fs.writeFile("graphs/old.mi23graph", "old"), AxiomFS::Status::Ok);

    SettingsApp app(display, settings, "Simulator", &fs);
    app.enter();
    const int storageIndex = MI23_ENABLE_DEVELOPER_OPTIONS ? 10 : 9;
    for (int i = 0; i < storageIndex; ++i) {
        app.handleKey(Key::CURSOR_DOWN);
    }
    app.handleKey(Key::ENTER); // Storage Manager
    app.handleKey(Key::CURSOR_DOWN);
    app.handleKey(Key::CURSOR_DOWN);
    app.handleKey(Key::ENTER); // Format storage confirmation
    app.handleKey(Key::ENTER); // Confirm format

    EXPECT_EQ(AxiomFS::getLastHealthResult().status, AxiomFS::FilesystemStatus::Healthy);
    for (int i = 0; i < AxiomFS::defaultDirectoryCount(); ++i) {
        EXPECT_TRUE(std::filesystem::is_directory(root / AxiomFS::defaultDirectories()[i]));
    }
    EXPECT_FALSE(std::filesystem::exists(root / "graphs" / "old.mi23graph"));

    std::filesystem::remove_all(root, error);
}
