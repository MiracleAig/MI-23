#pragma once

#include "app/settings/settings_state.h"
#include "hal/display.h"
#include "hal/fs/axiom_fs.h"
#include "hal/keypad.h"

class SettingsApp {
public:
    SettingsApp(Display& display,
                SettingsState& settings,
                const char* platformName,
                AxiomFS::FileSystem* filesystem = nullptr);

    void enter();
    bool handleKey(Key key);
    bool hasPendingChanges() const;
    bool consumeSaveRequest();
    void markSaved();
    void renderContent(int x, int y, int w, int h);
    void requestRender();
    bool needsRender() const;

private:
    enum class Screen {
        Main,
        About,
        Developer,
        ResetConfirm,
        Storage,
        FormatConfirm,
        FilesystemStatus,
        FilesystemCheck,
        DeveloperFormatConfirm,
        EraseConfirm,
    };

    Display& m_display;
    SettingsState& m_settings;
    const char* m_platformName;
    AxiomFS::FileSystem* m_filesystem;
    Screen m_screen;
    int m_selectedIndex;
    int m_developerIndex;
    int m_storageIndex;
    char m_storageMessage[96];
    char m_developerMessage[96];
    AxiomFS::ProbeResult m_probeResult;
    bool m_hasProbeResult;
    bool m_dirty;
    bool m_saveRequested;
    bool m_needsRender;
    DirtyRegionList m_dirtyRegions;
    DisplayRect m_contentBounds;

    void renderMain(int x, int y, int w, int h);
    void renderAbout(int x, int y, int w, int h);
    void renderDeveloper(int x, int y, int w, int h);
    void renderResetConfirm(int x, int y, int w, int h);
    void renderStorage(int x, int y, int w, int h);
    void renderFormatConfirm(int x, int y, int w, int h);
    void renderFilesystemStatus(int x, int y, int w, int h);
    void renderFilesystemCheck(int x, int y, int w, int h);
    void renderDeveloperFormatConfirm(int x, int y, int w, int h);
    void renderEraseConfirm(int x, int y, int w, int h);
    void cycleSelected(int direction);
    void cyclePrecision(int direction);
    void toggleDeveloperSelected();
    void runDeveloperAction();
    void runStorageAction();
    void setStorageMessage(const char* message);
    void setDeveloperMessage(const char* message);
    void markChanged();
    void invalidateRect(DisplayRect rect);
    void invalidateContent();
    DisplayRect mainRowRect(int x, int y, int w, int index) const;
    DisplayRect developerRowRect(int x, int y, int w, int index) const;
};
