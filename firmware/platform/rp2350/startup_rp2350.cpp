#include "platform/rp2350/startup_rp2350.h"

#include "hal/fs/fs_logger.h"
#include "hardware/flash.h"
#include "platform/rp2350/axiom_fs_flash_block_device.h"
#include "platform/rp2350/axiom_fs_flash_config.h"

#include <cstdio>

namespace {

constexpr uint32_t kSettingsSectorOffset = RP2350FlashLayout::kSettingsSectorOffset;

} // namespace

RP2350StartupBackend::RP2350StartupBackend(Keypad& keypad,
                                           RP2350SettingsStore& settingsStore,
                                           const char* firmwareVersion)
    : m_keypad(keypad)
    , m_settingsStore(settingsStore)
    , m_fsBackend()
    , m_fs(m_fsBackend)
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
    std::printf("[boot][rp2350] checking flash settings sector and AxiomFS region\n");
    std::printf("[boot][rp2350] AxiomFS backend=%s configured_flash=%lu detected_flash=%lu offset=%lu size=%lu block=%lu erase=%lu program=%lu\n",
                m_fs.backendName(),
                static_cast<unsigned long>(PICO_FLASH_SIZE_BYTES),
                static_cast<unsigned long>(RP2350FlashBlockDevice::detectedFlashSize()),
                static_cast<unsigned long>(RP2350FlashLayout::kLittleFsOffset),
                static_cast<unsigned long>(RP2350FlashLayout::kLittleFsSize),
                static_cast<unsigned long>(RP2350FlashLayout::kLittleFsBlockSize),
                static_cast<unsigned long>(FLASH_SECTOR_SIZE),
                static_cast<unsigned long>(RP2350FlashLayout::kLittleFsProgramSize));
    if ((kSettingsSectorOffset % FLASH_SECTOR_SIZE) != 0u) {
        return {false, true, false, "Flash settings region is misaligned."};
    }
    if ((PICO_FLASH_SIZE_BYTES % FLASH_SECTOR_SIZE) != 0u) {
        return {false, true, false, "Flash layout is invalid for settings persistence."};
    }
    const RP2350FlashBlockDevice::LayoutError layoutError =
        RP2350FlashBlockDevice::validateLayout();
    if (layoutError != RP2350FlashBlockDevice::LayoutError::None) {
        std::printf("[boot][rp2350] AxiomFS region invalid: %s\n",
                    RP2350FlashBlockDevice::layoutErrorToString(layoutError));
        return {false, true, false, "AxiomFS region invalid; storage disabled."};
    }

    const AxiomFS::HealthResult health = AxiomFS::initialize(m_fs);
    std::printf("[boot][rp2350] AxiomFS mount/init result health=%s mount=%s reason=%s layout=%s rw=%s detail=%s\n",
                AxiomFS::filesystemStatusToString(health.status),
                AxiomFS::statusToString(health.mountStatus),
                AxiomFS::mountFailureReasonToString(health.mountFailureReason),
                AxiomFS::statusToString(health.layoutStatus),
                AxiomFS::statusToString(health.readWriteStatus),
                health.detail.c_str());
    if (health.status == AxiomFS::FilesystemStatus::Healthy) {
        AxiomFS::appendBootLog(&m_fs, AxiomFS::releaseLabel());
        return {};
    }
    if (health.mountFailureReason == AxiomFS::MountFailureReason::BackendUnavailable ||
        health.mountStatus == AxiomFS::Status::Unsupported) {
        return {true, true, true, "AxiomFS storage backend is unsupported in this build."};
    }
    if (health.status == AxiomFS::FilesystemStatus::Unformatted) {
        std::printf("[boot][rp2350] AxiomFS unformatted: blank filesystem region detected; no automatic format performed\n");
        return {true, true, true, "AxiomFS needs format; use storage recovery."};
    }
    if (health.mountFailureReason == AxiomFS::MountFailureReason::RegionInvalid) {
        return {false, true, false, "AxiomFS region invalid; storage disabled."};
    }
    if (health.mountFailureReason == AxiomFS::MountFailureReason::FlashProbeFailed) {
        return {false, true, false, "AxiomFS flash probe failed; storage disabled."};
    }
    if (health.mountFailureReason == AxiomFS::MountFailureReason::Corrupt) {
        return {false, true, false, "AxiomFS corrupted; storage disabled."};
    }
    AxiomFS::appendBootLog(&m_fs, "AxiomFS health check failed or degraded.");
    return {false, true, false, "AxiomFS mount failed; storage disabled."};
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

AxiomFS::FileSystem& RP2350StartupBackend::filesystem() {
    return m_fs;
}
