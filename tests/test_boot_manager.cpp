#include "app/boot/boot_manager.h"
#include "platform/host/settings_store_host.h"
#include "platform/host/startup_host.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <string>

namespace {

class BootNullDisplay : public Display {
public:
    void init() override {}
    void clear(Color) override {}
    void drawPixel(int, int, Color) override {}
    void fillRect(int, int, int, int, Color) override {}
    void drawText(const char*, int, int, Color) override {}
    void present() override {}
};

class BootKeypadStub : public Keypad {
public:
    void init() override { initialized = true; }
    Key getKey() override { return Key::NONE; }

    bool initialized = false;
};

class FakeStartupBackend : public StartupBackend {
public:
    const char* platformName() const override { return "Test"; }
    const char* firmwareVersion() const override { return "Firmware: test"; }

    StartupCheckResult initializeInput() override {
        initializedInput = true;
        return {};
    }

    StartupCheckResult loadSettings(SettingsState& settings) override {
        settings.angleMode = AngleMode::Degrees;
        return {};
    }

    StartupCheckResult checkStorage() override { return storageResult; }
    StartupCheckResult verifyResources(SettingsState&) override { return {}; }
    StartupCheckResult startRuntime(SettingsState&) override { return {}; }
    StartupCheckResult formatStorage() override {
        ++formatCalls;
        return formatResult;
    }

    bool initializedInput = false;
    int formatCalls = 0;
    StartupCheckResult storageResult{};
    StartupCheckResult formatResult{true, true, true, "Storage formatted and verified."};
};

std::filesystem::path uniquePath(const char* name) {
    return std::filesystem::temp_directory_path() / "mi23_boot_tests" / name;
}

struct ScopedPathCleanup {
    explicit ScopedPathCleanup(std::filesystem::path path)
        : path(std::move(path)) {}

    ~ScopedPathCleanup() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }

    std::filesystem::path path;
};

void writeCorruptSettingsFile(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::array<unsigned char, SettingsStore::kSerializedSize> data{};
    data.fill(0xCCu);
    FILE* file = std::fopen(path.c_str(), "wb");
    ASSERT_NE(file, nullptr);
    ASSERT_EQ(std::fwrite(data.data(), 1, data.size(), file), data.size());
    ASSERT_EQ(std::fclose(file), 0);
}

} // namespace

TEST(BootManager, SuccessfulBootSequenceTransitionsToFinishedState) {
    BootNullDisplay display;
    SettingsState settings;
    FakeStartupBackend backend;
    BootManager boot(display, settings, backend);

    boot.begin();

    for (int i = 0; i < 16 && !boot.isFinished(); ++i) {
        boot.tick();
        if (boot.needsRender()) {
            boot.render();
        }
    }

    EXPECT_TRUE(boot.isFinished());
    EXPECT_TRUE(boot.bootSucceeded());
    EXPECT_TRUE(backend.initializedInput);
    EXPECT_EQ(settings.angleMode, AngleMode::Degrees);
}

TEST(BootManager, CorruptStorageCanContinueDegradedWithoutFormatting) {
    BootNullDisplay display;
    SettingsState settings;
    FakeStartupBackend backend;
    backend.storageResult = {false, true, false, "Storage corrupt.", true};
    BootManager boot(display, settings, backend);
    boot.begin();
    for (int i = 0; i < 12 && !boot.canContinue(); ++i) boot.tick();

    ASSERT_TRUE(boot.canContinue());
    boot.handleKey(Key::ENTER);
    EXPECT_TRUE(boot.isFinished());
    EXPECT_FALSE(boot.bootSucceeded());
    EXPECT_EQ(backend.formatCalls, 0);
}

TEST(BootManager, StorageRecoveryRequiresTwoDestructiveConfirmations) {
    BootNullDisplay display;
    SettingsState settings;
    FakeStartupBackend backend;
    backend.storageResult = {false, true, false, "Storage corrupt.", true};
    BootManager boot(display, settings, backend);
    boot.begin();
    for (int i = 0; i < 12 && !boot.canContinue(); ++i) boot.tick();

    boot.handleKey(Key::DELETE_KEY);
    EXPECT_EQ(backend.formatCalls, 0);
    boot.handleKey(Key::DELETE_KEY);
    EXPECT_EQ(backend.formatCalls, 0);
    boot.handleKey(Key::DELETE_KEY);
    EXPECT_EQ(backend.formatCalls, 1);

    for (int i = 0; i < 12 && !boot.isFinished(); ++i) boot.tick();
    EXPECT_TRUE(boot.isFinished());
    EXPECT_TRUE(boot.bootSucceeded());
}

