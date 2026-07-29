#include "hal/fs/axiom_fs.h"
#include "app/calculator/calculator_app.h"
#include "platform/host/axiom_fs_host.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <map>
#include <set>
#include <string>

namespace {

std::filesystem::path testRoot() {
    return std::filesystem::temp_directory_path() / "mi23_axiom_fs_smoke";
}

class FsTestDisplay : public Display {
public:
    void init() override {}
    void clear(Color) override {}
    void drawPixel(int, int, Color) override {}
    void fillRect(int, int, int, int, Color) override {}
    void drawText(const char*, int, int, Color) override {}
    void present() override {}
};

class FsTestKeypad : public Keypad {
public:
    void init() override {}
    Key getKey() override { return Key::NONE; }
};

class MockAxiomFSBackend : public AxiomFS::Backend {
public:
    MockAxiomFSBackend(AxiomFS::Status mountStatus,
                       AxiomFS::MountFailureReason mountFailureReason,
                       AxiomFS::Status formatStatus = AxiomFS::Status::Unsupported,
                       bool freshBlankFilesystem = false,
                       bool mountAfterFormat = false)
        : m_mountStatus(mountStatus)
        , m_mountFailureReason(mountFailureReason)
        , m_formatStatus(formatStatus)
        , m_freshBlankFilesystem(freshBlankFilesystem)
        , m_mountAfterFormat(mountAfterFormat) {}

    AxiomFS::Status mount() override {
        ++m_mountCalls;
        const AxiomFS::Status status = m_formatted && m_mountAfterFormat
            ? AxiomFS::Status::Ok
            : m_mountStatus;
        m_mounted = status == AxiomFS::Status::Ok;
        if (m_mounted) {
            m_mountFailureReason = AxiomFS::MountFailureReason::None;
        }
        return status;
    }

    AxiomFS::Status format() override {
        ++m_formatCalls;
        if (m_formatStatus == AxiomFS::Status::Ok) {
            m_formatted = true;
            m_mounted = false;
        }
        return m_formatStatus;
    }

    AxiomFS::Status exists(const std::string& path, bool& outExists) override {
        if (!m_mounted) {
            outExists = false;
            return AxiomFS::Status::NotMounted;
        }
        outExists = m_directories.count(path) > 0 || m_files.count(path) > 0;
        return AxiomFS::Status::Ok;
    }

    AxiomFS::ReadResult readFile(const std::string& path) override {
        AxiomFS::ReadResult result;
        if (!m_mounted) {
            result.status = AxiomFS::Status::NotMounted;
            return result;
        }
        const auto found = m_files.find(path);
        if (found == m_files.end()) {
            result.status = AxiomFS::Status::NotFound;
            return result;
        }
        result.status = AxiomFS::Status::Ok;
        result.data.assign(found->second.begin(), found->second.end());
        return result;
    }

    AxiomFS::Status writeFile(const std::string& path, const uint8_t* data, std::size_t size) override {
        if (!m_mounted) {
            return AxiomFS::Status::NotMounted;
        }
        m_files[path] = data && size > 0
            ? std::string(reinterpret_cast<const char*>(data), size)
            : std::string();
        return AxiomFS::Status::Ok;
    }

    AxiomFS::Status deleteFile(const std::string& path) override {
        if (!m_mounted) {
            return AxiomFS::Status::NotMounted;
        }
        m_files.erase(path);
        return AxiomFS::Status::Ok;
    }

    AxiomFS::Status createDir(const std::string& path) override {
        if (!m_mounted) {
            return AxiomFS::Status::NotMounted;
        }
        m_directories.insert(path);
        return AxiomFS::Status::Ok;
    }

    AxiomFS::Status renameFile(const std::string&, const std::string&) override {
        return AxiomFS::Status::NotMounted;
    }

    AxiomFS::ListResult listDir(const std::string&) override {
        return {};
    }

    AxiomFS::SpaceResult getFreeSpace() override {
        return {};
    }

    AxiomFS::SpaceResult getTotalSpace() override {
        return {};
    }

    const char* backendName() const override {
        return "mock";
    }

    AxiomFS::MountFailureReason lastMountFailureReason() const override {
        return m_mountFailureReason;
    }

    bool isFreshBlankFilesystem() const override {
        return m_freshBlankFilesystem;
    }

    int mountCalls() const {
        return m_mountCalls;
    }

