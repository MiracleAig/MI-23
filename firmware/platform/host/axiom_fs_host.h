#pragma once

#include "hal/fs/axiom_fs.h"

#include <filesystem>
#include <string>

class HostAxiomFSBackend : public AxiomFS::Backend {
public:
    HostAxiomFSBackend();
    explicit HostAxiomFSBackend(std::filesystem::path root);

    AxiomFS::Status mount() override;
    AxiomFS::Status unmount() override;
    AxiomFS::Status sync() override;
    AxiomFS::Status format() override;
    AxiomFS::Status exists(const std::string& path, bool& outExists) override;
    AxiomFS::MetadataResult metadata(const std::string& path) override;
    AxiomFS::RangeReadResult readRange(const std::string& path, uint64_t offset, std::size_t length) override;
    AxiomFS::Status writeRange(const std::string& path, uint64_t offset, const uint8_t* data,
                               std::size_t size, bool truncate) override;
    AxiomFS::ReadResult readFile(const std::string& path) override;
    AxiomFS::Status writeFile(const std::string& path, const uint8_t* data, std::size_t size) override;
    AxiomFS::Status deleteFile(const std::string& path) override;
    AxiomFS::Status createDir(const std::string& path) override;
    AxiomFS::Status renameFile(const std::string& oldPath, const std::string& newPath) override;
    AxiomFS::ListResult listDir(const std::string& path) override;
    AxiomFS::SpaceResult getFreeSpace() override;
    AxiomFS::SpaceResult getTotalSpace() override;
    const char* backendName() const override;
    bool isMounted() const override;
    AxiomFS::Diagnostics getDiagnostics() override;
    AxiomFS::ProbeResult runProbe() override;

    const std::filesystem::path& root() const;

private:
    std::filesystem::path m_root;
    bool m_mounted;

    AxiomFS::Status resolvePath(const std::string& path,
                                std::filesystem::path& resolved,
                                bool allowRoot) const;
    AxiomFS::Status ensureParentDirectories(const std::filesystem::path& path) const;
};
