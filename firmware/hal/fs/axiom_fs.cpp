#include "hal/fs/axiom_fs.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>

namespace AxiomFS {

namespace {

constexpr const char* kReleaseLabel = "MI-23 Filesystem Alpha";

constexpr const char* kDefaultDirectories[] = {
    "settings",
    "programs",
    "graphs",
    "notes",
    "themes",
    "logs",
    "cache",
    "screenshots",
};

HealthResult g_lastHealthResult{};

void setDetail(HealthResult& result, const char* detail) {
    result.detail = detail ? detail : "";
}

Status countFiles(FileSystem& fs, const std::string& path, int& count) {
    ListResult listing = fs.listDir(path.empty() ? "/" : path);
    if (!listing.ok()) {
        return listing.status;
    }

    for (const DirectoryEntry& entry : listing.entries) {
        const std::string child = path.empty() ? entry.name : path + "/" + entry.name;
        if (entry.isDirectory) {
            const Status status = countFiles(fs, child, count);
            if (status != Status::Ok) {
                return status;
            }
        } else {
            count++;
        }
    }

    return Status::Ok;
}

} // namespace

const char* statusToString(Status status) {
    switch (status) {
        case Status::Ok: return "ok";
        case Status::NotMounted: return "not mounted";
        case Status::NotFound: return "not found";
        case Status::AlreadyExists: return "already exists";
        case Status::InvalidPath: return "invalid path";
        case Status::IoError: return "I/O error";
        case Status::NoSpace: return "no space";
        case Status::Unsupported: return "unsupported";
        case Status::Unknown: return "unknown";
        default: return "unknown";
    }
}

const char* filesystemStatusToString(FilesystemStatus status) {
    switch (status) {
        case FilesystemStatus::Healthy: return "Ready";
        case FilesystemStatus::NotMounted: return "Disabled/Backend unavailable";
        case FilesystemStatus::Degraded: return "Degraded/Corrupt";
        case FilesystemStatus::Error: return "Degraded/Corrupt";
        case FilesystemStatus::Unformatted: return "Unformatted";
        case FilesystemStatus::Unknown:
        default: return "Unknown";
    }
}

const char* mountFailureReasonToString(MountFailureReason reason) {
    switch (reason) {
        case MountFailureReason::None: return "none";
        case MountFailureReason::BackendUnavailable: return "backend unavailable";
        case MountFailureReason::FlashProbeFailed: return "flash storage probe failed";
        case MountFailureReason::RegionInvalid: return "filesystem region invalid";
        case MountFailureReason::NotFormatted: return "filesystem not formatted";
        case MountFailureReason::MissingMagic: return "filesystem magic missing";
        case MountFailureReason::Corrupt: return "filesystem corrupted";
        case MountFailureReason::MountFailed: return "mount failed";
        default: return "mount failed";
    }
}

bool isOk(Status status) {
    return status == Status::Ok;
}

AxiomFS::Status Backend::unmount() {
    return Status::Unsupported;
}

AxiomFS::Status Backend::sync() {
    return Status::Ok;
}

bool Backend::isMounted() const {
    return false;
}

MountFailureReason Backend::lastMountFailureReason() const {
    return MountFailureReason::None;
}

bool Backend::isFreshBlankFilesystem() const {
    return false;
}

Diagnostics Backend::getDiagnostics() {
    Diagnostics diagnostics;
    diagnostics.backendName = backendName();
    diagnostics.mounted = isMounted();
    diagnostics.mountFailureReason = lastMountFailureReason();
    diagnostics.mountStatus = diagnostics.mounted ? Status::Ok : Status::NotMounted;
    diagnostics.status = diagnostics.mounted
        ? FilesystemStatus::Healthy
        : FilesystemStatus::NotMounted;
    return diagnostics;
}

ProbeResult Backend::runProbe() {
    ProbeResult result;
    result.mountedBeforeProbe = isMounted();
    result.mountStatus = mount();
    result.mountFailureReason = lastMountFailureReason();
    result.mountedAfterProbe = isMounted();
    result.probeStatus = result.mountStatus;
    return result;
}

Status Backend::eraseStorageRegion() {
    return Status::Unsupported;
}

Status Backend::repairOrFormatForDevMode() {
    const Status mountStatus = mount();
    if (mountStatus == Status::Ok) {
        return Status::Ok;
    }

    const Status formatStatus = format();
    if (formatStatus != Status::Ok) {
        return formatStatus;
    }

    return mount();
}

FileSystem::FileSystem(Backend& backend)
    : m_backend(backend) {}

Status FileSystem::mount() {
    return m_backend.mount();
}

Status FileSystem::unmount() {
    return m_backend.unmount();
}

Status FileSystem::sync() {
    return m_backend.sync();
}

Status FileSystem::remount() {
    std::printf("[fs] remount requested backend=%s mounted=%s\n",
                backendName(),
                isMounted() ? "yes" : "no");
    if (isMounted()) {
        const Status unmountStatus = unmount();
        std::printf("[fs] remount unmount result=%s\n", statusToString(unmountStatus));
        if (unmountStatus != Status::Ok) {
            return unmountStatus;
        }
    }

    const Status mountStatus = mount();
    std::printf("[fs] remount mount result=%s reason=%s\n",
                statusToString(mountStatus),
                mountFailureReasonToString(lastMountFailureReason()));
    return mountStatus;
}

Status FileSystem::format() {
    return m_backend.format();
}

Status FileSystem::exists(const std::string& path, bool& outExists) {
    return m_backend.exists(path, outExists);
}

ReadResult FileSystem::readFile(const std::string& path) {
    return m_backend.readFile(path);
}

Status FileSystem::writeFile(const std::string& path, const uint8_t* data, std::size_t size) {
    return m_backend.writeFile(path, data, size);
}

Status FileSystem::writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    return writeFile(path, data.data(), data.size());
}