    int formatCalls() const {
        return m_formatCalls;
    }

private:
    AxiomFS::Status m_mountStatus;
    AxiomFS::MountFailureReason m_mountFailureReason;
    AxiomFS::Status m_formatStatus;
    bool m_freshBlankFilesystem;
    bool m_mountAfterFormat;
    bool m_mounted = false;
    bool m_formatted = false;
    int m_mountCalls = 0;
    int m_formatCalls = 0;
    std::set<std::string> m_directories;
    std::map<std::string, std::string> m_files;
};

} // namespace

TEST(AxiomFSHost, SmokeTestRoundTripsFileOperations) {
    const std::filesystem::path root = testRoot();
    std::error_code error;
    std::filesystem::remove_all(root, error);

    HostAxiomFSBackend backend(root);
    AxiomFS::FileSystem fs(backend);

    ASSERT_EQ(fs.mount(), AxiomFS::Status::Ok);
    EXPECT_TRUE(std::filesystem::is_directory(root));

    const std::string data = "MI-23 filesystem smoke test";
    EXPECT_EQ(fs.writeFile("state/smoke.txt", data), AxiomFS::Status::Ok);

    bool exists = false;
    EXPECT_EQ(fs.exists("state/smoke.txt", exists), AxiomFS::Status::Ok);
    EXPECT_TRUE(exists);

    const AxiomFS::ReadResult read = fs.readFile("state/smoke.txt");
    ASSERT_TRUE(read.ok());
    EXPECT_EQ(std::string(read.data.begin(), read.data.end()), data);

    const AxiomFS::ListResult rootListing = fs.listDir("/");
    ASSERT_TRUE(rootListing.ok());

    const AxiomFS::ListResult listing = fs.listDir("state");
    ASSERT_TRUE(listing.ok());
    const auto found = std::find_if(listing.entries.begin(),
                                    listing.entries.end(),
                                    [](const AxiomFS::DirectoryEntry& entry) {
                                        return entry.name == "smoke.txt"
                                            && !entry.isDirectory
                                            && entry.size > 0;
                                    });
    EXPECT_NE(found, listing.entries.end());

    EXPECT_EQ(fs.deleteFile("state/smoke.txt"), AxiomFS::Status::Ok);
    EXPECT_EQ(fs.exists("state/smoke.txt", exists), AxiomFS::Status::Ok);
    EXPECT_FALSE(exists);

    std::filesystem::remove_all(root, error);
}

TEST(AxiomFSHost, RejectsTraversalOutsideStorageRoot) {
    const std::filesystem::path root = testRoot();
    std::error_code error;
    std::filesystem::remove_all(root, error);

    HostAxiomFSBackend backend(root);
    AxiomFS::FileSystem fs(backend);
    ASSERT_EQ(fs.mount(), AxiomFS::Status::Ok);

    EXPECT_EQ(fs.writeFile("../escape.txt", "bad"), AxiomFS::Status::InvalidPath);
    EXPECT_FALSE(std::filesystem::exists(root.parent_path() / "escape.txt"));

    std::filesystem::remove_all(root, error);
}

TEST(AxiomFSHost, HealthCheckCreatesDefaultLayout) {
    const std::filesystem::path root = testRoot();
    std::error_code error;
    std::filesystem::remove_all(root, error);

    HostAxiomFSBackend backend(root);
    AxiomFS::FileSystem fs(backend);

    const AxiomFS::HealthResult health = AxiomFS::initialize(fs);
    EXPECT_EQ(health.status, AxiomFS::FilesystemStatus::Healthy);
    EXPECT_TRUE(health.defaultLayoutReady);
    EXPECT_TRUE(health.readWriteReady);

    for (int i = 0; i < AxiomFS::defaultDirectoryCount(); ++i) {
        EXPECT_TRUE(std::filesystem::is_directory(root / AxiomFS::defaultDirectories()[i]));
    }

    std::filesystem::remove_all(root, error);
}

TEST(AxiomFSHealth, DistinguishesUnformattedFilesystemFromCorruption) {
    MockAxiomFSBackend backend(AxiomFS::Status::NotFound,
                               AxiomFS::MountFailureReason::NotFormatted);
    AxiomFS::FileSystem fs(backend);

    const AxiomFS::HealthResult health = AxiomFS::initialize(fs);

    EXPECT_EQ(health.status, AxiomFS::FilesystemStatus::Unformatted);
    EXPECT_EQ(health.mountStatus, AxiomFS::Status::NotFound);
    EXPECT_EQ(health.mountFailureReason, AxiomFS::MountFailureReason::NotFormatted);
}

