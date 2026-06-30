#pragma once

#include "hal/settings_store.h"

#include <string>

class HostSettingsStore : public SettingsStore {
public:
    HostSettingsStore();
    explicit HostSettingsStore(std::string path);

    bool load(SettingsState& settings) override;
    bool save(const SettingsState& settings) override;
    bool resetToDefaults(SettingsState& settings) override;

    const std::string& path() const;

private:
    std::string m_path;
};
