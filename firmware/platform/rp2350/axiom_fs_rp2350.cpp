#include "platform/rp2350/axiom_fs_rp2350.h"

#include "platform/rp2350/axiom_fs_flash_block_device.h"
#include "platform/rp2350/axiom_fs_flash_config.h"

#include <cstdio>
#include <cstring>

#if MI23_ENABLE_LITTLEFS
namespace {

int lfsRead(const lfs_config*, lfs_block_t block, lfs_off_t off, void* buffer, lfs_size_t size) {
    return RP2350FlashBlockDevice::read(block, off, buffer, size);
}

int lfsProgram(const lfs_config*, lfs_block_t block, lfs_off_t off, const void* buffer, lfs_size_t size) {
    return RP2350FlashBlockDevice::program(block, off, buffer, size);
}

int lfsErase(const lfs_config*, lfs_block_t block) {
    return RP2350FlashBlockDevice::erase(block);
}

int lfsSync(const lfs_config*) {
    return RP2350FlashBlockDevice::sync();
}

AxiomFS::Status mapLittleFsError(int error) {
    switch (error) {
        case 0: return AxiomFS::Status::Ok;
        case LFS_ERR_NOENT: return AxiomFS::Status::NotFound;
        case LFS_ERR_EXIST: return AxiomFS::Status::AlreadyExists;
        case LFS_ERR_NOSPC: return AxiomFS::Status::NoSpace;
        case LFS_ERR_INVAL: return AxiomFS::Status::InvalidPath;
        default: return AxiomFS::Status::IoError;
    }
}

const char* littleFsErrorName(int error) {
    switch (error) {
        case 0: return "LFS_ERR_OK";
        case LFS_ERR_IO: return "LFS_ERR_IO";
        case LFS_ERR_CORRUPT: return "LFS_ERR_CORRUPT";
        case LFS_ERR_NOENT: return "LFS_ERR_NOENT";
        case LFS_ERR_EXIST: return "LFS_ERR_EXIST";
        case LFS_ERR_INVAL: return "LFS_ERR_INVAL";
        case LFS_ERR_NOSPC: return "LFS_ERR_NOSPC";
        default: return "LFS_ERR_UNKNOWN";
    }
}

uint8_t s_readBuffer[RP2350FlashLayout::kLittleFsProgramSize];
uint8_t s_programBuffer[RP2350FlashLayout::kLittleFsProgramSize];
uint8_t s_lookaheadBuffer[16];

} // namespace
#endif

RP2350AxiomFSBackend::RP2350AxiomFSBackend()
    : m_mounted(false)
    , m_lastMountFailureReason(AxiomFS::MountFailureReason::None)
    , m_lastLittleFsError(0)
#if MI23_ENABLE_LITTLEFS
    , m_lfs{}
    , m_config{}
#endif
{
#if MI23_ENABLE_LITTLEFS
    m_config.read = lfsRead;
    m_config.prog = lfsProgram;
    m_config.erase = lfsErase;
    m_config.sync = lfsSync;

    // LittleFS writes in flash pages and erases in flash sectors. The cache size
    // matches the RP2350 flash program page, while the block size matches the
    // erase sector. This keeps all LittleFS writes aligned for flash_range_*.
    m_config.read_size = 16;
    m_config.prog_size = RP2350FlashBlockDevice::programSize();
    m_config.block_size = RP2350FlashBlockDevice::blockSize();
    m_config.block_count = RP2350FlashBlockDevice::blockCount();
    m_config.cache_size = RP2350FlashBlockDevice::programSize();
    m_config.lookahead_size = sizeof(s_lookaheadBuffer);
    m_config.block_cycles = 500;
    m_config.read_buffer = s_readBuffer;
    m_config.prog_buffer = s_programBuffer;
    m_config.lookahead_buffer = s_lookaheadBuffer;
#endif
}

