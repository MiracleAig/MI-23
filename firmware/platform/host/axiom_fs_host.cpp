#include "platform/host/axiom_fs_host.h"

#include <algorithm>
#include <cstdio>
#include <fstream>

namespace {

std::filesystem::path defaultRoot() {
    return std::filesystem::current_path() / "mi23_fs";
}

AxiomFS::Status mapFilesystemError(const std::error_code& error) {
    if (!error) {
        return AxiomFS::Status::Ok;
    }
    if (error == std::errc::no_space_on_device) {
        return AxiomFS::Status::NoSpace;
    }
    return AxiomFS::Status::IoError;
}

bool isSymlink(const std::filesystem::path& path, std::error_code& error) {
    return std::filesystem::is_symlink(std::filesystem::symlink_status(path, error));
}

} // namespace

HostAxiomFSBackend::HostAxiomFSBackend()
    : HostAxiomFSBackend(defaultRoot()) {}

HostAxiomFSBackend::HostAxiomFSBackend(std::filesystem::path root)
    : m_root(std::move(root))
    , m_mounted(false) {}

AxiomFS::Status HostAxiomFSBackend::mount() {
    std::error_code error;
    std::filesystem::create_directories(m_root, error);
    if (error) {
        return mapFilesystemError(error);
    }

    if (!std::filesystem::is_directory(m_root, error) || error) {
        return AxiomFS::Status::IoError;
    }

    m_root = std::filesystem::weakly_canonical(m_root, error);
    if (error) {
        return mapFilesystemError(error);
    }

    m_mounted = true;
    std::printf("[fs][host] mounted AxiomFS at %s\n", m_root.string().c_str());
    return AxiomFS::Status::Ok;
}

AxiomFS::Status HostAxiomFSBackend::unmount() {
    std::printf("[fs][host] unmounted AxiomFS at %s\n", m_root.string().c_str());
    m_mounted = false;
    return AxiomFS::Status::Ok;
}

AxiomFS::Status HostAxiomFSBackend::sync() {
    return AxiomFS::Status::Ok;
}

AxiomFS::Status HostAxiomFSBackend::format() {
    if (m_root.empty()) {
        return AxiomFS::Status::InvalidPath;
    }

    std::error_code error;
    std::filesystem::remove_all(m_root, error);
    if (error) {
        return mapFilesystemError(error);
    }

    m_mounted = false;
    return mount();
}

AxiomFS::Status HostAxiomFSBackend::exists(const std::string& path, bool& outExists) {
    outExists = false;
    if (!m_mounted) {
        return AxiomFS::Status::NotMounted;
    }

    std::filesystem::path resolved;
    const AxiomFS::Status status = resolvePath(path, resolved, true);
    if (status != AxiomFS::Status::Ok) {
        return status;
    }

    std::error_code error;
    outExists = std::filesystem::exists(resolved, error);
    if (error) {
        return mapFilesystemError(error);
    }

    return AxiomFS::Status::Ok;
}

AxiomFS::ReadResult HostAxiomFSBackend::readFile(const std::string& path) {
    AxiomFS::ReadResult result;
    if (!m_mounted) {
        result.status = AxiomFS::Status::NotMounted;
        return result;
    }

    std::filesystem::path resolved;
    result.status = resolvePath(path, resolved, false);
    if (result.status != AxiomFS::Status::Ok) {
        return result;
    }

    std::error_code error;
    if (isSymlink(resolved, error) || error) {
        result.status = error ? mapFilesystemError(error) : AxiomFS::Status::InvalidPath;
        return result;
    }
    if (!std::filesystem::exists(resolved, error)) {
        result.status = error ? mapFilesystemError(error) : AxiomFS::Status::NotFound;
        return result;
    }
    if (!std::filesystem::is_regular_file(resolved, error) || error) {
        result.status = error ? mapFilesystemError(error) : AxiomFS::Status::InvalidPath;
        return result;
    }

    const auto size = std::filesystem::file_size(resolved, error);
    if (error) {
        result.status = mapFilesystemError(error);
        return result;
    }

    std::ifstream file(resolved, std::ios::binary);
    if (!file) {
        result.status = AxiomFS::Status::IoError;
        return result;
    }

    result.data.resize(static_cast<std::size_t>(size));
    if (!result.data.empty()) {
        file.read(reinterpret_cast<char*>(result.data.data()), static_cast<std::streamsize>(result.data.size()));
    }

    result.status = file.good() || file.eof() ? AxiomFS::Status::Ok : AxiomFS::Status::IoError;
    return result;
}

