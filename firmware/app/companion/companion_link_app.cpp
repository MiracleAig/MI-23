#include "app/companion/companion_link_app.h"

#include "graphics/font.h"

namespace {

const Color COLOR_BG = Display::rgb(8, 10, 14);
const Color COLOR_TEXT = Display::WHITE;
const Color COLOR_MUTED = Display::rgb(150, 160, 172);
const Color COLOR_GOOD = Display::rgb(98, 220, 150);
const Color COLOR_WARN = Display::rgb(255, 180, 80);

} // namespace

CompanionLinkApp::CompanionLinkApp(Display& display, Companion::CompanionSession& session)
    : m_display(display)
    , m_session(session)
    , m_needsRender(true)
    , m_lastConnected(false) {}

void CompanionLinkApp::enter(uint64_t nowMs) {
    m_session.enter(nowMs);
    m_lastConnected = m_session.isConnected();
    invalidate();
}

void CompanionLinkApp::leave() {
    m_session.leave();
}

bool CompanionLinkApp::handleKey(Key key) {
    return key == Key::CLEAR;
}

void CompanionLinkApp::tick(uint64_t nowMs) {
    m_session.poll(nowMs);
    const bool connected = m_session.isConnected();
    if (connected != m_lastConnected) {
        m_lastConnected = connected;
        invalidate();
    }
}

void CompanionLinkApp::renderContent(int x, int y, int w, int h) {
    if (!m_needsRender) {
        return;
    }

    m_display.fillRect(x, y, w, h, COLOR_BG);
    m_display.drawText("Companion Link", x + 8, y + 8, COLOR_TEXT);

    const bool connected = m_session.isConnected();
    const char* status = connected ? "Serial port open" : "Waiting for serial";
    m_display.drawText(status, x + 12, y + 44, connected ? COLOR_GOOD : COLOR_WARN);
    m_display.drawText("Waiting for Miracle's Instruments Companion...", x + 12, y + 66, COLOR_MUTED);
    m_display.drawText("CLR back", x + 8, y + h - FONT_CHAR_HEIGHT - 4, COLOR_MUTED);
    m_needsRender = false;
}

bool CompanionLinkApp::needsRender() const {
    return m_needsRender;
}

void CompanionLinkApp::invalidate() {
    m_needsRender = true;
}