AxiomFS::Status RP2350AxiomFSBackend::mount() {
    m_lastMountFailureReason = AxiomFS::MountFailureReason::None;
    m_lastLittleFsError = 0;

    std::printf("[fs][rp2350] backend selected: %s\n", backendName());
    std::printf("[fs][rp2350] littlefs compiled=%s\n",
#if MI23_ENABLE_LITTLEFS
                "yes"
#else
                "no"
#endif
    );
    std::printf("[fs][rp2350] flash configured=%lu detected=%lu fs_offset=%lu fs_size=%lu block=%lu erase=%lu program=%lu blocks=%lu\n",
                static_cast<unsigned long>(PICO_FLASH_SIZE_BYTES),
                static_cast<unsigned long>(RP2350FlashBlockDevice::detectedFlashSize()),
                static_cast<unsigned long>(RP2350FlashBlockDevice::baseOffset()),
                static_cast<unsigned long>(RP2350FlashBlockDevice::totalSize()),
                static_cast<unsigned long>(RP2350FlashBlockDevice::blockSize()),
                static_cast<unsigned long>(FLASH_SECTOR_SIZE),
                static_cast<unsigned long>(RP2350FlashBlockDevice::programSize()),
                static_cast<unsigned long>(RP2350FlashBlockDevice::blockCount()));

    const RP2350FlashBlockDevice::LayoutError layoutError =
        RP2350FlashBlockDevice::validateLayout();
    if (layoutError != RP2350FlashBlockDevice::LayoutError::None) {
        m_lastMountFailureReason = AxiomFS::MountFailureReason::RegionInvalid;
        std::printf("[fs][rp2350] filesystem region invalid: %s\n",
                    RP2350FlashBlockDevice::layoutErrorToString(layoutError));
        return AxiomFS::Status::IoError;
    }

    if (!RP2350FlashBlockDevice::probe()) {
        m_lastMountFailureReason = AxiomFS::MountFailureReason::FlashProbeFailed;
        std::printf("[fs][rp2350] flash storage probe failed\n");
        return AxiomFS::Status::IoError;
    }

#if MI23_ENABLE_LITTLEFS
    if (!m_config.read || !m_config.prog || !m_config.erase || !m_config.sync ||
        m_config.block_size == 0 || m_config.block_count == 0 ||
        m_config.prog_size == 0 || m_config.cache_size == 0) {
        m_lastMountFailureReason = AxiomFS::MountFailureReason::BackendUnavailable;
        std::printf("[fs][rp2350] LittleFS backend config/vtable is incomplete\n");
        return AxiomFS::Status::Unsupported;
    }

    if (m_mounted) {
        std::printf("[fs][rp2350] LittleFS already mounted\n");
        return AxiomFS::Status::Ok;
    }

    const bool erased = RP2350FlashBlockDevice::isRegionErased();
    const bool hasMagic = !erased && RP2350FlashBlockDevice::hasLittleFsMagic();
    std::printf("[fs][rp2350] filesystem probe erased=%s magic=%s\n",
                erased ? "yes" : "no",
                hasMagic ? "yes" : "no");
    if (erased) {
        m_lastMountFailureReason = AxiomFS::MountFailureReason::NotFormatted;
        std::printf("[fs][rp2350] filesystem classification=unformatted reason=blank erased flash region\n");
        return AxiomFS::Status::NotFound;
    }
    if (!hasMagic) {
        m_lastMountFailureReason = AxiomFS::MountFailureReason::MissingMagic;
        std::printf("[fs][rp2350] filesystem classification=corrupt-or-foreign reason=LittleFS magic missing on nonblank region\n");
        return AxiomFS::Status::NotFound;
    }

    const int result = lfs_mount(&m_lfs, &m_config);
    if (result != 0) {
        m_mounted = false;
        m_lastLittleFsError = result;
        m_lastMountFailureReason = result == LFS_ERR_CORRUPT
            ? AxiomFS::MountFailureReason::Corrupt
            : AxiomFS::MountFailureReason::MountFailed;
        std::printf("[fs][rp2350] LittleFS mount result=%d error=%s status=%s reason=%s\n",
                    result,
                    littleFsErrorName(result),
                    AxiomFS::statusToString(mapLittleFsError(result)),
                    AxiomFS::mountFailureReasonToString(m_lastMountFailureReason));
        return mapLittleFsError(result);
    }

    m_mounted = true;
    m_lastMountFailureReason = AxiomFS::MountFailureReason::None;
    std::printf("[fs][rp2350] LittleFS mount result=0 status=%s\n",
                AxiomFS::statusToString(AxiomFS::Status::Ok));
    return AxiomFS::Status::Ok;
#else
    m_lastMountFailureReason = AxiomFS::MountFailureReason::BackendUnavailable;
    std::printf("[fs][rp2350] LittleFS support is not enabled in this build\n");
    return AxiomFS::Status::Unsupported;
#endif
}

