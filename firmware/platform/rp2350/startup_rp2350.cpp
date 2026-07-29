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
    , m_settingsRepairAttempted(false)
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
    const RP2350SettingsStore::LoadResult loadResult = m_settingsStore.loadDetailed(settings);
    if (loadResult == RP2350SettingsStore::LoadResult::ValidRecord) {
        return {};
    }
    if (loadResult == RP2350SettingsStore::LoadResult::LayoutMismatch ||
        loadResult == RP2350SettingsStore::LoadResult::FlashIoFailure) {
        settings.resetToDefaults();
        settings.sanitize();
        return {true, true, true, loadResult == RP2350SettingsStore::LoadResult::LayoutMismatch
            ? "Settings layout mismatch; defaults active."
            : "Settings flash read failed; defaults active."};
    }

    if (loadResult != RP2350SettingsStore::LoadResult::LegacyRecord) {
        settings.resetToDefaults();
    }
    settings.sanitize();
    // Loading is deliberately read-only. Flash repair is serviced only after
    // Home is available, so invalid settings cannot stall early boot.
    m_pendingSettingsRepair = true; // includes legacy migration
    m_settingsRepairAttempted = false;
    return {true, true, true,
            loadResult == RP2350SettingsStore::LoadResult::LegacyRecord
                ? "Legacy settings loaded; migration deferred."
                : "Settings recovery deferred; defaults active."};
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

    const AxiomFS::HealthResult health = AxiomFS::initializeForBoot(m_fs, "[fs][rp2350]");
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
        std::printf("[boot][rp2350] AxiomFS unformatted: storage was not formatted automatically\n");
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
    (void)settings;
    std::printf("[boot][rp2350] verifying storage resources\n");
    if (m_pendingSettingsRepair) {
        return {true, true, true, "Settings repair queued until Home is ready."};
    }
    return {};
}

StartupCheckResult RP2350StartupBackend::formatStorage() {
    std::printf("[boot][rp2350] explicit storage recovery requested\n");
    const AxiomFS::HealthResult result = AxiomFS::formatAndInitialize(m_fs);
    if (result.status != AxiomFS::FilesystemStatus::Healthy ||
        !result.mounted || !result.defaultLayoutReady || !result.readWriteReady) {
        return {false, true, false, "Format or verification failed; storage preserved if possible."};
    }
    return {true, true, true, "Storage formatted and verified."};
}

void RP2350StartupBackend::serviceDeferredWork(SettingsState& settings) {
    if (!m_pendingSettingsRepair || m_settingsRepairAttempted) return;
    m_settingsRepairAttempted = true;
    if (m_settingsStore.save(settings)) {
        m_pendingSettingsRepair = false;
        std::printf("[settings][rp2350] deferred repair complete\n");
    } else {
        std::printf("[settings][rp2350] warning: deferred repair failed; defaults remain temporary\n");
    }
}

StartupCheckResult RP2350StartupBackend::startRuntime(SettingsState& settings) {
    (void)settings;
    std::printf("[boot][rp2350] runtime ready\n");
    return {};
}

AxiomFS::FileSystem& RP2350StartupBackend::filesystem() {
    return m_fs;
}
