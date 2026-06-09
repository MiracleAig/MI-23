#include "app/settings/settings_state.h"

#include <algorithm>

void SettingsState::resetToDefaults() {
    angleMode = AngleMode::Radians;
    graphGrid = true;
    graphAxes = true;
    graphResolution = GraphResolution::Medium;
    theme = ThemeMode::Dark;
    uiScale = UiScaleMode::Normal;
    calculatorPrecision = 6;
    developer = {};
}

int SettingsState::graphSamplesPerPixel() const {
    switch (graphResolution) {
        case GraphResolution::Low: return 1;
        case GraphResolution::Medium: return 3;
        case GraphResolution::High: return 5;
        default: return 3;
    }
}

int SettingsState::uiScaleValue(int normalScale) const {
    const int base = std::max(1, normalScale);
    switch (uiScale) {
        case UiScaleMode::Small: return 1;
        case UiScaleMode::Normal: return base;
        case UiScaleMode::Large: return std::max(base + 1, 2);
        default: return base;
    }
}

const char* angleModeLabel(AngleMode mode) {
    switch (mode) {
        case AngleMode::Degrees: return "Degrees";
        case AngleMode::Radians: return "Radians";
        default: return "Radians";
    }
}

const char* graphResolutionLabel(GraphResolution resolution) {
    switch (resolution) {
        case GraphResolution::Low: return "Low";
        case GraphResolution::Medium: return "Medium";
        case GraphResolution::High: return "High";
        default: return "Medium";
    }
}

const char* themeModeLabel(ThemeMode theme) {
    switch (theme) {
        case ThemeMode::Dark: return "Dark";
        case ThemeMode::Light: return "Light";
        case ThemeMode::Classic: return "Classic";
        default: return "Dark";
    }
}

const char* uiScaleModeLabel(UiScaleMode scale) {
    switch (scale) {
        case UiScaleMode::Small: return "Small";
        case UiScaleMode::Normal: return "Normal";
        case UiScaleMode::Large: return "Large";
        default: return "Normal";
    }
}