AxiomFS::Status RP2350AxiomFSBackend::format() {
    m_lastMountFailureReason = AxiomFS::MountFailureReason::None;
    m_lastLittleFsError = 0;

#if MI23_ENABLE_LITTLEFS
    std::printf("[fs][rp2350] LittleFS format requested offset=%lu size=%lu blocks=%lu\n",
                static_cast<unsigned long>(RP2350FlashBlockDevice::baseOffset()),
                static_cast<unsigned long>(RP2350FlashBlockDevice::totalSize()),
                static_cast<unsigned long>(RP2350FlashBlockDevice::blockCount()));

    const RP2350FlashBlockDevice::LayoutError layoutError =
        RP2350FlashBlockDevice::validateLayout();
    if (layoutError != RP2350FlashBlockDevice::LayoutError::None) {
        m_lastMountFailureReason = AxiomFS::MountFailureReason::RegionInvalid;
        std::printf("[fs][rp2350] LittleFS format blocked: filesystem region invalid: %s\n",
                    RP2350FlashBlockDevice::layoutErrorToString(layoutError));
        return AxiomFS::Status::IoError;
    }

    if (!RP2350FlashBlockDevice::probe()) {
        m_lastMountFailureReason = AxiomFS::MountFailureReason::FlashProbeFailed;
        std::printf("[fs][rp2350] LittleFS format blocked: flash storage probe failed\n");
        return AxiomFS::Status::IoError;
    }

    if (!m_config.read || !m_config.prog || !m_config.erase || !m_config.sync ||
        m_config.block_size == 0 || m_config.block_count == 0 ||
        m_config.prog_size == 0 || m_config.cache_size == 0) {
        m_lastMountFailureReason = AxiomFS::MountFailureReason::BackendUnavailable;
        std::printf("[fs][rp2350] LittleFS format blocked: backend config/vtable is incomplete\n");
        return AxiomFS::Status::Unsupported;
    }

    if (m_mounted) {
        lfs_unmount(&m_lfs);
        m_mounted = false;
    }

    const bool erased = RP2350FlashBlockDevice::isRegionErased();
    const bool hasMagic = !erased && RP2350FlashBlockDevice::hasLittleFsMagic();
    std::printf("[fs][rp2350] format preflight erased=%s magic=%s\n",
                erased ? "yes" : "no",
                hasMagic ? "yes" : "no");

    const int result = lfs_format(&m_lfs, &m_config);
    if (result != 0) {
        m_lastLittleFsError = result;
        m_lastMountFailureReason = AxiomFS::MountFailureReason::MountFailed;
        std::printf("[fs][rp2350] LittleFS format result=%d error=%s status=%s\n",
                    result,
                    littleFsErrorName(result),
                    AxiomFS::statusToString(mapLittleFsError(result)));
        return mapLittleFsError(result);
    }

    std::printf("[fs][rp2350] LittleFS format result=0 status=%s\n",
                AxiomFS::statusToString(AxiomFS::Status::Ok));
    return AxiomFS::Status::Ok;
#else
    m_lastMountFailureReason = AxiomFS::MountFailureReason::BackendUnavailable;
    std::printf("[fs][rp2350] LittleFS format unavailable: support is not enabled in this build\n");
    return AxiomFS::Status::Unsupported;
#endif
}

AxiomFS::Status RP2350AxiomFSBackend::exists(const std::string& path, bool& outExists) {
    outExists = false;
    if (!m_mounted) {
        return AxiomFS::Status::NotMounted;
    }

#if MI23_ENABLE_LITTLEFS
    std::string normalized;
    const AxiomFS::Status pathStatus = normalizeMountedPath(path, normalized, true);
    if (pathStatus != AxiomFS::Status::Ok) {
        return pathStatus;
    }

    lfs_info info{};
    const int result = lfs_stat(&m_lfs, normalized.empty() ? "/" : normalized.c_str(), &info);
    if (result == LFS_ERR_NOENT) {
        return AxiomFS::Status::Ok;
    }
    outExists = result == 0;
    return mapLittleFsError(result);
#else
    return AxiomFS::Status::Unsupported;
#endif
}

