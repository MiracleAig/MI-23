#pragma once

#include "app/boot/boot_manager.h"
#include "hal/keypad.h"
#include "hal/fs/axiom_fs.h"
#include "platform/host/axiom_fs_host.h"
#include "platform/host/settings_store_host.h"

#include <string>

class HostStartupBackend : public StartupBackend {
public:
    HostStartupBackend(Keypad& keypad,
                       HostSettingsStore& settingsStore,
                       const char* firmwareVersion = "Firmware: dev");

    const char* platformName() const override;
    const char* firmwareVersion() const override;

    StartupCheckResult initializeInput() override;
    StartupCheckResult loadSettings(SettingsState& settings) override;
    StartupCheckResult checkStorage() override;
    StartupCheckResult verifyResources(SettingsState& settings) override;
    StartupCheckResult startRuntime(SettingsState& settings) override;
    AxiomFS::FileSystem& filesystem();

private:
    Keypad& m_keypad;
    HostSettingsStore& m_settingsStore;
    HostAxiomFSBackend m_fsBackend;
    AxiomFS::FileSystem m_fs;
    const char* m_firmwareVersion;
    std::string m_storageRoot;
    bool m_pendingSettingsRepair;

    StartupCheckResult ensureDirectory(const std::string& path,
                                       const char* repairedMessage,
                                       const char* failureMessage);
};
