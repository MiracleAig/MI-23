#pragma once

#include "app/boot/boot_manager.h"
#include "hal/keypad.h"
#include "platform/rp2350/settings_store_rp2350.h"

class RP2350StartupBackend : public StartupBackend {
public:
    RP2350StartupBackend(Keypad& keypad,
                         RP2350SettingsStore& settingsStore,
                         const char* firmwareVersion = "Firmware: dev");

    const char* platformName() const override;
    const char* firmwareVersion() const override;

    StartupCheckResult initializeInput() override;
    StartupCheckResult loadSettings(SettingsState& settings) override;
    StartupCheckResult checkStorage() override;
    StartupCheckResult verifyResources(SettingsState& settings) override;
    StartupCheckResult startRuntime(SettingsState& settings) override;

private:
    Keypad& m_keypad;
    RP2350SettingsStore& m_settingsStore;
    const char* m_firmwareVersion;
    bool m_pendingSettingsRepair;
};