AxiomFS::Status HostAxiomFSBackend::writeFile(const std::string& path,
                                              const uint8_t* data,
                                              std::size_t size) {
    if (!m_mounted) {
        return AxiomFS::Status::NotMounted;
    }
    if (!data && size != 0) {
        return AxiomFS::Status::IoError;
    }

    std::filesystem::path resolved;
    const AxiomFS::Status pathStatus = resolvePath(path, resolved, false);
    if (pathStatus != AxiomFS::Status::Ok) {
        return pathStatus;
    }

    const AxiomFS::Status parentStatus = ensureParentDirectories(resolved);
    if (parentStatus != AxiomFS::Status::Ok) {
        return parentStatus;
    }

    std::error_code error;
    if (std::filesystem::exists(resolved, error)) {
        if (error) {
            return mapFilesystemError(error);
        }
        if (isSymlink(resolved, error) || error) {
            return error ? mapFilesystemError(error) : AxiomFS::Status::InvalidPath;
        }
        if (!std::filesystem::is_regular_file(resolved, error) || error) {
            return error ? mapFilesystemError(error) : AxiomFS::Status::InvalidPath;
        }
    }

    std::ofstream file(resolved, std::ios::binary | std::ios::trunc);
    if (!file) {
        return AxiomFS::Status::IoError;
    }
    if (size != 0) {
        file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    }
    return file.good() ? AxiomFS::Status::Ok : AxiomFS::Status::IoError;
}

AxiomFS::Status HostAxiomFSBackend::deleteFile(const std::string& path) {
    if (!m_mounted) {
        return AxiomFS::Status::NotMounted;
    }

    std::filesystem::path resolved;
    const AxiomFS::Status pathStatus = resolvePath(path, resolved, false);
    if (pathStatus != AxiomFS::Status::Ok) {
        return pathStatus;
    }

    std::error_code error;
    if (!std::filesystem::exists(resolved, error)) {
        return error ? mapFilesystemError(error) : AxiomFS::Status::NotFound;
    }
    if (isSymlink(resolved, error) || error) {
        return error ? mapFilesystemError(error) : AxiomFS::Status::InvalidPath;
    }
    if (!std::filesystem::is_regular_file(resolved, error) || error) {
        return error ? mapFilesystemError(error) : AxiomFS::Status::InvalidPath;
    }

    const bool removed = std::filesystem::remove(resolved, error);
    if (error) {
        return mapFilesystemError(error);
    }
    return removed ? AxiomFS::Status::Ok : AxiomFS::Status::NotFound;
}

AxiomFS::Status HostAxiomFSBackend::createDir(const std::string& path) {
    if (!m_mounted) {
        return AxiomFS::Status::NotMounted;
    }

    std::filesystem::path resolved;
    const AxiomFS::Status pathStatus = resolvePath(path, resolved, false);
    if (pathStatus != AxiomFS::Status::Ok) {
        return pathStatus;
    }

    const AxiomFS::Status parentStatus = ensureParentDirectories(resolved);
    if (parentStatus != AxiomFS::Status::Ok) {
        return parentStatus;
    }

    std::error_code error;
    if (std::filesystem::exists(resolved, error)) {
        if (error) {
            return mapFilesystemError(error);
        }
        if (isSymlink(resolved, error) || error) {
            return error ? mapFilesystemError(error) : AxiomFS::Status::InvalidPath;
        }
        return std::filesystem::is_directory(resolved, error) && !error
            ? AxiomFS::Status::Ok
            : AxiomFS::Status::AlreadyExists;
    }

    return std::filesystem::create_directory(resolved, error) && !error
        ? AxiomFS::Status::Ok
        : mapFilesystemError(error);
}