TEST(BootManager, ClearCancelsStorageFormatConfirmation) {
    BootNullDisplay display;
    SettingsState settings;
    FakeStartupBackend backend;
    backend.storageResult = {false, true, false, "Storage corrupt.", true};
    BootManager boot(display, settings, backend);
    boot.begin();
    for (int i = 0; i < 12 && !boot.canContinue(); ++i) boot.tick();

    boot.handleKey(Key::DELETE_KEY);
    boot.handleKey(Key::CLEAR);
    EXPECT_TRUE(boot.canContinue());
    EXPECT_EQ(backend.formatCalls, 0);
}

TEST(HostStartupBackend, MissingSettingsFileLoadsDefaultsAndRepairsPersistence) {
    const auto root = uniquePath("missing_settings_case");
    ScopedPathCleanup cleanup(root);

    BootKeypadStub keypad;
    HostSettingsStore store((root / "state" / "settings.bin").string());
    HostStartupBackend backend(keypad, store);
    SettingsState settings;
    settings.angleMode = AngleMode::Degrees;

    EXPECT_TRUE(backend.initializeInput().ok);
    const StartupCheckResult loadResult = backend.loadSettings(settings);
    EXPECT_TRUE(loadResult.ok);
    EXPECT_TRUE(loadResult.repaired);
    EXPECT_EQ(settings.angleMode, SettingsState::kDefaultAngleMode);

    EXPECT_TRUE(backend.checkStorage().ok);
    EXPECT_TRUE(backend.verifyResources(settings).ok);
    EXPECT_TRUE(std::filesystem::exists(root / "state" / "settings.bin"));
}

TEST(HostStartupBackend, CorruptedSettingsFallbackToDefaults) {
    const auto root = uniquePath("corrupt_settings_case");
    ScopedPathCleanup cleanup(root);
    const auto settingsPath = root / "state" / "settings.bin";
    writeCorruptSettingsFile(settingsPath);

    BootKeypadStub keypad;
    HostSettingsStore store(settingsPath.string());
    HostStartupBackend backend(keypad, store);
    SettingsState settings;
    settings.angleMode = AngleMode::Degrees;

    const StartupCheckResult result = backend.loadSettings(settings);
    EXPECT_TRUE(result.ok);
    EXPECT_TRUE(result.repaired);
    EXPECT_EQ(settings.angleMode, SettingsState::kDefaultAngleMode);
}

TEST(HostStartupBackend, MissingFilesystemDirectoriesAreRecreated) {
    const auto root = uniquePath("directory_repair_case");
    ScopedPathCleanup cleanup(root);

    BootKeypadStub keypad;
    HostSettingsStore store((root / "state" / "settings.bin").string());
    HostStartupBackend backend(keypad, store);
    SettingsState settings;

    const StartupCheckResult storageResult = backend.checkStorage();
    EXPECT_TRUE(storageResult.ok);
    EXPECT_TRUE(std::filesystem::is_directory(root / "state"));

    const StartupCheckResult resourceResult = backend.verifyResources(settings);
    EXPECT_TRUE(resourceResult.ok);
    bool logsExists = false;
    EXPECT_EQ(backend.filesystem().exists("logs", logsExists), AxiomFS::Status::Ok);
    EXPECT_TRUE(logsExists);
}

TEST(HostStartupBackend, UnrecoverableStorageFailureFallsBackSafely) {
    const auto root = uniquePath("storage_failure_case");
    ScopedPathCleanup cleanup(root);
    std::filesystem::create_directories(root.parent_path());
    FILE* file = std::fopen(root.c_str(), "wb");
    ASSERT_NE(file, nullptr);
    ASSERT_NE(std::fputs("not a directory", file), EOF);
    ASSERT_EQ(std::fclose(file), 0);

    BootKeypadStub keypad;
    HostSettingsStore store((root / "settings.bin").string());
    HostStartupBackend backend(keypad, store);

    const StartupCheckResult result = backend.checkStorage();
    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(result.continueAllowed);
}
