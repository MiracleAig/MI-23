#pragma once

#include "app/boot/boot_manager.h"
#include "hal/fs/axiom_fs.h"
#include "hal/keypad.h"
#include "mi23_metadata.h"
#include "platform/rp2350/axiom_fs_rp2350.h"
#include "platform/rp2350/settings_store_rp2350.h"

class RP2350StartupBackend : public StartupBackend {
public:
    RP2350StartupBackend(Keypad& keypad,
                         RP2350SettingsStore& settingsStore,
                         const char* firmwareVersion = MI23::Metadata::kFirmwareVersionLabel);

    const char* platformName() const override;
    const char* firmwareVersion() const override;

    StartupCheckResult initializeInput() override;
    StartupCheckResult loadSettings(SettingsState& settings) override;
    StartupCheckResult checkStorage() override;
    StartupCheckResult verifyResources(SettingsState& settings) override;
    StartupCheckResult startRuntime(SettingsState& settings) override;
    StartupCheckResult formatStorage() override;
    void serviceDeferredWork(SettingsState& settings) override;
    AxiomFS::FileSystem& filesystem();

private:
    Keypad& m_keypad;
    RP2350SettingsStore& m_settingsStore;
    RP2350AxiomFSBackend m_fsBackend;
    AxiomFS::FileSystem m_fs;
    const char* m_firmwareVersion;
    bool m_pendingSettingsRepair;
    bool m_settingsRepairAttempted;
};
