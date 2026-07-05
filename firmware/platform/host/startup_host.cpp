#include "platform/host/startup_host.h"

#include "hal/fs/fs_logger.h"

#include <cstdio>
#include <filesystem>

namespace {

std::string parentDirectory(const std::string& path) {
    const std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (parent.empty()) {
        return ".";
    }
    return parent.string();
}

} // namespace

HostStartupBackend::HostStartupBackend(Keypad& keypad,
                                       HostSettingsStore& settingsStore,
                                       const char* firmwareVersion)
    : m_keypad(keypad)
    , m_settingsStore(settingsStore)
    , m_fsBackend()
    , m_fs(m_fsBackend)
    , m_firmwareVersion(firmwareVersion)
    , m_storageRoot(parentDirectory(settingsStore.path()))
    , m_pendingSettingsRepair(false)
{}

const char* HostStartupBackend::platformName() const {
    return "Simulator";
}

const char* HostStartupBackend::firmwareVersion() const {
    return m_firmwareVersion;
}

StartupCheckResult HostStartupBackend::initializeInput() {
    std::printf("[boot][host] initializing input\n");
    m_keypad.init();
    return {};
}

StartupCheckResult HostStartupBackend::loadSettings(SettingsState& settings) {
    namespace fs = std::filesystem;

    std::printf("[boot][host] loading settings from %s\n", m_settingsStore.path().c_str());

    std::error_code error;
    const fs::path settingsPath(m_settingsStore.path());
    const bool exists = fs::exists(settingsPath, error);
    if (error) {
        settings.resetToDefaults();
        settings.sanitize();
        m_pendingSettingsRepair = true;
        return {false, true, false, "Settings path is unavailable; using defaults."};
    }

    if (!exists) {
        settings.resetToDefaults();
        settings.sanitize();
        m_pendingSettingsRepair = !m_settingsStore.save(settings);
        return {true, true, true,
                m_pendingSettingsRepair
                    ? "Settings file missing; defaults loaded."
                    : "Settings file missing; defaults saved."};
    }

    if (m_settingsStore.load(settings)) {
        return {};
    }

    settings.resetToDefaults();
    settings.sanitize();
    m_pendingSettingsRepair = !m_settingsStore.save(settings);
    return {true, true, true,
            m_pendingSettingsRepair
                ? "Settings invalid; defaults active."
                : "Settings invalid; defaults restored."};
}

StartupCheckResult HostStartupBackend::checkStorage() {
    std::printf("[boot][host] running AxiomFS health check\n");
    const AxiomFS::HealthResult health = AxiomFS::initialize(m_fs);
    if (health.status != AxiomFS::FilesystemStatus::Healthy) {
        AxiomFS::appendBootLog(&m_fs, "AxiomFS health check failed or degraded.");
        return {false, true, false, "AxiomFS degraded; persistence is limited."};
    }
    AxiomFS::appendBootLog(&m_fs, AxiomFS::releaseLabel());

    std::printf("[boot][host] checking settings storage root %s\n", m_storageRoot.c_str());
    StartupCheckResult result = ensureDirectory(m_storageRoot,
                                                "Storage folder recreated.",
                                                "Storage folder unavailable; continuing without persistence.");
    if (!result.ok) {
        return result;
    }

    return result.repaired ? result : StartupCheckResult{};
}

StartupCheckResult HostStartupBackend::verifyResources(SettingsState& settings) {
    (void)settings;

    std::printf("[boot][host] verifying filesystem resources\n");
    if (m_pendingSettingsRepair) {
        if (m_settingsStore.save(settings)) {
            m_pendingSettingsRepair = false;
            return {true, true, true, "Recovered settings were saved."};
        }
        return {false, true, false, "Storage remains unavailable; defaults are temporary."};
    }

    return {};
}

StartupCheckResult HostStartupBackend::startRuntime(SettingsState& settings) {
    (void)settings;
    std::printf("[boot][host] runtime ready\n");
    return {};
}

StartupCheckResult HostStartupBackend::ensureDirectory(const std::string& path,
                                                       const char* repairedMessage,
                                                       const char* failureMessage) {
    namespace fs = std::filesystem;

    std::error_code error;
    const fs::path directory(path);
    if (fs::exists(directory, error)) {
        if (error) {
            return {false, true, false, failureMessage};
        }
        if (!fs::is_directory(directory, error) || error) {
            return {false, true, false, failureMessage};
        }
        return {};
    }

    fs::create_directories(directory, error);
    if (error) {
        return {false, true, false, failureMessage};
    }

    return {true, true, true, repairedMessage};
}

AxiomFS::FileSystem& HostStartupBackend::filesystem() {
    return m_fs;
}