Status FileSystem::writeFile(const std::string& path, const std::string& data) {
    return writeFile(path, reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

Status FileSystem::deleteFile(const std::string& path) {
    return m_backend.deleteFile(path);
}

Status FileSystem::createDir(const std::string& path) {
    return m_backend.createDir(path);
}

Status FileSystem::renameFile(const std::string& oldPath, const std::string& newPath) {
    return m_backend.renameFile(oldPath, newPath);
}

ListResult FileSystem::listDir(const std::string& path) {
    return m_backend.listDir(path);
}

SpaceResult FileSystem::getFreeSpace() {
    return m_backend.getFreeSpace();
}

SpaceResult FileSystem::getTotalSpace() {
    return m_backend.getTotalSpace();
}

const char* FileSystem::backendName() const {
    return m_backend.backendName();
}

bool FileSystem::isMounted() const {
    return m_backend.isMounted();
}

MountFailureReason FileSystem::lastMountFailureReason() const {
    return m_backend.lastMountFailureReason();
}

bool FileSystem::isFreshBlankFilesystem() const {
    return m_backend.isFreshBlankFilesystem();
}

Diagnostics FileSystem::getDiagnostics() {
    return m_backend.getDiagnostics();
}

ProbeResult FileSystem::runProbe() {
    return m_backend.runProbe();
}

Status FileSystem::eraseStorageRegion() {
    std::printf("[fs] erase storage region requested backend=%s\n", backendName());
    return m_backend.eraseStorageRegion();
}

Status FileSystem::repairOrFormatForDevMode() {
    return m_backend.repairOrFormatForDevMode();
}

Status normalizePath(const std::string& path, std::string& normalized) {
    normalized.clear();

    if (path.empty()) {
        return Status::InvalidPath;
    }

    if (path == "/") {
        return Status::Ok;
    }

    if (path[0] == '/' || path[0] == '\\') {
        return Status::InvalidPath;
    }

    std::vector<std::string> components;
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t slash = path.find('/', start);
        const std::size_t end = slash == std::string::npos ? path.size() : slash;
        const std::string component = path.substr(start, end - start);

        if (component.empty() || component == ".") {
            // Repeated separators and "." do not affect the normalized path.
        } else if (component == "..") {
            return Status::InvalidPath;
        } else if (component.find('\\') != std::string::npos) {
            return Status::InvalidPath;
        } else {
            components.push_back(component);
        }

        if (slash == std::string::npos) {
            break;
        }
        start = slash + 1;
    }

    for (std::size_t i = 0; i < components.size(); ++i) {
        if (i != 0) {
            normalized.push_back('/');
        }
        normalized += components[i];
    }

    return Status::Ok;
}

bool isProtectedSystemPath(const std::string& path) {
    std::string normalized;
    if (normalizePath(path, normalized) != Status::Ok) {
        return false;
    }

    for (const char* directory : kDefaultDirectories) {
        if (normalized == directory) {
            return true;
        }
    }
    return false;
}

const char* releaseLabel() {
    return kReleaseLabel;
}

const char* const* defaultDirectories() {
    return kDefaultDirectories;
}

int defaultDirectoryCount() {
    return static_cast<int>(sizeof(kDefaultDirectories) / sizeof(kDefaultDirectories[0]));
}

Status ensureDefaultLayout(FileSystem& fs, int* createdDirectories) {
    int created = 0;
    for (const char* directory : kDefaultDirectories) {
        bool exists = false;
        Status status = fs.exists(directory, exists);
        if (status != Status::Ok) {
            if (createdDirectories) {
                *createdDirectories = created;
            }
            return status;
        }
        if (!exists) {
            status = fs.createDir(directory);
            if (status != Status::Ok) {
                if (createdDirectories) {
                    *createdDirectories = created;
                }
                return status;
            }
            created++;
        }
    }

    if (createdDirectories) {
        *createdDirectories = created;
    }
    return Status::Ok;
}

HealthResult runHealthCheck(FileSystem& fs) {
    HealthResult result;
    result.mountStatus = fs.mount();
    result.mounted = result.mountStatus == Status::Ok;
    result.mountFailureReason = result.mounted
        ? MountFailureReason::None
        : fs.lastMountFailureReason();
    if (!result.mounted) {
        if (result.mountStatus == Status::Unsupported ||
            result.mountFailureReason == MountFailureReason::BackendUnavailable) {
            result.status = FilesystemStatus::NotMounted;
            setDetail(result, "AxiomFS backend is not compiled or enabled.");
        } else if (result.mountFailureReason == MountFailureReason::NotFormatted) {
            result.status = FilesystemStatus::Unformatted;
            setDetail(result, "Filesystem is not formatted; storage was not formatted automatically.");
        } else if (result.mountFailureReason == MountFailureReason::MissingMagic) {
            result.status = FilesystemStatus::Error;
            setDetail(result, "LittleFS magic is missing on nonblank storage; storage was not formatted automatically.");
        } else if (result.mountFailureReason == MountFailureReason::RegionInvalid ||
                   result.mountFailureReason == MountFailureReason::FlashProbeFailed) {
            result.status = FilesystemStatus::Error;
            setDetail(result, mountFailureReasonToString(result.mountFailureReason));
        } else if (result.mountFailureReason == MountFailureReason::Corrupt) {
            result.status = FilesystemStatus::Error;
            setDetail(result, "Filesystem appears corrupted; storage was not formatted automatically.");
        } else {
            result.status = FilesystemStatus::Error;
            setDetail(result, "Mount failed for another reason; storage was not formatted automatically.");
        }
        g_lastHealthResult = result;
        return result;
    }

    result.layoutStatus = ensureDefaultLayout(fs, &result.createdDirectories);
    result.defaultLayoutReady = result.layoutStatus == Status::Ok;
    if (!result.defaultLayoutReady) {
        result.status = FilesystemStatus::Degraded;
        setDetail(result, "Default folders could not be created.");
        g_lastHealthResult = result;
        return result;
    }

    const std::string probePath = "cache/.healthcheck.tmp";
    const std::string probeData = "mi23-fs-health";
    result.readWriteStatus = fs.writeFile(probePath, probeData);
    if (result.readWriteStatus == Status::Ok) {
        const ReadResult read = fs.readFile(probePath);
        if (!read.ok() || std::string(read.data.begin(), read.data.end()) != probeData) {
            result.readWriteStatus = read.ok() ? Status::IoError : read.status;
        }
    }
    if (result.readWriteStatus == Status::Ok) {
        result.readWriteStatus = fs.deleteFile(probePath);
    }

    result.readWriteReady = result.readWriteStatus == Status::Ok;
    if (!result.readWriteReady) {
        result.status = FilesystemStatus::Degraded;
        setDetail(result, "Temporary read/write/delete test failed.");
    } else {
        result.status = FilesystemStatus::Healthy;
        setDetail(result, result.createdDirectories > 0
            ? "Filesystem repaired and ready."
            : "Filesystem ready.");
    }

    g_lastHealthResult = result;
    return result;
}

HealthResult initialize(FileSystem& fs) {
    return runHealthCheck(fs);
}

HealthResult initializeForBoot(FileSystem& fs, const char* platformLogPrefix) {
    const char* prefix = platformLogPrefix ? platformLogPrefix : "[fs]";
    HealthResult result = runHealthCheck(fs);
    if (result.status != FilesystemStatus::Unformatted ||
        result.mountFailureReason != MountFailureReason::NotFormatted ||
        !fs.isFreshBlankFilesystem()) {
        return result;
    }

    std::printf("%s fresh blank filesystem detected\n", prefix);
    std::printf("%s formatting storage for first boot\n", prefix);

    const Status formatStatus = fs.format();
    if (formatStatus != Status::Ok) {
        result.mountStatus = formatStatus;
        result.status = formatStatus == Status::Unsupported
            ? FilesystemStatus::NotMounted
            : FilesystemStatus::Error;
        result.mountFailureReason = fs.lastMountFailureReason();
        setDetail(result, result.mountFailureReason == MountFailureReason::None
            ? statusToString(formatStatus)
            : mountFailureReasonToString(result.mountFailureReason));
        std::printf("%s format failed: %s reason=%s\n",
                    prefix,
                    statusToString(formatStatus),
                    mountFailureReasonToString(result.mountFailureReason));
        g_lastHealthResult = result;
        return result;
    }

    std::printf("%s format ok\n", prefix);

    result = runHealthCheck(fs);
    if (result.mountStatus == Status::Ok) {
        std::printf("%s mount ok\n", prefix);
    } else {
        std::printf("%s mount failed after format: %s reason=%s\n",
                    prefix,
                    statusToString(result.mountStatus),
                    mountFailureReasonToString(result.mountFailureReason));
        return result;
    }

    if (result.defaultLayoutReady) {
        std::printf("[fs] default directories created:");
        for (const char* directory : kDefaultDirectories) {
            std::printf(" /%s", directory);
        }
        std::printf("\n");
    } else {
        std::printf("[fs] default directories failed: %s\n",
                    statusToString(result.layoutStatus));
        return result;
    }

    if (result.status == FilesystemStatus::Healthy) {
        std::printf("[fs] status=ready\n");
    } else {
        std::printf("[fs] status=%s detail=%s\n",
                    filesystemStatusToString(result.status),
                    result.detail.c_str());
    }

    return result;
}

HealthResult formatAndInitialize(FileSystem& fs) {
    HealthResult result;
    result.mountStatus = fs.format();
    if (result.mountStatus != Status::Ok) {
        result.status = result.mountStatus == Status::Unsupported
            ? FilesystemStatus::NotMounted
            : FilesystemStatus::Error;
        result.mountFailureReason = fs.lastMountFailureReason();
        setDetail(result, result.mountStatus == Status::Unsupported
            ? "AxiomFS backend is not compiled or enabled."
            : "Filesystem format failed; storage was not mounted.");
        g_lastHealthResult = result;
        return result;
    }

    return runHealthCheck(fs);
}

const HealthResult& getLastHealthResult() {
    return g_lastHealthResult;
}

FilesystemStatus getStatus() {
    return g_lastHealthResult.status;
}

StorageStats getStorageStats(FileSystem& fs) {
    StorageStats stats;
    stats.status = getStatus();
    stats.backendType = fs.backendName();

    const SpaceResult total = fs.getTotalSpace();
    const SpaceResult free = fs.getFreeSpace();
    if (!total.ok()) {
        stats.queryStatus = total.status;
        return stats;
    }
    if (!free.ok()) {
        stats.queryStatus = free.status;
        return stats;
    }

    stats.totalBytes = total.bytes;
    stats.freeBytes = free.bytes;
    stats.usedBytes = total.bytes >= free.bytes ? total.bytes - free.bytes : 0;
    stats.queryStatus = Status::Ok;

    int count = 0;
    if (countFiles(fs, "", count) == Status::Ok) {
        stats.fileCount = count;
    }
    return stats;
}

} // namespace AxiomFS
