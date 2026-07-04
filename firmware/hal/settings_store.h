#pragma once

#include "app/settings/settings_state.h"

#include <cstddef>
#include <cstdint>

class SettingsStore {
public:
    virtual ~SettingsStore() = default;

    virtual bool load(SettingsState& settings) = 0;
    virtual bool save(const SettingsState& settings) = 0;

    virtual bool resetToDefaults(SettingsState& settings) {
        settings.resetToDefaults();
        settings.sanitize();
        return save(settings);
    }

    static constexpr uint32_t kMagic = 0x4D493233u; // "MI23"
    static constexpr uint32_t kVersion = 1u;
    static constexpr std::size_t kSerializedSize = 32u;

    static bool deserialize(const uint8_t* data, std::size_t size, SettingsState& settings);
    static std::size_t serialize(const SettingsState& settings, uint8_t* out, std::size_t size);
};
