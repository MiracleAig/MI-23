#include "hal/fs/axiom_fs.h"
#include "app/calculator/calculator_app.h"
#include "platform/host/axiom_fs_host.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
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
                       AxiomFS::MountFailureReason mountFailureReason)
        : m_mountStatus(mountStatus)
        , m_mountFailureReason(mountFailureReason) {}

    AxiomFS::Status mount() override {
        return m_mountStatus;
    }

    AxiomFS::Status format() override {
        return AxiomFS::Status::Unsupported;
    }

    AxiomFS::Status exists(const std::string&, bool& outExists) override {
        outExists = false;
        return AxiomFS::Status::NotMounted;
    }

    AxiomFS::ReadResult readFile(const std::string&) override {
        return {};
    }

    AxiomFS::Status writeFile(const std::string&, const uint8_t*, std::size_t) override {
        return AxiomFS::Status::NotMounted;
    }

    AxiomFS::Status deleteFile(const std::string&) override {
        return AxiomFS::Status::NotMounted;
    }

    AxiomFS::Status createDir(const std::string&) override {
        return AxiomFS::Status::NotMounted;
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

private:
    AxiomFS::Status m_mountStatus;
    AxiomFS::MountFailureReason m_mountFailureReason;
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
