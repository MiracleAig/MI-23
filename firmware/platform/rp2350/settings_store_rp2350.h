#pragma once

#include "hal/settings_store.h"

class RP2350SettingsStore : public SettingsStore {
public:
    bool load(SettingsState& settings) override;
    bool save(const SettingsState& settings) override;
    bool resetToDefaults(SettingsState& settings) override;
};
