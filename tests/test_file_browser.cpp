#include "app/files/file_browser_app.h"
#include "platform/host/axiom_fs_host.h"

#include <gtest/gtest.h>

#include <filesystem>

namespace {

class BrowserNullDisplay : public Display {
public:
    void init() override {}
    void clear(Color) override {}
    void drawPixel(int, int, Color) override {}
    void fillRect(int, int, int, int, Color) override {}
    void drawText(const char*, int, int, Color) override {}
    void present() override {}
};

std::filesystem::path browserRoot() {
    return std::filesystem::temp_directory_path() / "mi23_file_browser_tests";
}

} // namespace

TEST(FileBrowserApp, ProtectsSystemFoldersAndRejectsTraversal) {
    BrowserNullDisplay display;
    FileBrowserApp browser(display, nullptr);

    EXPECT_FALSE(browser.canDeletePathForTest("../outside"));
    EXPECT_FALSE(browser.canDeletePathForTest("settings"));
    EXPECT_FALSE(browser.canDeletePathForTest("graphs"));
    EXPECT_TRUE(browser.canDeletePathForTest("notes/readme.txt"));
}

TEST(FileBrowserApp, EmptyDirectoryDoesNotCrashNavigation) {
    const std::filesystem::path root = browserRoot();
    std::error_code error;
    std::filesystem::remove_all(root, error);

    HostAxiomFSBackend backend(root);
    AxiomFS::FileSystem fs(backend);
    ASSERT_EQ(AxiomFS::initialize(fs).status, AxiomFS::FilesystemStatus::Healthy);

    BrowserNullDisplay display;
    FileBrowserApp browser(display, &fs);
    browser.enter();
    browser.handleKey(Key::ENTER);
    browser.handleKey(Key::CLEAR);
    browser.renderContent(0, 22, DISPLAY_WIDTH, DISPLAY_HEIGHT - 22);

    EXPECT_EQ(browser.currentPathForTest(), "");

    std::filesystem::remove_all(root, error);
}
