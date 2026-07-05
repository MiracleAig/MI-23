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

class Backend {
public:
    virtual ~Backend() = default;

    virtual Status mount() = 0;
    virtual Status format() = 0;
    virtual Status exists(const std::string& path, bool& outExists) = 0;
    virtual ReadResult readFile(const std::string& path) = 0;
    virtual Status writeFile(const std::string& path, const uint8_t* data, std::size_t size) = 0;
    virtual Status deleteFile(const std::string& path) = 0;
    virtual Status createDir(const std::string& path) = 0;
    virtual Status renameFile(const std::string& oldPath, const std::string& newPath) = 0;
    virtual ListResult listDir(const std::string& path) = 0;
    virtual SpaceResult getFreeSpace() = 0;
    virtual SpaceResult getTotalSpace() = 0;
    virtual const char* backendName() const = 0;
    virtual MountFailureReason lastMountFailureReason() const;

    virtual Status repairOrFormatForDevMode();
};

class FileSystem {
public:
    explicit FileSystem(Backend& backend);

    Status mount();
    Status format();
    Status exists(const std::string& path, bool& outExists);
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
    MountFailureReason lastMountFailureReason() const;

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
HealthResult formatAndInitialize(FileSystem& fs);
Status ensureDefaultLayout(FileSystem& fs, int* createdDirectories = nullptr);
HealthResult runHealthCheck(FileSystem& fs);
const HealthResult& getLastHealthResult();
FilesystemStatus getStatus();
StorageStats getStorageStats(FileSystem& fs);

} // namespace AxiomFS
