#include "app/ui/system_status.h"

#include "app/home/calculator_home.h"

#include <algorithm>
#include <cstring>

SystemStatusState::SystemStatusState()
    : m_appTitle{}
    , m_batteryPercentage(kDefaultBatteryPercentage)
    , m_angleMode(AngleMode::Radians)
    , m_inputLayer(InputLayer::Base) {
    setAppTitle("Home");
}

void SystemStatusState::setAppTitle(const char* title) {
    const char* source = title ? title : "";
    std::strncpy(m_appTitle, source, kMaxAppTitleLength);
    m_appTitle[kMaxAppTitleLength] = '\0';
}

const char* SystemStatusState::appTitle() const {
    return m_appTitle;
}

void SystemStatusState::setBatteryPercentage(int percentage) {
    m_batteryPercentage = std::clamp(percentage, 0, 100);
}

int SystemStatusState::batteryPercentage() const {
    return m_batteryPercentage;
}

void SystemStatusState::setAngleMode(AngleMode mode) {
    m_angleMode = mode;
}

AngleMode SystemStatusState::angleMode() const {
    return m_angleMode;
}

void SystemStatusState::setInputLayer(InputLayer layer) {
    m_inputLayer = layer;
}

InputLayer SystemStatusState::inputLayer() const {
    return m_inputLayer;
}

const char* inputLayerLabel(InputLayer layer) {
    switch (layer) {
        case InputLayer::Second: return "L2";
        case InputLayer::Alpha: return "L3";
        case InputLayer::Base:
        default: return "L1";
    }
}

const char* compactAngleModeLabel(AngleMode mode) {
    return mode == AngleMode::Degrees ? "DEG" : "RAD";
}

const char* appTitleForId(AppId app) {
    switch (app) {
        case AppId::Home: return "Home";
        case AppId::Calculator: return "Calculator";
        case AppId::Graphing: return "Graphing";
        case AppId::Files: return "Files";
        case AppId::Settings: return "Settings";
        case AppId::Matrix: return "Matrix";
        case AppId::Boot:
        default: return "";
    }
}

bool operator==(const SystemStatusState& lhs, const SystemStatusState& rhs) {
    return std::strcmp(lhs.appTitle(), rhs.appTitle()) == 0 &&
           lhs.batteryPercentage() == rhs.batteryPercentage() &&
           lhs.angleMode() == rhs.angleMode() &&
           lhs.inputLayer() == rhs.inputLayer();
}

bool operator!=(const SystemStatusState& lhs, const SystemStatusState& rhs) {
    return !(lhs == rhs);
}