AxiomFS::Status HostAxiomFSBackend::renameFile(const std::string& oldPath,
                                               const std::string& newPath) {
    if (!m_mounted) {
        return AxiomFS::Status::NotMounted;
    }

    std::filesystem::path oldResolved;
    AxiomFS::Status status = resolvePath(oldPath, oldResolved, false);
    if (status != AxiomFS::Status::Ok) {
        return status;
    }
    std::filesystem::path newResolved;
    status = resolvePath(newPath, newResolved, false);
    if (status != AxiomFS::Status::Ok) {
        return status;
    }

    status = ensureParentDirectories(newResolved);
    if (status != AxiomFS::Status::Ok) {
        return status;
    }

    std::error_code error;
    if (!std::filesystem::exists(oldResolved, error)) {
        return error ? mapFilesystemError(error) : AxiomFS::Status::NotFound;
    }
    if (std::filesystem::exists(newResolved, error)) {
        return error ? mapFilesystemError(error) : AxiomFS::Status::AlreadyExists;
    }

    std::filesystem::rename(oldResolved, newResolved, error);
    return error ? mapFilesystemError(error) : AxiomFS::Status::Ok;
}

AxiomFS::ListResult HostAxiomFSBackend::listDir(const std::string& path) {
    AxiomFS::ListResult result;
    if (!m_mounted) {
        result.status = AxiomFS::Status::NotMounted;
        return result;
    }

    std::filesystem::path resolved;
    result.status = resolvePath(path, resolved, true);
    if (result.status != AxiomFS::Status::Ok) {
        return result;
    }

    std::error_code error;
    if (!std::filesystem::exists(resolved, error)) {
        result.status = error ? mapFilesystemError(error) : AxiomFS::Status::NotFound;
        return result;
    }
    if (isSymlink(resolved, error) || error) {
        result.status = error ? mapFilesystemError(error) : AxiomFS::Status::InvalidPath;
        return result;
    }
    if (!std::filesystem::is_directory(resolved, error) || error) {
        result.status = error ? mapFilesystemError(error) : AxiomFS::Status::InvalidPath;
        return result;
    }

    for (const auto& entry : std::filesystem::directory_iterator(resolved, error)) {
        if (error) {
            result.status = mapFilesystemError(error);
            return result;
        }

        std::error_code entryError;
        if (std::filesystem::is_symlink(entry.symlink_status(entryError)) || entryError) {
            continue;
        }

        AxiomFS::DirectoryEntry axiomEntry;
        axiomEntry.name = entry.path().filename().string();
        axiomEntry.isDirectory = entry.is_directory(entryError);
        if (entryError) {
            continue;
        }
        if (!axiomEntry.isDirectory && entry.is_regular_file(entryError) && !entryError) {
            axiomEntry.size = entry.file_size(entryError);
            if (entryError) {
                axiomEntry.size = 0;
            }
        }
        result.entries.push_back(std::move(axiomEntry));
    }

    std::sort(result.entries.begin(), result.entries.end(),
              [](const AxiomFS::DirectoryEntry& lhs, const AxiomFS::DirectoryEntry& rhs) {
                  return lhs.name < rhs.name;
              });
    result.status = AxiomFS::Status::Ok;
    return result;
}

AxiomFS::SpaceResult HostAxiomFSBackend::getFreeSpace() {
    AxiomFS::SpaceResult result;
    if (!m_mounted) {
        result.status = AxiomFS::Status::NotMounted;
        return result;
    }

    std::error_code error;
    const auto space = std::filesystem::space(m_root, error);
    if (error) {
        result.status = mapFilesystemError(error);
        return result;
    }
    result.status = AxiomFS::Status::Ok;
    result.bytes = space.available;
    return result;
}

