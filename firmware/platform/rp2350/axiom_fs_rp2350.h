#pragma once

#include "hal/fs/axiom_fs.h"

#if MI23_ENABLE_LITTLEFS
#include "lfs.h"
#endif

class RP2350AxiomFSBackend : public AxiomFS::Backend {
public:
    RP2350AxiomFSBackend();

    AxiomFS::Status mount() override;
    AxiomFS::Status format() override;
    AxiomFS::Status exists(const std::string& path, bool& outExists) override;
    AxiomFS::ReadResult readFile(const std::string& path) override;
    AxiomFS::Status writeFile(const std::string& path, const uint8_t* data, std::size_t size) override;
    AxiomFS::Status deleteFile(const std::string& path) override;
    AxiomFS::Status createDir(const std::string& path) override;
    AxiomFS::Status renameFile(const std::string& oldPath, const std::string& newPath) override;
    AxiomFS::ListResult listDir(const std::string& path) override;
    AxiomFS::SpaceResult getFreeSpace() override;
    AxiomFS::SpaceResult getTotalSpace() override;
    const char* backendName() const override;
    AxiomFS::MountFailureReason lastMountFailureReason() const override;

private:
    bool m_mounted;
    AxiomFS::MountFailureReason m_lastMountFailureReason;
    int m_lastLittleFsError;

#if MI23_ENABLE_LITTLEFS
    lfs_t m_lfs;
    lfs_config m_config;

    AxiomFS::Status normalizeMountedPath(const std::string& path,
                                         std::string& normalized,
                                         bool allowRoot) const;
#endif
};