AxiomFS::ReadResult RP2350AxiomFSBackend::readFile(const std::string& path) {
    AxiomFS::ReadResult result;
    if (!m_mounted) {
        result.status = AxiomFS::Status::NotMounted;
        return result;
    }

#if MI23_ENABLE_LITTLEFS
    std::string normalized;
    result.status = normalizeMountedPath(path, normalized, false);
    if (result.status != AxiomFS::Status::Ok) {
        return result;
    }

    lfs_file_t file{};
    int lfsResult = lfs_file_open(&m_lfs, &file, normalized.c_str(), LFS_O_RDONLY);
    if (lfsResult != 0) {
        result.status = mapLittleFsError(lfsResult);
        return result;
    }

    const lfs_soff_t size = lfs_file_size(&m_lfs, &file);
    if (size < 0) {
        lfs_file_close(&m_lfs, &file);
        result.status = mapLittleFsError(static_cast<int>(size));
        return result;
    }

    result.data.resize(static_cast<std::size_t>(size));
    if (!result.data.empty()) {
        lfsResult = lfs_file_read(&m_lfs, &file, result.data.data(), result.data.size());
    }
    const int closeResult = lfs_file_close(&m_lfs, &file);
    if (lfsResult < 0) {
        result.status = mapLittleFsError(lfsResult);
    } else if (closeResult != 0) {
        result.status = mapLittleFsError(closeResult);
    } else {
        result.status = AxiomFS::Status::Ok;
    }
    return result;
#else
    result.status = AxiomFS::Status::Unsupported;
    return result;
#endif
}

