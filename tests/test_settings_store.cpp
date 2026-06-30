#include "app/settings/settings_state.h"
#include "hal/settings_store.h"
#include "platform/host/settings_store_host.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>

namespace {

std::filesystem::path uniqueSettingsPath(const char* name) {
    return std::filesystem::temp_directory_path() / "mi23_settings_tests" / name;
}

void expectDefaults(const SettingsState& settings) {
    EXPECT_EQ(settings.angleMode, SettingsState::kDefaultAngleMode);
    EXPECT_EQ(settings.graphGrid, SettingsState::kDefaultGraphGrid);
    EXPECT_EQ(settings.graphAxes, SettingsState::kDefaultGraphAxes);
    EXPECT_EQ(settings.graphResolution, SettingsState::kDefaultGraphResolution);
    EXPECT_EQ(settings.theme, SettingsState::kDefaultTheme);
    EXPECT_EQ(settings.uiScale, SettingsState::kDefaultUiScale);
    EXPECT_EQ(settings.calculatorPrecision, SettingsState::kDefaultCalculatorPrecision);
    EXPECT_FALSE(settings.developer.showTouchRegions);
    EXPECT_FALSE(settings.developer.showCursorPosition);
    EXPECT_FALSE(settings.developer.showGraphBounds);
    EXPECT_FALSE(settings.developer.parserLogs);
    EXPECT_FALSE(settings.developer.inputEventLogs);
}

void expectEqual(const SettingsState& actual, const SettingsState& expected) {
    EXPECT_EQ(actual.angleMode, expected.angleMode);
    EXPECT_EQ(actual.graphGrid, expected.graphGrid);
    EXPECT_EQ(actual.graphAxes, expected.graphAxes);
    EXPECT_EQ(actual.graphResolution, expected.graphResolution);
    EXPECT_EQ(actual.theme, expected.theme);
    EXPECT_EQ(actual.uiScale, expected.uiScale);
    EXPECT_EQ(actual.calculatorPrecision, expected.calculatorPrecision);
    EXPECT_EQ(actual.developer.showTouchRegions, expected.developer.showTouchRegions);
    EXPECT_EQ(actual.developer.showCursorPosition, expected.developer.showCursorPosition);
    EXPECT_EQ(actual.developer.showGraphBounds, expected.developer.showGraphBounds);
    EXPECT_EQ(actual.developer.parserLogs, expected.developer.parserLogs);
    EXPECT_EQ(actual.developer.inputEventLogs, expected.developer.inputEventLogs);
}

struct ScopedFileCleanup {
    explicit ScopedFileCleanup(std::filesystem::path path)
        : path(std::move(path)) {}

    ~ScopedFileCleanup() {
        std::error_code error;
        std::filesystem::remove(path, error);
        std::filesystem::remove(path.parent_path(), error);
    }

    std::filesystem::path path;
};

} // namespace

TEST(SettingsStore, MissingFileLoadsDefaults) {
    const auto path = uniqueSettingsPath("missing_settings.bin");
    ScopedFileCleanup cleanup(path);
    std::error_code error;
    std::filesystem::remove(path, error);

    HostSettingsStore store(path.string());
    SettingsState settings;
    settings.angleMode = AngleMode::Degrees;

    EXPECT_FALSE(store.load(settings));
    expectDefaults(settings);
}

TEST(SettingsStore, SaveThenReloadRoundTripsSettings) {
    const auto path = uniqueSettingsPath("round_trip_settings.bin");
    ScopedFileCleanup cleanup(path);

    HostSettingsStore store(path.string());
    SettingsState saved;
    saved.angleMode = AngleMode::Degrees;
    saved.graphGrid = false;
    saved.graphAxes = false;
    saved.graphResolution = GraphResolution::High;
    saved.theme = ThemeMode::Classic;
    saved.uiScale = UiScaleMode::Large;
    saved.calculatorPrecision = 12;
    saved.developer.showTouchRegions = true;
    saved.developer.showCursorPosition = true;
    saved.developer.showGraphBounds = true;
    saved.developer.parserLogs = true;
    saved.developer.inputEventLogs = true;

    ASSERT_TRUE(store.save(saved));

    SettingsState loaded;
    EXPECT_TRUE(store.load(loaded));
    expectEqual(loaded, saved);
}

TEST(SettingsStore, CorruptedBlobFallsBackToDefaults) {
    const auto path = uniqueSettingsPath("corrupt_settings.bin");
    ScopedFileCleanup cleanup(path);
    std::filesystem::create_directories(path.parent_path());

    std::array<uint8_t, SettingsStore::kSerializedSize> bytes{};
    bytes.fill(0xA5u);
    FILE* file = std::fopen(path.c_str(), "wb");
    ASSERT_NE(file, nullptr);
    ASSERT_EQ(std::fwrite(bytes.data(), 1, bytes.size(), file), bytes.size());
    ASSERT_EQ(std::fclose(file), 0);

    HostSettingsStore store(path.string());
    SettingsState loaded;
    EXPECT_FALSE(store.load(loaded));
    expectDefaults(loaded);
}

TEST(SettingsStore, VersionMismatchFallsBackToDefaults) {
    const auto path = uniqueSettingsPath("version_mismatch_settings.bin");
    ScopedFileCleanup cleanup(path);

    SettingsState settings;
    settings.angleMode = AngleMode::Degrees;

    std::array<uint8_t, SettingsStore::kSerializedSize> bytes{};
    ASSERT_EQ(SettingsStore::serialize(settings, bytes.data(), bytes.size()), bytes.size());
    bytes[4] = 0xFFu;
    bytes[5] = 0xFFu;
    bytes[6] = 0xFFu;
    bytes[7] = 0x7Fu;

    std::filesystem::create_directories(path.parent_path());
    FILE* file = std::fopen(path.c_str(), "wb");
    ASSERT_NE(file, nullptr);
    ASSERT_EQ(std::fwrite(bytes.data(), 1, bytes.size(), file), bytes.size());
    ASSERT_EQ(std::fclose(file), 0);

    HostSettingsStore store(path.string());
    SettingsState loaded;
    EXPECT_FALSE(store.load(loaded));
    expectDefaults(loaded);
}

TEST(SettingsStore, ValidationClampsInvalidValues) {
    SettingsState settings;
    settings.angleMode = static_cast<AngleMode>(-1);
    settings.graphResolution = static_cast<GraphResolution>(9);
    settings.theme = static_cast<ThemeMode>(99);
    settings.uiScale = static_cast<UiScaleMode>(-2);
    settings.calculatorPrecision = 11;

    EXPECT_TRUE(settings.sanitize());
    EXPECT_EQ(settings.angleMode, AngleMode::Degrees);
    EXPECT_EQ(settings.graphResolution, GraphResolution::High);
    EXPECT_EQ(settings.theme, ThemeMode::Classic);
    EXPECT_EQ(settings.uiScale, UiScaleMode::Small);
    EXPECT_EQ(settings.calculatorPrecision, 12);
}
