#include "platform/rp2350/startup_rp2350.h"

#include "hardware/flash.h"

#include <cstdio>

namespace {

constexpr uint32_t kSettingsSectorOffset = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;

} // namespace

RP2350StartupBackend::RP2350StartupBackend(Keypad& keypad,
                                           RP2350SettingsStore& settingsStore,
                                           const char* firmwareVersion)
    : m_keypad(keypad)
    , m_settingsStore(settingsStore)
    , m_firmwareVersion(firmwareVersion)
    , m_pendingSettingsRepair(false)
{}

const char* RP2350StartupBackend::platformName() const {
    return "RP2350";
}

const char* RP2350StartupBackend::firmwareVersion() const {
    return m_firmwareVersion;
}

StartupCheckResult RP2350StartupBackend::initializeInput() {
    std::printf("[boot][rp2350] initializing input\n");
    m_keypad.init();
    return {};
}

StartupCheckResult RP2350StartupBackend::loadSettings(SettingsState& settings) {
    std::printf("[boot][rp2350] loading settings from flash\n");
    if (m_settingsStore.load(settings)) {
        return {};
    }

    settings.resetToDefaults();
    settings.sanitize();
    m_pendingSettingsRepair = !m_settingsStore.save(settings);
    return {true, true, true,
            m_pendingSettingsRepair
                ? "Settings recovery deferred; defaults active."
                : "Settings restored to defaults."};
}

StartupCheckResult RP2350StartupBackend::checkStorage() {
    std::printf("[boot][rp2350] checking flash settings sector\n");
    if ((kSettingsSectorOffset % FLASH_SECTOR_SIZE) != 0u) {
        return {false, true, false, "Flash settings region is misaligned."};
    }
    if ((PICO_FLASH_SIZE_BYTES % FLASH_SECTOR_SIZE) != 0u) {
        return {false, true, false, "Flash layout is invalid for settings persistence."};
    }
    return {};
}

StartupCheckResult RP2350StartupBackend::verifyResources(SettingsState& settings) {
    std::printf("[boot][rp2350] verifying storage resources\n");
    if (m_pendingSettingsRepair) {
        if (m_settingsStore.save(settings)) {
            m_pendingSettingsRepair = false;
            return {true, true, true, "Recovered settings written to flash."};
        }
        return {false, true, false, "Flash writes failed; continuing without persistence."};
    }
    return {};
}

StartupCheckResult RP2350StartupBackend::startRuntime(SettingsState& settings) {
    (void)settings;
    std::printf("[boot][rp2350] runtime ready\n");
    return {};
}
