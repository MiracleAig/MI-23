#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace AxiomFS {

enum class Status {
    Ok,
    NotMounted,
    NotFound,
    AlreadyExists,
    InvalidPath,
    IoError,
    NoSpace,
    Unsupported,
    Unknown,
};

enum class FilesystemStatus {
    Healthy,
    NotMounted,
    Degraded,
    Error,
    Unformatted,
    NeedsFormat = Unformatted,
    Unknown,
};

enum class MountFailureReason {
    None,
    BackendUnavailable,
    FlashProbeFailed,
    RegionInvalid,
    NotFormatted,
    MissingMagic,
    Corrupt,
    MountFailed,
};

const char* statusToString(Status status);
bool isOk(Status status);
const char* filesystemStatusToString(FilesystemStatus status);
const char* mountFailureReasonToString(MountFailureReason reason);

struct DirectoryEntry {
    std::string name;
    bool isDirectory = false;
    uint64_t size = 0;
};

struct ReadResult {
    Status status = Status::IoError;
    std::vector<uint8_t> data;

    bool ok() const { return status == Status::Ok; }
};

struct MetadataResult {
    Status status = Status::IoError;
    bool isDirectory = false;
    uint64_t size = 0;
    bool ok() const { return status == Status::Ok; }
};

struct RangeReadResult {
    Status status = Status::IoError;
    std::vector<uint8_t> data;
    uint64_t totalSize = 0;
    bool eof = false;
    bool ok() const { return status == Status::Ok; }
};

struct ListResult {
    Status status = Status::IoError;
    std::vector<DirectoryEntry> entries;

    bool ok() const { return status == Status::Ok; }
};

struct SpaceResult {
    Status status = Status::Unsupported;
    uint64_t bytes = 0;

    bool ok() const { return status == Status::Ok; }
};

struct ProbeResult {
    Status probeStatus = Status::Unsupported;
    Status mountStatus = Status::Unsupported;
    MountFailureReason mountFailureReason = MountFailureReason::None;
    bool erasedKnown = false;
    bool erased = false;
    bool magicKnown = false;
    bool hasMagic = false;
    bool mountedBeforeProbe = false;
    bool mountedAfterProbe = false;
};

struct Diagnostics {
    const char* backendName = "Unknown";
    FilesystemStatus status = FilesystemStatus::Unknown;
    Status mountStatus = Status::Unknown;
    MountFailureReason mountFailureReason = MountFailureReason::None;
    bool mounted = false;
    bool geometryKnown = false;
    uint32_t flashSize = 0;
    uint32_t fsOffset = 0;
    uint32_t fsSize = 0;
    uint32_t blockSize = 0;
    bool spaceKnown = false;
    uint64_t totalBytes = 0;
    uint64_t usedBytes = 0;
    uint64_t freeBytes = 0;
};

class Backend {
public:
    virtual ~Backend() = default;

    virtual Status mount() = 0;
    virtual Status unmount();
    virtual Status sync();
    virtual Status format() = 0;
    virtual Status exists(const std::string& path, bool& outExists) = 0;
    virtual MetadataResult metadata(const std::string& path);
    virtual RangeReadResult readRange(const std::string& path, uint64_t offset, std::size_t length);
    virtual Status writeRange(const std::string& path, uint64_t offset, const uint8_t* data,
                              std::size_t size, bool truncate);
    virtual ReadResult readFile(const std::string& path) = 0;
    virtual Status writeFile(const std::string& path, const uint8_t* data, std::size_t size) = 0;
    virtual Status deleteFile(const std::string& path) = 0;
    virtual Status createDir(const std::string& path) = 0;
    virtual Status renameFile(const std::string& oldPath, const std::string& newPath) = 0;
    virtual ListResult listDir(const std::string& path) = 0;
    virtual SpaceResult getFreeSpace() = 0;
    virtual SpaceResult getTotalSpace() = 0;
    virtual const char* backendName() const = 0;
    virtual bool isMounted() const;
    virtual MountFailureReason lastMountFailureReason() const;
    virtual bool isFreshBlankFilesystem() const;
    virtual Diagnostics getDiagnostics();
    virtual ProbeResult runProbe();
    virtual Status eraseStorageRegion();

    virtual Status repairOrFormatForDevMode();
};

class FileSystem {
public:
    explicit FileSystem(Backend& backend);

    Status mount();
    Status unmount();
    Status sync();
    Status remount();
    Status format();
    Status exists(const std::string& path, bool& outExists);
    MetadataResult metadata(const std::string& path);
    RangeReadResult readRange(const std::string& path, uint64_t offset, std::size_t length);
    Status writeRange(const std::string& path, uint64_t offset, const uint8_t* data,
                      std::size_t size, bool truncate);
    ReadResult readFile(const std::string& path);
    Status writeFile(const std::string& path, const uint8_t* data, std::size_t size);
    Status writeFile(const std::string& path, const std::vector<uint8_t>& data);
    Status writeFile(const std::string& path, const std::string& data);
    Status deleteFile(const std::string& path);
    Status createDir(const std::string& path);
    Status renameFile(const std::string& oldPath, const std::string& newPath);
    ListResult listDir(const std::string& path);
    SpaceResult getFreeSpace();
    SpaceResult getTotalSpace();
    const char* backendName() const;
    bool isMounted() const;
    MountFailureReason lastMountFailureReason() const;
    bool isFreshBlankFilesystem() const;
    Diagnostics getDiagnostics();
    ProbeResult runProbe();
    Status eraseStorageRegion();

    // Development-only recovery hook. Boot code should call mount() and report
    // failures instead of formatting automatically.
    Status repairOrFormatForDevMode();

private:
    Backend& m_backend;
};

Status normalizePath(const std::string& path, std::string& normalized);
bool isProtectedSystemPath(const std::string& path);

struct HealthResult {
    FilesystemStatus status = FilesystemStatus::Unknown;
    Status mountStatus = Status::NotMounted;
    Status layoutStatus = Status::Unknown;
    Status readWriteStatus = Status::Unknown;
    MountFailureReason mountFailureReason = MountFailureReason::None;
    int createdDirectories = 0;
    bool mounted = false;
    bool defaultLayoutReady = false;
    bool readWriteReady = false;
    std::string detail;
};

struct StorageStats {
    FilesystemStatus status = FilesystemStatus::Unknown;
    Status queryStatus = Status::Unknown;
    uint64_t totalBytes = 0;
    uint64_t freeBytes = 0;
    uint64_t usedBytes = 0;
    int fileCount = -1;
    const char* backendType = "Unknown";
};

const char* releaseLabel();
const char* const* defaultDirectories();
int defaultDirectoryCount();

HealthResult initialize(FileSystem& fs);
HealthResult initializeForBoot(FileSystem& fs, const char* platformLogPrefix = "[fs]");
HealthResult formatAndInitialize(FileSystem& fs);
Status ensureDefaultLayout(FileSystem& fs, int* createdDirectories = nullptr);
HealthResult runHealthCheck(FileSystem& fs);
const HealthResult& getLastHealthResult();
FilesystemStatus getStatus();
StorageStats getStorageStats(FileSystem& fs);

} // namespace AxiomFS
