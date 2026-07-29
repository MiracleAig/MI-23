#pragma once

#include "hal/settings_store.h"

class RP2350SettingsStore : public SettingsStore {
public:
    enum class LoadResult {
        ValidRecord,
        LegacyRecord,
        MissingOrInvalid,
        LayoutMismatch,
        FlashIoFailure,
    };

    bool load(SettingsState& settings) override;
    LoadResult loadDetailed(SettingsState& settings);
    bool save(const SettingsState& settings) override;
    bool resetToDefaults(SettingsState& settings) override;
};
