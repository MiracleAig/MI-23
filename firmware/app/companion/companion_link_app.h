#pragma once

#include "core/companion/CompanionSession.h"
#include "hal/display.h"
#include "hal/keypad.h"

class CompanionLinkApp {
public:
    CompanionLinkApp(Display& display, Companion::CompanionSession& session);

    void enter(uint64_t nowMs);
    bool handleKey(Key key);
    void tick(uint64_t nowMs);
    void renderContent(int x, int y, int w, int h);
    bool needsRender() const;

private:
    Display& m_display;
    Companion::CompanionSession& m_session;
    bool m_needsRender;
    bool m_lastConnected;

    void invalidate();
};
