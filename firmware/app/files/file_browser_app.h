#pragma once

#include "hal/display.h"
#include "hal/fs/axiom_fs.h"
#include "hal/keypad.h"

#include <string>

class FileBrowserApp {
public:
    explicit FileBrowserApp(Display& display, AxiomFS::FileSystem* filesystem = nullptr);

    void enter();
    void handleKey(Key key);
    void renderContent(int x, int y, int w, int h);
    bool needsRender() const;
    void requestRender();

    const std::string& currentPathForTest() const;
    bool canDeletePathForTest(const std::string& path) const;

private:
    enum class Screen {
        List,
        Details,
        DeleteConfirm,
    };

    Display& m_display;
    AxiomFS::FileSystem* m_filesystem;
    Screen m_screen;
    std::string m_path;
    AxiomFS::ListResult m_listing;
    int m_selectedIndex;
    bool m_needsRefresh;
    bool m_needsRender;
    DisplayRect m_bounds;

    void refresh();
    void openSelected();
    void goUp();
    void requestDelete();
    void confirmDelete();
    void renderList(int x, int y, int w, int h);
    void renderDetails(int x, int y, int w, int h);
    void renderDeleteConfirm(int x, int y, int w, int h);
    const AxiomFS::DirectoryEntry* selectedEntry() const;
    std::string selectedPath() const;
    std::string childPath(const std::string& name) const;
    static std::string parentPath(const std::string& path);
};
