#include "app/settings/settings_state.h"

#include <algorithm>
#include <cstdlib>
#include <type_traits>

void SettingsState::resetToDefaults() {
    angleMode = kDefaultAngleMode;
    graphGrid = kDefaultGraphGrid;
    graphAxes = kDefaultGraphAxes;
    graphResolution = kDefaultGraphResolution;
    theme = kDefaultTheme;
    uiScale = kDefaultUiScale;
    calculatorPrecision = kDefaultCalculatorPrecision;
    developer = {};
}

bool SettingsState::sanitize() {
    bool changed = false;

    auto clampEnum = [&changed](auto& value, int minValue, int maxValue, auto defaultValue) {
        using EnumType = std::remove_reference_t<decltype(value)>;
        const int numeric = static_cast<int>(value);
        if (numeric < minValue) {
            value = static_cast<EnumType>(minValue);
            changed = true;
        } else if (numeric > maxValue) {
            value = static_cast<EnumType>(maxValue);
            changed = true;
        }
        if (static_cast<int>(value) < minValue || static_cast<int>(value) > maxValue) {
            value = defaultValue;
            changed = true;
        }
    };

    clampEnum(angleMode, static_cast<int>(AngleMode::Degrees), static_cast<int>(AngleMode::Radians), kDefaultAngleMode);
    clampEnum(graphResolution,
              static_cast<int>(GraphResolution::Low),
              static_cast<int>(GraphResolution::High),
              kDefaultGraphResolution);
    clampEnum(theme, static_cast<int>(ThemeMode::Dark), static_cast<int>(ThemeMode::Classic), kDefaultTheme);
    clampEnum(uiScale, static_cast<int>(UiScaleMode::Small), static_cast<int>(UiScaleMode::Large), kDefaultUiScale);

    constexpr int kPrecisionValues[] = {3, 6, 9, 12};
    bool precisionValid = false;
    int bestPrecision = kPrecisionValues[0];
    int bestDistance = std::abs(calculatorPrecision - bestPrecision);
    for (const int candidate : kPrecisionValues) {
        if (candidate == calculatorPrecision) {
            precisionValid = true;
            break;
        }
        const int distance = std::abs(calculatorPrecision - candidate);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestPrecision = candidate;
        }
    }

    if (!precisionValid) {
        calculatorPrecision = bestPrecision;
        changed = true;
    }

    return changed;
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