TEST(AxiomFSHealth, DistinguishesCorruptFilesystemFromUnformattedFilesystem) {
    MockAxiomFSBackend backend(AxiomFS::Status::IoError,
                               AxiomFS::MountFailureReason::Corrupt);
    AxiomFS::FileSystem fs(backend);

    const AxiomFS::HealthResult health = AxiomFS::initialize(fs);

    EXPECT_EQ(health.status, AxiomFS::FilesystemStatus::Error);
    EXPECT_EQ(health.mountStatus, AxiomFS::Status::IoError);
    EXPECT_EQ(health.mountFailureReason, AxiomFS::MountFailureReason::Corrupt);
}

TEST(AxiomFSHealth, MissingMagicOnNonblankStorageIsCorruptNotUnformatted) {
    MockAxiomFSBackend backend(AxiomFS::Status::NotFound,
                               AxiomFS::MountFailureReason::MissingMagic);
    AxiomFS::FileSystem fs(backend);

    const AxiomFS::HealthResult health = AxiomFS::initialize(fs);

    EXPECT_EQ(health.status, AxiomFS::FilesystemStatus::Error);
    EXPECT_EQ(health.mountStatus, AxiomFS::Status::NotFound);
    EXPECT_EQ(health.mountFailureReason, AxiomFS::MountFailureReason::MissingMagic);
}

TEST(AxiomFSBoot, FreshBlankFilesystemFormatsAndMounts) {
    MockAxiomFSBackend backend(AxiomFS::Status::NotFound,
                               AxiomFS::MountFailureReason::NotFormatted,
                               AxiomFS::Status::Ok,
                               true,
                               true);
    AxiomFS::FileSystem fs(backend);

    const AxiomFS::HealthResult health = AxiomFS::initializeForBoot(fs, "[fs][test]");

    EXPECT_EQ(health.status, AxiomFS::FilesystemStatus::Healthy);
    EXPECT_EQ(health.mountStatus, AxiomFS::Status::Ok);
    EXPECT_TRUE(health.defaultLayoutReady);
    EXPECT_TRUE(health.readWriteReady);
    EXPECT_GE(backend.mountCalls(), 2);
    EXPECT_EQ(backend.formatCalls(), 1);

    bool graphsExists = false;
    EXPECT_EQ(fs.exists("graphs", graphsExists), AxiomFS::Status::Ok);
    EXPECT_TRUE(graphsExists);
    EXPECT_EQ(AxiomFS::getLastHealthResult().status, AxiomFS::FilesystemStatus::Healthy);
}

TEST(AxiomFSBoot, NonblankMissingMagicDoesNotAutoFormat) {
    MockAxiomFSBackend backend(AxiomFS::Status::NotFound,
                               AxiomFS::MountFailureReason::MissingMagic,
                               AxiomFS::Status::Ok,
                               false,
                               true);
    AxiomFS::FileSystem fs(backend);

    const AxiomFS::HealthResult health = AxiomFS::initializeForBoot(fs, "[fs][test]");

    EXPECT_EQ(health.status, AxiomFS::FilesystemStatus::Error);
    EXPECT_EQ(health.mountStatus, AxiomFS::Status::NotFound);
    EXPECT_EQ(health.mountFailureReason, AxiomFS::MountFailureReason::MissingMagic);
    EXPECT_EQ(backend.formatCalls(), 0);
}

TEST(AxiomFSHealth, DistinguishesBackendUnavailable) {
    MockAxiomFSBackend backend(AxiomFS::Status::Unsupported,
                               AxiomFS::MountFailureReason::BackendUnavailable);
    AxiomFS::FileSystem fs(backend);

    const AxiomFS::HealthResult health = AxiomFS::initialize(fs);

    EXPECT_EQ(health.status, AxiomFS::FilesystemStatus::NotMounted);
    EXPECT_EQ(health.mountStatus, AxiomFS::Status::Unsupported);
    EXPECT_EQ(health.mountFailureReason, AxiomFS::MountFailureReason::BackendUnavailable);
}

