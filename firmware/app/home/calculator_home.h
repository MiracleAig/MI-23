//
// Created by Miracle Aigbogun on 4/19/26.
//

#pragma once

#include "hal/display.h"
#include "hal/keypad.h"

enum class AppId {
    Home,
    Calculator,
    Graphing,
};

class HomeScreen {
public:
    explicit HomeScreen(Display& display);

    void enter();
    AppId handleKey(Key key);
    void render();
    void renderContent(int contentY, int contentHeight);

private:
    Display& m_display;
    int m_selectedIndex;

    void renderContentArea(int contentY, int contentHeight);
    void moveSelection(int deltaCol, int deltaRow);
};
