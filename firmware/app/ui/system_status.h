#pragma once

#include "app/settings/settings_state.h"

#include <cstddef>

enum class AppId;

enum class InputLayer {
    Base,
    Second,
    Alpha,
};

class SystemStatusState {
public:
    static constexpr int kDefaultBatteryPercentage = 85;
    static constexpr int kMaxAppTitleLength = 24;

    SystemStatusState();

    void setAppTitle(const char* title);
    const char* appTitle() const;

    void setBatteryPercentage(int percentage);
    int batteryPercentage() const;

    void setAngleMode(AngleMode mode);
    AngleMode angleMode() const;

    void setInputLayer(InputLayer layer);
    InputLayer inputLayer() const;

private:
    char m_appTitle[kMaxAppTitleLength + 1];
    int m_batteryPercentage;
    AngleMode m_angleMode;
    InputLayer m_inputLayer;
};

const char* inputLayerLabel(InputLayer layer);
const char* compactAngleModeLabel(AngleMode mode);
const char* appTitleForId(AppId app);

bool operator==(const SystemStatusState& lhs, const SystemStatusState& rhs);
bool operator!=(const SystemStatusState& lhs, const SystemStatusState& rhs);
