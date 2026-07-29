#include "core/companion/CompanionProtocol.h"

#include "app/graphing/graph_storage.h"
#include "core/companion/CompanionSystemActions.h"
#include "core/companion/CompanionUtils.h"
#include "hal/system_time.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>

#ifndef MI23_COMPANION_DEBUG_LOGS
#define MI23_COMPANION_DEBUG_LOGS 0
#endif

namespace Companion {

namespace {

constexpr const char* kFormatConfirmation = "FORMAT_MI23_STORAGE";

void logCompanion(const char* format, ...) {
#if MI23_COMPANION_DEBUG_LOGS
    va_list args;
    va_start(args, format);
    std::vprintf(format, args);
    va_end(args);
#else
    (void)format;
#endif
}

const char* filesystemProtocolName(const char* backendName) {
    if (!backendName) {
        return "unknown";
    }
    if (std::strstr(backendName, "LittleFS") || std::strstr(backendName, "littlefs")) {
        return "littlefs";
    }
    if (std::strstr(backendName, "Host")) {
        return "host";
    }
    return "unknown";
}

const char* statusErrorCode(AxiomFS::Status status, AxiomFS::MountFailureReason reason) {
    switch (status) {
        case AxiomFS::Status::NotMounted:
            return reason == AxiomFS::MountFailureReason::NotFormatted
                ? "storage_unformatted"
                : "fs_unavailable";
        case AxiomFS::Status::NotFound:
            return reason == AxiomFS::MountFailureReason::NotFormatted
                ? "storage_unformatted"
                : "not_found";
        case AxiomFS::Status::AlreadyExists:
            return "already_exists";
        case AxiomFS::Status::InvalidPath:
            return "path_denied";
        case AxiomFS::Status::Unsupported:
            return "unsupported";
        case AxiomFS::Status::NoSpace:
        case AxiomFS::Status::IoError:
            return "io_error";
        case AxiomFS::Status::Unknown:
        default:
            return "internal_error";
    }
}

std::string statusMessage(AxiomFS::Status status, const std::string& context) {
    std::string message = context;
    if (!message.empty()) {
        message += ": ";
    }
    message += AxiomFS::statusToString(status);
    return message;
}

bool getStringField(const JsonValue& object, const char* key, std::string& out) {
    const JsonValue* value = object.get(key);
    if (!value || !value->isString()) {
        return false;
    }
    out = value->stringValue();
    return true;
}

bool getIntegerField(const JsonValue& object, const char* key, int64_t& out) {
    const JsonValue* value = object.get(key);
    if (!value || !value->isInteger()) {
        return false;
    }
    out = value->integerValue();
    return true;
}

bool getBoolField(const JsonValue& object, const char* key, bool& out, bool defaultValue) {
    const JsonValue* value = object.get(key);
    if (!value) {
        out = defaultValue;
        return true;
    }
    if (!value->isBoolean()) {
        return false;
    }
    out = value->boolValue();
    return true;
}

const char* stringOrFallback(const char* value, const char* fallback) {
    return value ? value : fallback;
}

int positiveOrFallback(int value, int fallback) {
    return value > 0 ? value : fallback;
}

uint32_t nonZeroOrFallback(uint32_t value, uint32_t fallback) {
    return value != 0u ? value : fallback;
}

class OperationGuard {
public:
    OperationGuard(CompanionProtocol::ActiveOperation& slot,
                   CompanionProtocol::ActiveOperation operation)
        : m_slot(slot)
        , m_previous(slot) {
        m_slot = operation;
    }

