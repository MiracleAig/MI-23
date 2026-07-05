//
// Created by Miracle Aigbogun on 4/19/26.
//

#pragma once

#include "hal/display.h"
#include "hal/keypad.h"

enum class AppId {
    Boot,
    Home,
    Calculator,
    Graphing,
    Files,
    Settings,
    Matrix,
};

class HomeScreen {
public:
    explicit HomeScreen(Display& display);

    void enter();
    AppId handleKey(Key key);
    void render();
    void renderContent(int contentY, int contentHeight);
    bool needsRender() const;

private:
    Display& m_display;
    int m_selectedIndex;
    int m_contentY;
    int m_contentHeight;
    bool m_needsRender;
    DirtyRegionList m_dirtyRegions;

    void renderContentArea(int contentY, int contentHeight);
    void invalidateContent(int contentY, int contentHeight);
    void invalidateSelectionChange(int oldIndex, int newIndex);
    void moveSelection(int deltaCol, int deltaRow);
    void invalidateRect(DisplayRect rect);
};
