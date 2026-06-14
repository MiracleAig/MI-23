#pragma once

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
    AngleMode angleMode = AngleMode::Radians;
    bool graphGrid = true;
    bool graphAxes = true;
    GraphResolution graphResolution = GraphResolution::Medium;
    ThemeMode theme = ThemeMode::Dark;
    UiScaleMode uiScale = UiScaleMode::Normal;
    int calculatorPrecision = 6;
    DeveloperSettings developer;

    void resetToDefaults();

    int graphSamplesPerPixel() const;
    int uiScaleValue(int normalScale = 1) const;
};

const char* angleModeLabel(AngleMode mode);
const char* graphResolutionLabel(GraphResolution resolution);
const char* themeModeLabel(ThemeMode theme);
const char* uiScaleModeLabel(UiScaleMode scale);