    ~OperationGuard() {
        m_slot = m_previous;
    }

private:
    CompanionProtocol::ActiveOperation& m_slot;
    CompanionProtocol::ActiveOperation m_previous;
};

std::string parentPath(const std::string& filesystemPath) {
    const std::size_t slash = filesystemPath.rfind('/');
    if (slash == std::string::npos) {
        return {};
    }
    return filesystemPath.substr(0, slash);
}

std::string withoutGraphExtension(const std::string& fileName) {
    const std::string extension = GraphSessionStorage::kExtension;
    if (fileName.size() >= extension.size() &&
        fileName.compare(fileName.size() - extension.size(), extension.size(), extension) == 0) {
        return fileName.substr(0, fileName.size() - extension.size());
    }
    return fileName;
}

const char* angleModeValue(AngleMode mode) {
    return mode == AngleMode::Degrees ? "deg" : "rad";
}

const char* themeValue(ThemeMode theme) {
    switch (theme) {
        case ThemeMode::Light: return "light";
        case ThemeMode::Classic: return "classic";
        case ThemeMode::Dark:
        default:
            return "dark";
    }
}

const char* graphResolutionValue(GraphResolution resolution) {
    switch (resolution) {
        case GraphResolution::Low: return "low";
        case GraphResolution::High: return "high";
        case GraphResolution::Medium:
        default:
            return "medium";
    }
}

const char* uiScaleValue(UiScaleMode scale) {
    switch (scale) {
        case UiScaleMode::Small: return "small";
        case UiScaleMode::Large: return "large";
        case UiScaleMode::Normal:
        default:
            return "normal";
    }
}

void appendSettingsJson(const SettingsState& settings, std::string& result) {
    result += "{\"angle_mode\":";
    appendJsonString(result, angleModeValue(settings.angleMode));
    result += ",\"theme\":";
    appendJsonString(result, themeValue(settings.theme));
    result += ",\"graph_grid\":";
    result += settings.graphGrid ? "true" : "false";
    result += ",\"graph_axes\":";
    result += settings.graphAxes ? "true" : "false";
    result += ",\"graph_resolution\":";
    appendJsonString(result, graphResolutionValue(settings.graphResolution));
    result += ",\"ui_scale\":";
    appendJsonString(result, uiScaleValue(settings.uiScale));
    result += ",\"calculator_precision\":";
    result += std::to_string(settings.calculatorPrecision);
    result += "}";
}

bool parseAngleMode(const JsonValue& value, AngleMode& out) {
    if (!value.isString()) {
        return false;
    }
    if (value.stringValue() == "deg" || value.stringValue() == "degrees") {
        out = AngleMode::Degrees;
        return true;
    }
    if (value.stringValue() == "rad" || value.stringValue() == "radians") {
        out = AngleMode::Radians;
        return true;
    }
    return false;
}

bool parseTheme(const JsonValue& value, ThemeMode& out) {
    if (!value.isString()) {
        return false;
    }
    if (value.stringValue() == "dark") {
        out = ThemeMode::Dark;
        return true;
    }
    if (value.stringValue() == "light") {
        out = ThemeMode::Light;
        return true;
    }
    if (value.stringValue() == "classic") {
        out = ThemeMode::Classic;
        return true;
    }
    return false;
}

bool parseGraphResolution(const JsonValue& value, GraphResolution& out) {
    if (!value.isString()) {
        return false;
    }
    if (value.stringValue() == "low") {
        out = GraphResolution::Low;
        return true;
    }
    if (value.stringValue() == "medium") {
        out = GraphResolution::Medium;
        return true;
    }
    if (value.stringValue() == "high") {
        out = GraphResolution::High;
        return true;
    }
    return false;
}

bool parseUiScale(const JsonValue& value, UiScaleMode& out) {
    if (!value.isString()) {
        return false;
    }
    if (value.stringValue() == "small") {
        out = UiScaleMode::Small;
        return true;
    }
    if (value.stringValue() == "normal") {
        out = UiScaleMode::Normal;
        return true;
    }
    if (value.stringValue() == "large") {
        out = UiScaleMode::Large;
        return true;
    }
    return false;
}

} // namespace

CompanionProtocol::CompanionProtocol(AxiomFS::FileSystem& filesystem,
                                     SettingsState& settings,
                                     SettingsStore& settingsStore,
                                     DeviceInfo deviceInfo,
                                     CompanionSystemActions* systemActions)
    : m_filesystem(filesystem)
    , m_settings(settings)
    , m_settingsStore(settingsStore)
    , m_deviceInfo(deviceInfo)
    , m_systemActions(systemActions)
    , m_activeOperation(ActiveOperation::None) {}

bool CompanionProtocol::handleCommand(const std::string& request, std::string& response) {
    response.clear();

    if (request.size() > kMaxMessageLength) {
        return respondError(0, "invalid_argument", "Request is too long", response);
    }

    JsonValue root;
    std::string parseError;
    if (!parseJson(request, root, &parseError) || !root.isObject()) {
        logCompanion("[companion] bad json: %s\n", parseError.c_str());
        return respondError(0, "bad_json", "Malformed JSON request", response);
    }

    int64_t id = 0;
    if (!getIntegerField(root, "id", id) || id < 0) {
        return respondError(0, "missing_id", "Request must include a non-negative integer id", response);
    }

    std::string command;
    if (!getStringField(root, "cmd", command) || command.empty()) {
        return respondError(id, "missing_cmd", "Request must include a command string", response);
    }

    return dispatch(id, command, root, response);
}

void CompanionProtocol::pollSystemActions(uint64_t nowMs) {
    if (m_systemActions) {
        m_systemActions->poll(nowMs);
    }
}

bool CompanionProtocol::dispatch(int64_t id,
                                 const std::string& command,
                                 const JsonValue& request,
                                 std::string& response) {
    if (command == "protocol.ping") {
        return handlePing(id, response);
    }
    if (command == "device.info" || command == "GET_DEVICE_INFO") {
        return handleDeviceInfo(id, response);
    }
    if (isBootloaderCommand(command)) {
        return handleEnterBootloader(id, request, response);
    }
    if (command == "device.capabilities") {
        return handleCapabilities(id, response);
    }
    if (command == "storage.info") {
        return handleStorageInfo(id, response);
    }
    if (command == "fs.list") {
        return handleFsList(id, request, response);
    }
    if (command == "fs.read") {
        return handleFsRead(id, request, response);
    }
    if (command == "fs.write") {
        return handleFsWrite(id, request, response);
    }
    if (command == "fs.delete") {
        return handleFsDelete(id, request, response);
    }
    if (command == "fs.mkdir") {
        return handleFsMkdir(id, request, response);
    }
    if (command == "storage.format") {
        return handleStorageFormat(id, request, response);
    }
    if (command == "settings.get") {
        return handleSettingsGet(id, response);
    }
    if (command == "settings.set") {
        return handleSettingsSet(id, request, response);
    }
    if (command == "terminal.exec") {
        return handleTerminalExec(id, request, response);
    }
    if (command == "graphs.list") {
        return handleGraphsList(id, response);
    }

    return respondError(id, "unknown_command", "Unknown command", response);
}

bool CompanionProtocol::handlePing(int64_t id, std::string& response) const {
    return respondOk(id, "{\"pong\":true}", response);
}

bool CompanionProtocol::handleDeviceInfo(int64_t id, std::string& response) const {
    const char* productId = stringOrFallback(m_deviceInfo.productId, MI23::Metadata::kProductId);
    const char* productName = stringOrFallback(m_deviceInfo.productName, MI23::Metadata::kProductName);
    const char* firmwareVersion = stringOrFallback(m_deviceInfo.firmwareVersion, MI23::Metadata::kFirmwareVersion);
    const char* hardwareRevision = stringOrFallback(m_deviceInfo.hardwareRevision, MI23::Metadata::kHardwareRevision);
    const char* platform = stringOrFallback(m_deviceInfo.platform, MI23::Metadata::kPlatform);
    const char* deviceId = stringOrFallback(m_deviceInfo.deviceId, MI23::Metadata::kDefaultDeviceId);
    const char* serialNumber = stringOrFallback(m_deviceInfo.serialNumber, deviceId);
    const int protocolVersion = positiveOrFallback(m_deviceInfo.protocolVersion,
                                                   MI23::Metadata::kCompanionProtocolVersion);
    const int filesystemSchemaVersion = positiveOrFallback(m_deviceInfo.filesystemSchemaVersion,
                                                           MI23::Metadata::kFilesystemSchemaVersion);
    const uint32_t flashSizeBytes = nonZeroOrFallback(m_deviceInfo.flashSizeBytes,
                                                      MI23::Metadata::kFlashSizeBytes);
    const uint32_t filesystemOffsetBytes = nonZeroOrFallback(m_deviceInfo.filesystemOffsetBytes,
                                                             MI23::Metadata::kFilesystemOffsetBytes);
    const uint32_t filesystemSizeBytes = nonZeroOrFallback(m_deviceInfo.filesystemSizeBytes,
                                                           MI23::Metadata::kFilesystemSizeBytes);

    std::string result = "{";
    result += "\"model\":";
    appendJsonString(result, productName);
    result += ",\"name\":";
    appendJsonString(result, productName);
    result += ",\"firmware\":";
    appendJsonString(result, firmwareVersion);
    result += ",\"protocol\":";
    result += std::to_string(protocolVersion);
    result += ",\"transport\":\"usb_cdc\",";
    result += "\"serial\":";
    appendJsonString(result, serialNumber);
    result += ",\"hardware_revision\":";
    appendJsonString(result, hardwareRevision);
    result += ",\"product_id\":";
    appendJsonString(result, productId);
    result += ",\"product_name\":";
    appendJsonString(result, productName);
    result += ",\"device_id\":";
    appendJsonString(result, deviceId);
    result += ",\"firmware_version\":";
    appendJsonString(result, firmwareVersion);
    result += ",\"protocol_version\":";
    result += std::to_string(protocolVersion);
    result += ",\"filesystem_schema_version\":";
    result += std::to_string(filesystemSchemaVersion);
    result += ",\"platform\":";
    appendJsonString(result, platform);
    result += ",\"flash_size_bytes\":";
    result += std::to_string(flashSizeBytes);
    result += ",\"filesystem_offset_bytes\":";
    result += std::to_string(filesystemOffsetBytes);
    result += ",\"filesystem_size_bytes\":";
    result += std::to_string(filesystemSizeBytes);
    result += ",\"supports_bootsel_reboot\":";
    result += m_deviceInfo.supportsBootselReboot ? "true" : "false";
    result += ",\"supports_firmware_update\":";
    result += m_deviceInfo.supportsFirmwareUpdate ? "true" : "false";
    result += ",\"supports_file_transfer\":";
    result += m_deviceInfo.supportsFileTransfer ? "true" : "false";
    result += ",\"supports_filesystem_backup\":";
    result += m_deviceInfo.supportsFilesystemBackup ? "true" : "false";
    result += "}";
    return respondOk(id, result, response);
}

bool CompanionProtocol::handleEnterBootloader(int64_t id,
                                              const JsonValue& request,
                                              std::string& response) {
    (void)request;

    if (!isBootloaderSafeState()) {
        std::string message = "Cannot enter BOOTSEL while ";
        message += activeOperationName();
        message += " is active";
        return respondError(id, "busy", message, response);
    }

    if (!m_systemActions) {
        return respondError(id,
                            "unsupported",
                            "BOOTSEL reboot is not supported on this platform.",
                            response);
    }

    const AxiomFS::Status syncStatus = m_filesystem.sync();
    if (syncStatus != AxiomFS::Status::Ok) {
        return respondFsError(id, syncStatus, "Filesystem sync before BOOTSEL failed", response);
    }

    if (!m_settingsStore.save(m_settings)) {
        return respondError(id,
                            "io_error",
                            "Failed to persist settings before BOOTSEL reboot",
                            response);
    }

    const SystemActionResult action = m_systemActions->requestBootloader();
    if (!action.accepted) {
        return respondError(id,
                            action.effectiveErrorCode(),
                            action.errorMessage.empty()
                                ? "BOOTSEL reboot is not supported on this platform."
                                : action.errorMessage,
                            response);
    }

    std::string result = "{\"accepted\":true,\"mode\":\"bootsel\",\"reboot_scheduled\":true";
    if (action.alreadyPending) {
        result += ",\"already_pending\":true";
    }
    result += "}";
    return respondOk(id, result, response);
}

bool CompanionProtocol::handleCapabilities(int64_t id, std::string& response) const {
    return respondOk(id,
                     "{\"filesystem\":true,"
                     "\"settings\":true,"
                     "\"terminal\":true,"
                     "\"graphs\":true,"
                     "\"screenshots\":false,"
                     "\"battery\":false,"
                     "\"firmware_update\":false}",
                     response);
}

bool CompanionProtocol::handleStorageInfo(int64_t id, std::string& response) {
    AxiomFS::Diagnostics diagnostics = m_filesystem.getDiagnostics();
    const bool mounted = diagnostics.mounted || m_filesystem.isMounted();
    bool spaceKnown = diagnostics.spaceKnown;
    uint64_t totalBytes = diagnostics.totalBytes;
    uint64_t freeBytes = diagnostics.freeBytes;
    uint64_t usedBytes = diagnostics.usedBytes;

    if (mounted && !spaceKnown) {
        const AxiomFS::SpaceResult total = m_filesystem.getTotalSpace();
        const AxiomFS::SpaceResult free = m_filesystem.getFreeSpace();
        if (total.ok() && free.ok()) {
            spaceKnown = true;
            totalBytes = total.bytes;
            freeBytes = free.bytes;
            usedBytes = total.bytes >= free.bytes ? total.bytes - free.bytes : 0;
        }
    }

    const bool formatted = mounted ||
        (diagnostics.status != AxiomFS::FilesystemStatus::Unformatted &&
         diagnostics.mountFailureReason != AxiomFS::MountFailureReason::NotFormatted &&
         diagnostics.status != AxiomFS::FilesystemStatus::NotMounted);

    std::string result = "{";
    result += "\"mounted\":";
    result += mounted ? "true" : "false";
    result += ",\"formatted\":";
    result += formatted ? "true" : "false";
    result += ",\"total_bytes\":";
    result += std::to_string(spaceKnown ? totalBytes : 0u);
    result += ",\"used_bytes\":";
    result += std::to_string(spaceKnown ? usedBytes : 0u);
    result += ",\"free_bytes\":";
    result += std::to_string(spaceKnown ? freeBytes : 0u);
    result += ",\"fs_type\":";
    appendJsonString(result, filesystemProtocolName(m_filesystem.backendName()));
    result += ",\"unknown\":";
    result += spaceKnown ? "false" : "true";
    result += "}";
    return respondOk(id, result, response);
}

bool CompanionProtocol::handleFsList(int64_t id, const JsonValue& request, std::string& response) {
    OperationGuard operation(m_activeOperation, ActiveOperation::FileList);
    if (!ensureFilesystemReady(id, response)) {
        return false;
    }

    std::string fsPath;
    if (!requirePath(id, request, "path", true, fsPath, response)) {
        return false;
    }

    AxiomFS::ListResult listing = m_filesystem.listDir(fsPath.empty() ? "/" : fsPath);
    if (!listing.ok()) {
        return respondFsError(id, listing.status, "List directory failed", response);
    }

    std::string result = "{\"path\":";
    appendJsonString(result, virtualPathFromFilesystemPath(fsPath));
    result += ",\"entries\":[";
    bool first = true;
    bool truncated = false;
    for (const AxiomFS::DirectoryEntry& entry : listing.entries) {
        std::string candidate;
        if (!first) {
            candidate += ",";
        }
        candidate += "{\"name\":";
        appendJsonString(candidate, entry.name);
        candidate += ",\"type\":\"";
        candidate += entry.isDirectory ? "dir" : "file";
        candidate += "\",\"size\":";
        candidate += std::to_string(entry.size);
        candidate += "}";

        if (result.size() + candidate.size() + 32u > kMaxResponseLength) {
            truncated = true;
            break;
        }
        result += candidate;
        first = false;
    }
    result += "]";
    if (truncated) {
        result += ",\"truncated\":true";
    }
    result += "}";
    return respondOk(id, result, response);
}

bool CompanionProtocol::handleFsRead(int64_t id, const JsonValue& request, std::string& response) {
    OperationGuard operation(m_activeOperation, ActiveOperation::FileRead);
    if (!ensureFilesystemReady(id, response)) {
        return false;
    }

    std::string fsPath;
    if (!requirePath(id, request, "path", false, fsPath, response)) {
        return false;
    }

    int64_t offset = 0;
    int64_t length = 0;
    if (!getIntegerField(request, "offset", offset) || offset < 0 ||
        !getIntegerField(request, "length", length) || length < 0 ||
        static_cast<std::size_t>(length) > kMaxFileChunkSize) {
        return respondError(id, "invalid_argument", "offset and length must be valid integers within chunk limits", response);
    }

    const AxiomFS::RangeReadResult read =
        m_filesystem.readRange(fsPath, static_cast<uint64_t>(offset), static_cast<std::size_t>(length));
    if (!read.ok()) {
        return respondFsError(id, read.status, "Read file failed", response);
    }

    const std::size_t bytesToRead = read.data.size();
    const std::string encoded = base64Encode(read.data.data(), bytesToRead);

    std::string result = "{\"path\":";
    appendJsonString(result, virtualPathFromFilesystemPath(fsPath));
    result += ",\"offset\":";
    result += std::to_string(offset);
    result += ",\"data_b64\":";
    appendJsonString(result, encoded);
    result += ",\"bytes_read\":";
    result += std::to_string(bytesToRead);
    result += ",\"eof\":";
    result += read.eof ? "true" : "false";
    result += "}";
    return respondOk(id, result, response);
}

bool CompanionProtocol::handleFsWrite(int64_t id, const JsonValue& request, std::string& response) {
    OperationGuard operation(m_activeOperation, ActiveOperation::FileWrite);
    if (!ensureFilesystemReady(id, response)) {
        return false;
    }

    std::string fsPath;
    if (!requirePath(id, request, "path", false, fsPath, response)) {
        return false;
    }
    if (!requireParentDirectory(id, fsPath, response)) {
        return false;
    }

    int64_t offset = 0;
    std::string encoded;
    bool truncate = false;
    if (!getIntegerField(request, "offset", offset) || offset < 0 ||
        !getStringField(request, "data_b64", encoded) ||
        !getBoolField(request, "truncate", truncate, false)) {
        return respondError(id, "invalid_argument", "fs.write requires path, offset, data_b64, and optional truncate", response);
    }
    if (truncate && offset != 0) {
        return respondError(id, "invalid_argument", "truncate writes must use offset 0", response);
    }
    constexpr std::size_t kMaxEncodedChunkSize = ((kMaxFileChunkSize + 2u) / 3u) * 4u;
    if (encoded.size() > kMaxEncodedChunkSize) {
        return respondError(id, "invalid_argument", "Write chunk is too large", response);
    }

    std::vector<uint8_t> decoded;
    if (!base64Decode(encoded, decoded)) {
        return respondError(id, "invalid_argument", "data_b64 is not valid Base64", response);
    }
    const std::size_t bytesWritten = decoded.size();
    if (bytesWritten > kMaxFileChunkSize) {
        return respondError(id, "invalid_argument", "Write chunk is too large", response);
    }

    const uint64_t endOffset = static_cast<uint64_t>(offset) + static_cast<uint64_t>(bytesWritten);
    if (endOffset > kMaxCompanionFileSize) {
        return respondError(id, "invalid_argument", "Resulting file is too large for companion writes", response);
    }
    const AxiomFS::Status status = m_filesystem.writeRange(
        fsPath, static_cast<uint64_t>(offset), decoded.data(), decoded.size(), truncate);
    if (status != AxiomFS::Status::Ok) {
        if (status == AxiomFS::Status::InvalidPath) {
            return respondError(id, "invalid_argument", "Write offset is beyond the current file size", response);
        }
        return respondFsError(id, status, "Write file failed", response);
    }

    std::string result = "{\"path\":";
    appendJsonString(result, virtualPathFromFilesystemPath(fsPath));
    result += ",\"offset\":";
    result += std::to_string(offset);
    result += ",\"bytes_written\":";
    result += std::to_string(bytesWritten);
    result += "}";
    return respondOk(id, result, response);
}

bool CompanionProtocol::handleFsDelete(int64_t id, const JsonValue& request, std::string& response) {
    OperationGuard operation(m_activeOperation, ActiveOperation::FileDelete);
    if (!ensureFilesystemReady(id, response)) {
        return false;
    }

    std::string fsPath;
    if (!requirePath(id, request, "path", false, fsPath, response)) {
        return false;
    }

    const AxiomFS::Status status = m_filesystem.deleteFile(fsPath);
    if (status != AxiomFS::Status::Ok) {
        return respondFsError(id, status, "Delete file failed", response);
    }

    return respondOk(id, "{\"deleted\":true}", response);
}

bool CompanionProtocol::handleFsMkdir(int64_t id, const JsonValue& request, std::string& response) {
    OperationGuard operation(m_activeOperation, ActiveOperation::FileMkdir);
    if (!ensureFilesystemReady(id, response)) {
        return false;
    }

    std::string fsPath;
    if (!requirePath(id, request, "path", false, fsPath, response)) {
        return false;
    }
    if (!requireParentDirectory(id, fsPath, response)) {
        return false;
    }

    bool exists = false;
    AxiomFS::Status status = m_filesystem.exists(fsPath, exists);
    if (status != AxiomFS::Status::Ok) {
        return respondFsError(id, status, "Check directory failed", response);
    }
    if (exists) {
        return respondError(id, "already_exists", "Path already exists", response);
    }

    status = m_filesystem.createDir(fsPath);
    if (status != AxiomFS::Status::Ok) {
        return respondFsError(id, status, "Create directory failed", response);
    }

    return respondOk(id, "{\"created\":true}", response);
}

bool CompanionProtocol::handleStorageFormat(int64_t id, const JsonValue& request, std::string& response) {
    OperationGuard operation(m_activeOperation, ActiveOperation::StorageFormat);
    std::string confirm;
    if (!getStringField(request, "confirm", confirm) || confirm != kFormatConfirmation) {
        return respondError(id, "invalid_argument", "storage.format requires the exact confirmation string", response);
    }

    const AxiomFS::HealthResult result = AxiomFS::formatAndInitialize(m_filesystem);
    if (result.status != AxiomFS::FilesystemStatus::Healthy &&
        result.status != AxiomFS::FilesystemStatus::Degraded) {
        return respondFsError(id, result.mountStatus, "Format storage failed", response);
    }

    return respondOk(id, "{\"formatted\":true}", response);
}

bool CompanionProtocol::handleSettingsGet(int64_t id, std::string& response) const {
    std::string result;
    appendSettingsJson(m_settings, result);
    return respondOk(id, result, response);
}

bool CompanionProtocol::handleSettingsSet(int64_t id, const JsonValue& request, std::string& response) {
    const JsonValue* values = request.get("values");
    if (!values || !values->isObject()) {
        return respondError(id, "invalid_argument", "settings.set requires a values object", response);
    }

    SettingsState updated = m_settings;
    for (const auto& item : values->objectItems()) {
        const std::string& key = item.first;
        const JsonValue& value = item.second;
        if (key == "angle_mode") {
            if (!parseAngleMode(value, updated.angleMode)) {
                return respondError(id, "invalid_argument", "Invalid angle_mode", response);
            }
        } else if (key == "theme") {
            if (!parseTheme(value, updated.theme)) {
                return respondError(id, "invalid_argument", "Invalid theme", response);
            }
        } else if (key == "graph_grid") {
            if (!value.isBoolean()) {
                return respondError(id, "invalid_argument", "Invalid graph_grid", response);
            }
            updated.graphGrid = value.boolValue();
        } else if (key == "graph_axes") {
            if (!value.isBoolean()) {
                return respondError(id, "invalid_argument", "Invalid graph_axes", response);
            }
            updated.graphAxes = value.boolValue();
        } else if (key == "graph_resolution") {
            if (!parseGraphResolution(value, updated.graphResolution)) {
                return respondError(id, "invalid_argument", "Invalid graph_resolution", response);
            }
        } else if (key == "ui_scale") {
            if (!parseUiScale(value, updated.uiScale)) {
                return respondError(id, "invalid_argument", "Invalid ui_scale", response);
            }
        } else if (key == "calculator_precision") {
            if (!value.isInteger() ||
                value.integerValue() < SettingsState::kMinCalculatorPrecision ||
                value.integerValue() > SettingsState::kMaxCalculatorPrecision) {
                return respondError(id, "invalid_argument", "Invalid calculator_precision", response);
            }
            updated.calculatorPrecision = static_cast<int>(value.integerValue());
            SettingsState sanitized = updated;
            if (sanitized.sanitize() && sanitized.calculatorPrecision != updated.calculatorPrecision) {
                return respondError(id, "invalid_argument", "Invalid calculator_precision", response);
            }
        } else {
            return respondError(id, "invalid_argument", "Unknown setting: " + key, response);
        }
    }

    updated.sanitize();
    if (!m_settingsStore.save(updated)) {
        return respondError(id, "io_error", "Failed to persist settings", response);
    }
    m_settings = updated;

    std::string result = "{\"updated\":true,\"settings\":";
    appendSettingsJson(m_settings, result);
    result += "}";
    return respondOk(id, result, response);
}

bool CompanionProtocol::handleTerminalExec(int64_t id, const JsonValue& request, std::string& response) {
    std::string line;
    if (!getStringField(request, "line", line) || line.size() > 80u) {
        return respondError(id, "invalid_argument", "terminal.exec requires a short line string", response);
    }

    std::string output;
    if (line == "help") {
        output = "available commands: help, info, storage, capabilities, uptime, version, reboot, bootloader\n";
    } else if (line == "info") {
        output = stringOrFallback(m_deviceInfo.productName, MI23::Metadata::kProductName);
        output += " firmware ";
        output += stringOrFallback(m_deviceInfo.firmwareVersion, MI23::Metadata::kFirmwareVersion);
        output += " protocol ";
        output += std::to_string(positiveOrFallback(m_deviceInfo.protocolVersion,
                                                    MI23::Metadata::kCompanionProtocolVersion));
        output += "\n";
    } else if (line == "storage") {
        AxiomFS::Diagnostics diagnostics = m_filesystem.getDiagnostics();
        output = "storage mounted=";
        output += diagnostics.mounted ? "true" : "false";
        output += " fs=";
        output += filesystemProtocolName(m_filesystem.backendName());
        if (diagnostics.spaceKnown) {
            output += " free=";
            output += std::to_string(diagnostics.freeBytes);
            output += " total=";
            output += std::to_string(diagnostics.totalBytes);
        }
        output += "\n";
    } else if (line == "capabilities") {
        output = "filesystem=true settings=true terminal=true graphs=true screenshots=false battery=false firmware_update=false\n";
    } else if (line == "uptime") {
        output = "uptime_ms=";
        output += std::to_string(systemTimeMs());
        output += "\n";
    } else if (line == "version") {
        output = stringOrFallback(m_deviceInfo.firmwareVersion, MI23::Metadata::kFirmwareVersion);
        output += "\n";
    } else if (line == "reboot") {
        if (!m_systemActions) {
            return respondError(id,
                                "unsupported",
                                "Reboot is not supported on this platform.",
                                response);
        }
        const SystemActionResult action = m_systemActions->requestReboot();
        if (!action.accepted) {
            return respondError(id,
                                action.effectiveErrorCode(),
                                action.errorMessage.empty()
                                    ? "Reboot is not supported on this platform."
                                    : action.errorMessage,
                                response);
        }
        output = action.output.empty() ? "reboot scheduled\n" : action.output;
    } else if (line == "bootloader") {
        if (!m_systemActions) {
            return respondError(id,
                                "unsupported",
                                "USB BOOT mode is not supported on this platform.",
                                response);
        }
        const SystemActionResult action = m_systemActions->requestBootloader();
        if (!action.accepted) {
            return respondError(id,
                                action.effectiveErrorCode(),
                                action.errorMessage.empty()
                                    ? "USB BOOT mode is not supported on this platform."
                                    : action.errorMessage,
                                response);
        }
        output = action.output.empty() ? "bootloader reboot scheduled\n" : action.output;
    } else {
        return respondError(id, "invalid_argument", "Unknown terminal command", response);
    }

    std::string result = "{\"output\":";
    appendJsonString(result, output);
    result += "}";
    return respondOk(id, result, response);
}

bool CompanionProtocol::handleGraphsList(int64_t id, std::string& response) {
    if (!ensureFilesystemReady(id, response)) {
        return false;
    }

    AxiomFS::ListResult listing = GraphSessionStorage::list(m_filesystem);
    if (!listing.ok()) {
        return respondFsError(id, listing.status, "List graphs failed", response);
    }

    std::string result = "{\"graphs\":[";
    bool first = true;
    bool truncated = false;
    for (const AxiomFS::DirectoryEntry& entry : listing.entries) {
        std::string candidate;
        if (!first) {
            candidate += ",";
        }
        candidate += "{\"name\":";
        appendJsonString(candidate, withoutGraphExtension(entry.name));
        candidate += ",\"path\":";
        appendJsonString(candidate, "/graphs/" + entry.name);
        candidate += ",\"size\":";
        candidate += std::to_string(entry.size);
        candidate += "}";

        if (result.size() + candidate.size() + 32u > kMaxResponseLength) {
            truncated = true;
            break;
        }
        result += candidate;
        first = false;
    }
    result += "]";
    if (truncated) {
        result += ",\"truncated\":true";
    }
    result += "}";
    return respondOk(id, result, response);
}

bool CompanionProtocol::ensureFilesystemReady(int64_t id, std::string& response) {
    if (m_filesystem.isMounted()) {
        return true;
    }

    const AxiomFS::Status status = m_filesystem.mount();
    if (status == AxiomFS::Status::Ok) {
        return true;
    }
    return respondFsError(id, status, "Filesystem is not available", response);
}

bool CompanionProtocol::requirePath(int64_t id,
                                    const JsonValue& request,
                                    const char* field,
                                    bool allowRoot,
                                    std::string& filesystemPath,
                                    std::string& response) const {
    std::string virtualPath;
    if (!getStringField(request, field, virtualPath)) {
        return respondError(id, "invalid_argument", std::string("Missing path field: ") + field, response);
    }

    std::string pathError;
    if (!validateVirtualPath(virtualPath, allowRoot, filesystemPath, &pathError)) {
        return respondError(id, "path_denied", pathError, response);
    }
    return true;
}

bool CompanionProtocol::requireParentDirectory(int64_t id,
                                               const std::string& filesystemPath,
                                               std::string& response) {
    const std::string parent = parentPath(filesystemPath);
    if (parent.empty()) {
        return true;
    }

    bool exists = false;
    AxiomFS::Status status = m_filesystem.exists(parent, exists);
    if (status != AxiomFS::Status::Ok) {
        return respondFsError(id, status, "Check parent directory failed", response);
    }
    if (!exists) {
        return respondError(id, "not_found", "Parent directory does not exist", response);
    }

    AxiomFS::ListResult listing = m_filesystem.listDir(parent);
    if (!listing.ok()) {
        return respondError(id, "invalid_argument", "Parent path is not a directory", response);
    }
    return true;
}

bool CompanionProtocol::respondOk(int64_t id, const std::string& resultJson, std::string& response) const {
    response = "{\"id\":";
    response += std::to_string(id);
    response += ",\"ok\":true,\"result\":";
    response += resultJson;
    response += "}\n";
    if (response.size() > kMaxResponseLength) {
        return respondError(id, "internal_error", "Response is too large", response);
    }
    return true;
}

bool CompanionProtocol::respondError(int64_t id,
                                     const char* code,
                                     const std::string& message,
                                     std::string& response) const {
    response = "{\"id\":";
    response += std::to_string(id);
    response += ",\"ok\":false,\"error\":{\"code\":";
    appendJsonString(response, code ? code : "internal_error");
    response += ",\"message\":";
    appendJsonString(response, message);
    response += "}}\n";
    return false;
}

bool CompanionProtocol::respondFsError(int64_t id,
                                       AxiomFS::Status status,
                                       const std::string& context,
                                       std::string& response) const {
    const char* code = statusErrorCode(status, m_filesystem.lastMountFailureReason());
    return respondError(id, code, statusMessage(status, context), response);
}

bool CompanionProtocol::isBootloaderSafeState() const {
    return m_activeOperation == ActiveOperation::None;
}

bool CompanionProtocol::isBootloaderCommand(const std::string& command) const {
    return command == "device.enter_bootloader" || command == "ENTER_BOOTLOADER";
}

const char* CompanionProtocol::activeOperationName() const {
    switch (m_activeOperation) {
        case ActiveOperation::None:
            return "no operation";
        case ActiveOperation::FileList:
            return "filesystem listing";
        case ActiveOperation::FileRead:
            return "file download";
        case ActiveOperation::FileWrite:
            return "file upload";
        case ActiveOperation::FileDelete:
            return "file deletion";
        case ActiveOperation::FileMkdir:
            return "directory creation";
        case ActiveOperation::StorageFormat:
            return "storage format";
        default:
            return "another operation";
    }
}

} // namespace Companion
