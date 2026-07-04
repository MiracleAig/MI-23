#pragma once

#include <cstdint>

enum class AngleMode {
    Degrees,
    Radians,
};

enum class GraphResolution {
    Low,
    Medium,
    High,
};

enum class ThemeMode {
    Dark,
    Light,
    Classic,
};

enum class UiScaleMode {
    Small,
    Normal,
    Large,
};

struct DeveloperSettings {
    bool showTouchRegions = false;
    bool showCursorPosition = false;
    bool showGraphBounds = false;
    bool parserLogs = false;
    bool inputEventLogs = false;
};

struct SettingsState {
    static constexpr AngleMode kDefaultAngleMode = AngleMode::Radians;
    static constexpr bool kDefaultGraphGrid = true;
    static constexpr bool kDefaultGraphAxes = true;
    static constexpr GraphResolution kDefaultGraphResolution = GraphResolution::Medium;
    static constexpr ThemeMode kDefaultTheme = ThemeMode::Dark;
    static constexpr UiScaleMode kDefaultUiScale = UiScaleMode::Normal;
    static constexpr int kDefaultCalculatorPrecision = 6;
    static constexpr int kMinCalculatorPrecision = 3;
    static constexpr int kMaxCalculatorPrecision = 12;

    AngleMode angleMode = AngleMode::Radians;
    bool graphGrid = true;
    bool graphAxes = true;
    GraphResolution graphResolution = GraphResolution::Medium;
    ThemeMode theme = ThemeMode::Dark;
    UiScaleMode uiScale = UiScaleMode::Normal;
    int calculatorPrecision = 6;
    DeveloperSettings developer;

    void resetToDefaults();
    bool sanitize();

    int graphSamplesPerPixel() const;
    int uiScaleValue(int normalScale = 1) const;
};

const char* angleModeLabel(AngleMode mode);
const char* graphResolutionLabel(GraphResolution resolution);
const char* themeModeLabel(ThemeMode theme);
const char* uiScaleModeLabel(UiScaleMode scale);
