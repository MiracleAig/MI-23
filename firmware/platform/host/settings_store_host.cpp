#include "platform/host/settings_store_host.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <utility>

namespace {

std::string defaultSettingsPath() {
    namespace fs = std::filesystem;

    const char* xdgStateHome = std::getenv("XDG_STATE_HOME");
    if (xdgStateHome && xdgStateHome[0] != '\0') {
        const fs::path dir = fs::path(xdgStateHome) / "mi23";
        return (dir / "settings.bin").string();
    }

    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        const fs::path dir = fs::path(home) / ".local" / "state" / "mi23";
        return (dir / "settings.bin").string();
    }

    return ".mi23_settings.bin";
}

bool ensureParentDirectory(const std::filesystem::path& path) {
    const std::filesystem::path parent = path.parent_path();
    if (parent.empty()) {
        return true;
    }

    std::error_code error;
    std::filesystem::create_directories(parent, error);
    return !error;
}

} // namespace

HostSettingsStore::HostSettingsStore()
    : m_path(defaultSettingsPath()) {}

HostSettingsStore::HostSettingsStore(std::string path)
    : m_path(std::move(path)) {}

bool HostSettingsStore::load(SettingsState& settings) {
    settings.resetToDefaults();

    std::array<uint8_t, SettingsStore::kSerializedSize> buffer{};
    FILE* file = std::fopen(m_path.c_str(), "rb");
    if (!file) {
        return false;
    }

    const std::size_t bytesRead = std::fread(buffer.data(), 1, buffer.size(), file);
    const int closeResult = std::fclose(file);
    if (closeResult != 0 || bytesRead != buffer.size()) {
        settings.resetToDefaults();
        return false;
    }

    if (!SettingsStore::deserialize(buffer.data(), buffer.size(), settings)) {
        settings.resetToDefaults();
        return false;
    }

    return true;
}

bool HostSettingsStore::save(const SettingsState& settings) {
    const std::filesystem::path filePath(m_path);
    if (!ensureParentDirectory(filePath)) {
        return false;
    }

    std::array<uint8_t, SettingsStore::kSerializedSize> buffer{};
    if (SettingsStore::serialize(settings, buffer.data(), buffer.size()) != buffer.size()) {
        return false;
    }

    FILE* file = std::fopen(m_path.c_str(), "wb");
    if (!file) {
        return false;
    }

    const std::size_t bytesWritten = std::fwrite(buffer.data(), 1, buffer.size(), file);
    const int closeResult = std::fclose(file);
    return closeResult == 0 && bytesWritten == buffer.size();
}

bool HostSettingsStore::resetToDefaults(SettingsState& settings) {
    settings.resetToDefaults();
    settings.sanitize();
    return save(settings);
}

const std::string& HostSettingsStore::path() const {
    return m_path;
}