AxiomFS::SpaceResult HostAxiomFSBackend::getTotalSpace() {
    AxiomFS::SpaceResult result;
    if (!m_mounted) {
        result.status = AxiomFS::Status::NotMounted;
        return result;
    }

    std::error_code error;
    const auto space = std::filesystem::space(m_root, error);
    if (error) {
        result.status = mapFilesystemError(error);
        return result;
    }
    result.status = AxiomFS::Status::Ok;
    result.bytes = space.capacity;
    return result;
}

const std::filesystem::path& HostAxiomFSBackend::root() const {
    return m_root;
}

const char* HostAxiomFSBackend::backendName() const {
    return "Host folder";
}

bool HostAxiomFSBackend::isMounted() const {
    return m_mounted;
}

AxiomFS::Diagnostics HostAxiomFSBackend::getDiagnostics() {
    AxiomFS::Diagnostics diagnostics;
    diagnostics.backendName = backendName();
    diagnostics.mounted = m_mounted;
    diagnostics.mountStatus = m_mounted ? AxiomFS::Status::Ok : AxiomFS::Status::NotMounted;
    diagnostics.status = m_mounted
        ? AxiomFS::FilesystemStatus::Healthy
        : AxiomFS::FilesystemStatus::NotMounted;

    if (m_mounted) {
        const AxiomFS::SpaceResult total = getTotalSpace();
        const AxiomFS::SpaceResult free = getFreeSpace();
        if (total.ok() && free.ok()) {
            diagnostics.spaceKnown = true;
            diagnostics.totalBytes = total.bytes;
            diagnostics.freeBytes = free.bytes;
            diagnostics.usedBytes = total.bytes >= free.bytes ? total.bytes - free.bytes : 0;
        }
    }

    return diagnostics;
}

AxiomFS::ProbeResult HostAxiomFSBackend::runProbe() {
    AxiomFS::ProbeResult result;
    result.mountedBeforeProbe = m_mounted;
    result.mountStatus = mount();
    result.mountFailureReason = lastMountFailureReason();
    result.mountedAfterProbe = m_mounted;
    result.probeStatus = result.mountStatus;
    std::printf("[fs][host] probe mount=%s mounted=%s\n",
                AxiomFS::statusToString(result.mountStatus),
                result.mountedAfterProbe ? "yes" : "no");
    return result;
}

AxiomFS::Status HostAxiomFSBackend::resolvePath(const std::string& path,
                                                std::filesystem::path& resolved,
                                                bool allowRoot) const {
    std::string normalized;
    const AxiomFS::Status status = AxiomFS::normalizePath(path, normalized);
    if (status != AxiomFS::Status::Ok) {
        return status;
    }
    if (normalized.empty() && !allowRoot) {
        return AxiomFS::Status::InvalidPath;
    }

    resolved = normalized.empty() ? m_root : (m_root / std::filesystem::path(normalized));
    return AxiomFS::Status::Ok;
}

AxiomFS::Status HostAxiomFSBackend::ensureParentDirectories(const std::filesystem::path& path) const {
    std::filesystem::path current = m_root;
    const std::filesystem::path relative = std::filesystem::relative(path.parent_path(), m_root);
    if (relative.empty() || relative == ".") {
        return AxiomFS::Status::Ok;
    }

    for (const auto& component : relative) {
        if (component == ".") {
            continue;
        }

        current /= component;
        std::error_code error;
        if (std::filesystem::exists(current, error)) {
            if (error) {
                return mapFilesystemError(error);
            }
            if (isSymlink(current, error) || error) {
                return error ? mapFilesystemError(error) : AxiomFS::Status::InvalidPath;
            }
            if (!std::filesystem::is_directory(current, error) || error) {
                return error ? mapFilesystemError(error) : AxiomFS::Status::InvalidPath;
            }
            continue;
        }

        if (!std::filesystem::create_directory(current, error) || error) {
            return error ? mapFilesystemError(error) : AxiomFS::Status::IoError;
        }
    }

    return AxiomFS::Status::Ok;
}