TEST(AxiomFSHost, FormatAndInitializeCreatesDefaultLayout) {
    const std::filesystem::path root = testRoot();
    std::error_code error;
    std::filesystem::remove_all(root, error);

    HostAxiomFSBackend backend(root);
    AxiomFS::FileSystem fs(backend);
    ASSERT_EQ(fs.mount(), AxiomFS::Status::Ok);
    ASSERT_EQ(fs.writeFile("graphs/old.mi23graph", "old"), AxiomFS::Status::Ok);

    const AxiomFS::HealthResult health = AxiomFS::formatAndInitialize(fs);

    EXPECT_EQ(health.status, AxiomFS::FilesystemStatus::Healthy);
    EXPECT_TRUE(health.defaultLayoutReady);
    EXPECT_TRUE(health.readWriteReady);
    for (int i = 0; i < AxiomFS::defaultDirectoryCount(); ++i) {
        EXPECT_TRUE(std::filesystem::is_directory(root / AxiomFS::defaultDirectories()[i]));
    }
    EXPECT_FALSE(std::filesystem::exists(root / "graphs" / "old.mi23graph"));

    std::filesystem::remove_all(root, error);
}

TEST(AxiomFSHost, CalculatorHistoryPersistsAsReadableJson) {
    const std::filesystem::path root = testRoot();
    std::error_code error;
    std::filesystem::remove_all(root, error);

    HostAxiomFSBackend backend(root);
    AxiomFS::FileSystem fs(backend);
    ASSERT_EQ(AxiomFS::initialize(fs).status, AxiomFS::FilesystemStatus::Healthy);

    FsTestDisplay display;
    FsTestKeypad keypad;
    CalculatorAppConfig config{};
    config.filesystem = &fs;

    CalculatorApp saved(display, keypad, config);
    saved.handleKey(Key::NUM_1);
    saved.handleKey(Key::PLUS);
    saved.handleKey(Key::NUM_2);
    saved.handleKey(Key::ENTER);
    ASSERT_EQ(saved.historySize(), 1);

    CalculatorApp loaded(display, keypad, config);
    EXPECT_TRUE(loaded.loadPersistentHistory());
    ASSERT_EQ(loaded.historySize(), 1);
    EXPECT_EQ(loaded.historyAt(0).input, "1+2");
    EXPECT_EQ(loaded.historyAt(0).result, "3.000000");
    EXPECT_FALSE(loaded.historyAt(0).isError);

    const AxiomFS::ReadResult read = fs.readFile("cache/history.json");
    ASSERT_TRUE(read.ok());
    const std::string json(read.data.begin(), read.data.end());
    EXPECT_NE(json.find("\"entries\""), std::string::npos);

    std::filesystem::remove_all(root, error);
}
TEST(AxiomFS, RangedReadsAndWritesRespectOffsetsAndTruncation) {
    const auto root = std::filesystem::temp_directory_path() / "mi23_axiom_range_tests";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    HostAxiomFSBackend backend(root);
    AxiomFS::FileSystem fs(backend);
    ASSERT_EQ(fs.mount(), AxiomFS::Status::Ok);

    const uint8_t initial[] = {'a', 'b', 'c'};
    ASSERT_EQ(fs.writeRange("range.bin", 0, initial, sizeof(initial), true), AxiomFS::Status::Ok);
    auto part = fs.readRange("range.bin", 1, 1);
    ASSERT_TRUE(part.ok());
    EXPECT_EQ(part.data, std::vector<uint8_t>({'b'}));
    EXPECT_FALSE(part.eof);
    EXPECT_EQ(part.totalSize, 3u);

    const uint8_t suffix[] = {'X', 'Y'};
    EXPECT_EQ(fs.writeRange("range.bin", 3, suffix, sizeof(suffix), false), AxiomFS::Status::Ok);
    EXPECT_EQ(fs.writeRange("range.bin", 6, suffix, sizeof(suffix), false), AxiomFS::Status::InvalidPath);
    auto tail = fs.readRange("range.bin", 3, 10);
    ASSERT_TRUE(tail.ok());
    EXPECT_EQ(tail.data, std::vector<uint8_t>({'X', 'Y'}));
    EXPECT_TRUE(tail.eof);
    EXPECT_EQ(fs.readRange("range.bin", 6, 1).status, AxiomFS::Status::InvalidPath);
    std::filesystem::remove_all(root, error);
}