AxiomFS::Status RP2350AxiomFSBackend::writeFile(const std::string& path,
                                                const uint8_t* data,
                                                std::size_t size) {
    if (!m_mounted) {
        return AxiomFS::Status::NotMounted;
    }
    if (!data && size != 0) {
        return AxiomFS::Status::IoError;
    }

#if MI23_ENABLE_LITTLEFS
    std::string normalized;
    AxiomFS::Status status = normalizeMountedPath(path, normalized, false);
    if (status != AxiomFS::Status::Ok) {
        return status;
    }

    lfs_file_t file{};
    int lfsResult = lfs_file_open(&m_lfs,
                                  &file,
                                  normalized.c_str(),
                                  LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (lfsResult != 0) {
        return mapLittleFsError(lfsResult);
    }

    if (size != 0) {
        lfsResult = lfs_file_write(&m_lfs, &file, data, size);
    }
    const int closeResult = lfs_file_close(&m_lfs, &file);
    if (lfsResult < 0) {
        return mapLittleFsError(lfsResult);
    }
    if (closeResult != 0) {
        return mapLittleFsError(closeResult);
    }
    return AxiomFS::Status::Ok;
#else
    return AxiomFS::Status::Unsupported;
#endif
}

AxiomFS::Status RP2350AxiomFSBackend::deleteFile(const std::string& path) {
    if (!m_mounted) {
        return AxiomFS::Status::NotMounted;
    }

#if MI23_ENABLE_LITTLEFS
    std::string normalized;
    const AxiomFS::Status status = normalizeMountedPath(path, normalized, false);
    if (status != AxiomFS::Status::Ok) {
        return status;
    }
    return mapLittleFsError(lfs_remove(&m_lfs, normalized.c_str()));
#else
    return AxiomFS::Status::Unsupported;
#endif
}

AxiomFS::Status RP2350AxiomFSBackend::createDir(const std::string& path) {
    if (!m_mounted) {
        return AxiomFS::Status::NotMounted;
    }

#if MI23_ENABLE_LITTLEFS
    std::string normalized;
    const AxiomFS::Status status = normalizeMountedPath(path, normalized, false);
    if (status != AxiomFS::Status::Ok) {
        return status;
    }

    const int result = lfs_mkdir(&m_lfs, normalized.c_str());
    if (result == LFS_ERR_EXIST) {
        return AxiomFS::Status::Ok;
    }
    return mapLittleFsError(result);
#else
    return AxiomFS::Status::Unsupported;
#endif
}

AxiomFS::Status RP2350AxiomFSBackend::renameFile(const std::string& oldPath,
                                                 const std::string& newPath) {
    if (!m_mounted) {
        return AxiomFS::Status::NotMounted;
    }

#if MI23_ENABLE_LITTLEFS
    std::string oldNormalized;
    AxiomFS::Status status = normalizeMountedPath(oldPath, oldNormalized, false);
    if (status != AxiomFS::Status::Ok) {
        return status;
    }
    std::string newNormalized;
    status = normalizeMountedPath(newPath, newNormalized, false);
    if (status != AxiomFS::Status::Ok) {
        return status;
    }
    return mapLittleFsError(lfs_rename(&m_lfs, oldNormalized.c_str(), newNormalized.c_str()));
#else
    return AxiomFS::Status::Unsupported;
#endif
}

AxiomFS::ListResult RP2350AxiomFSBackend::listDir(const std::string& path) {
    AxiomFS::ListResult result;
    if (!m_mounted) {
        result.status = AxiomFS::Status::NotMounted;
        return result;
    }

#if MI23_ENABLE_LITTLEFS
    std::string normalized;
    result.status = normalizeMountedPath(path, normalized, true);
    if (result.status != AxiomFS::Status::Ok) {
        return result;
    }

    lfs_dir_t dir{};
    int lfsResult = lfs_dir_open(&m_lfs, &dir, normalized.empty() ? "/" : normalized.c_str());
    if (lfsResult != 0) {
        result.status = mapLittleFsError(lfsResult);
        return result;
    }

    while (true) {
        lfs_info info{};
        lfsResult = lfs_dir_read(&m_lfs, &dir, &info);
        if (lfsResult < 0) {
            result.status = mapLittleFsError(lfsResult);
            lfs_dir_close(&m_lfs, &dir);
            return result;
        }
        if (lfsResult == 0) {
            break;
        }
        if (std::strcmp(info.name, ".") == 0 || std::strcmp(info.name, "..") == 0) {
            continue;
        }

        AxiomFS::DirectoryEntry entry;
        entry.name = info.name;
        entry.isDirectory = info.type == LFS_TYPE_DIR;
        entry.size = info.size;
        result.entries.push_back(entry);
    }

    const int closeResult = lfs_dir_close(&m_lfs, &dir);
    result.status = mapLittleFsError(closeResult);
    return result;
#else
    result.status = AxiomFS::Status::Unsupported;
    return result;
#endif
}

AxiomFS::SpaceResult RP2350AxiomFSBackend::getFreeSpace() {
    AxiomFS::SpaceResult result;
    if (!m_mounted) {
        result.status = AxiomFS::Status::NotMounted;
        return result;
    }

#if MI23_ENABLE_LITTLEFS
    const lfs_ssize_t used = lfs_fs_size(&m_lfs);
    if (used < 0) {
        result.status = mapLittleFsError(static_cast<int>(used));
        return result;
    }

    result.status = AxiomFS::Status::Ok;
    result.bytes = (RP2350FlashBlockDevice::blockCount() - static_cast<uint32_t>(used))
        * RP2350FlashBlockDevice::blockSize();
    return result;
#else
    result.status = AxiomFS::Status::Unsupported;
    return result;
#endif
}

AxiomFS::SpaceResult RP2350AxiomFSBackend::getTotalSpace() {
    AxiomFS::SpaceResult result;
    if (!m_mounted) {
        result.status = AxiomFS::Status::NotMounted;
        return result;
    }

    result.status = AxiomFS::Status::Ok;
    result.bytes = RP2350FlashBlockDevice::totalSize();
    return result;
}

const char* RP2350AxiomFSBackend::backendName() const {
#if MI23_ENABLE_LITTLEFS
    return "RP2350 LittleFS";
#else
    return "RP2350 LittleFS stub";
#endif
}

AxiomFS::MountFailureReason RP2350AxiomFSBackend::lastMountFailureReason() const {
    return m_lastMountFailureReason;
}

#if MI23_ENABLE_LITTLEFS
AxiomFS::Status RP2350AxiomFSBackend::normalizeMountedPath(const std::string& path,
                                                           std::string& normalized,
                                                           bool allowRoot) const {
    const AxiomFS::Status status = AxiomFS::normalizePath(path, normalized);
    if (status != AxiomFS::Status::Ok) {
        return status;
    }
    if (normalized.empty() && !allowRoot) {
        return AxiomFS::Status::InvalidPath;
    }
    return AxiomFS::Status::Ok;
}
#endif
