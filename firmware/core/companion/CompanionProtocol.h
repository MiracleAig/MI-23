#pragma once

#include "app/settings/settings_state.h"
#include "core/companion/CompanionJson.h"
#include "hal/fs/axiom_fs.h"
#include "hal/settings_store.h"

#include <cstdint>
#include <string>

namespace Companion {

class CompanionSystemActions;

struct DeviceInfo {
    const char* firmwareVersion = "dev";
    const char* hardwareRevision = "unknown";
    const char* serialNumber = "";
};

class CompanionProtocol {
public:
    CompanionProtocol(AxiomFS::FileSystem& filesystem,
                      SettingsState& settings,
                      SettingsStore& settingsStore,
                      DeviceInfo deviceInfo,
                      CompanionSystemActions* systemActions = nullptr);

    bool handleCommand(const std::string& request, std::string& response);
    void pollSystemActions(uint64_t nowMs);

private:
    AxiomFS::FileSystem& m_filesystem;
    SettingsState& m_settings;
    SettingsStore& m_settingsStore;
    DeviceInfo m_deviceInfo;
    CompanionSystemActions* m_systemActions;

    bool dispatch(int64_t id, const std::string& command, const JsonValue& request, std::string& response);
    bool handleDeviceInfo(int64_t id, std::string& response) const;
    bool handleCapabilities(int64_t id, std::string& response) const;
    bool handleStorageInfo(int64_t id, std::string& response);
    bool handleFsList(int64_t id, const JsonValue& request, std::string& response);
    bool handleFsRead(int64_t id, const JsonValue& request, std::string& response);
    bool handleFsWrite(int64_t id, const JsonValue& request, std::string& response);
    bool handleFsDelete(int64_t id, const JsonValue& request, std::string& response);
    bool handleFsMkdir(int64_t id, const JsonValue& request, std::string& response);
    bool handleStorageFormat(int64_t id, const JsonValue& request, std::string& response);
    bool handleSettingsGet(int64_t id, std::string& response) const;
    bool handleSettingsSet(int64_t id, const JsonValue& request, std::string& response);
    bool handleTerminalExec(int64_t id, const JsonValue& request, std::string& response);
    bool handleGraphsList(int64_t id, std::string& response);
    bool handlePing(int64_t id, std::string& response) const;

    bool ensureFilesystemReady(int64_t id, std::string& response);
    bool requirePath(int64_t id,
                     const JsonValue& request,
                     const char* field,
                     bool allowRoot,
                     std::string& filesystemPath,
                     std::string& response) const;
    bool requireParentDirectory(int64_t id, const std::string& filesystemPath, std::string& response);
    bool respondOk(int64_t id, const std::string& resultJson, std::string& response) const;
    bool respondError(int64_t id,
                      const char* code,
                      const std::string& message,
                      std::string& response) const;
    bool respondFsError(int64_t id,
                        AxiomFS::Status status,
                        const std::string& context,
                        std::string& response) const;
};

} // namespace Companion
