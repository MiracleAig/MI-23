#pragma once

#include "app/settings/settings_state.h"
#include "core/companion/CompanionJson.h"
#include "hal/fs/axiom_fs.h"
#include "hal/settings_store.h"
#include "mi23_metadata.h"

#include <cstdint>
#include <string>

namespace Companion {

class CompanionSystemActions;

struct DeviceInfo {
    const char* firmwareVersion = MI23::Metadata::kFirmwareVersion;
    const char* hardwareRevision = MI23::Metadata::kHardwareRevision;
    const char* serialNumber = MI23::Metadata::kDefaultDeviceId;
    const char* deviceId = MI23::Metadata::kDefaultDeviceId;
    const char* productId = MI23::Metadata::kProductId;
    const char* productName = MI23::Metadata::kProductName;
    int protocolVersion = MI23::Metadata::kCompanionProtocolVersion;
    int filesystemSchemaVersion = MI23::Metadata::kFilesystemSchemaVersion;
    const char* platform = MI23::Metadata::kPlatform;
    uint32_t flashSizeBytes = MI23::Metadata::kFlashSizeBytes;
    uint32_t filesystemOffsetBytes = MI23::Metadata::kFilesystemOffsetBytes;
    uint32_t filesystemSizeBytes = MI23::Metadata::kFilesystemSizeBytes;
    bool supportsBootselReboot = MI23::Metadata::kSupportsBootselReboot;
    bool supportsFirmwareUpdate = MI23::Metadata::kSupportsFirmwareUpdate;
    bool supportsFileTransfer = MI23::Metadata::kSupportsFileTransfer;
    bool supportsFilesystemBackup = MI23::Metadata::kSupportsFilesystemBackup;
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

    enum class ActiveOperation {
        None,
        FileList,
        FileRead,
        FileWrite,
        FileDelete,
        FileMkdir,
        StorageFormat,
    };

private:
    AxiomFS::FileSystem& m_filesystem;
    SettingsState& m_settings;
    SettingsStore& m_settingsStore;
    DeviceInfo m_deviceInfo;
    CompanionSystemActions* m_systemActions;
    ActiveOperation m_activeOperation;

    bool dispatch(int64_t id, const std::string& command, const JsonValue& request, std::string& response);
    bool handleDeviceInfo(int64_t id, std::string& response) const;
    bool handleEnterBootloader(int64_t id, const JsonValue& request, std::string& response);
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
    bool isBootloaderSafeState() const;
    bool isBootloaderCommand(const std::string& command) const;
    const char* activeOperationName() const;
};

} // namespace Companion
