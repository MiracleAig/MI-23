#include "app/boot/boot_manager.h"

#include "graphics/font.h"

#include <cstdio>
#include <cstring>

namespace {

const Color COLOR_BG = Display::rgb(8, 10, 14);
const Color COLOR_PANEL = Display::rgb(18, 24, 32);
const Color COLOR_ACCENT = Display::rgb(36, 86, 132);
const Color COLOR_TEXT = Display::WHITE;
const Color COLOR_MUTED = Display::rgb(150, 160, 172);
const Color COLOR_WARN = Display::rgb(255, 180, 80);
const Color COLOR_ERROR = Display::rgb(220, 72, 72);
const Color COLOR_PROGRESS_BG = Display::rgb(30, 34, 42);
const Color COLOR_PROGRESS_FILL = Display::rgb(90, 190, 255);

void copyText(char* destination, std::size_t size, const char* text) {
    if (!destination || size == 0) {
        return;
    }

    if (!text) {
        destination[0] = '\0';
        return;
    }

    std::snprintf(destination, size, "%s", text);
}

void drawCenteredText(Display& display, const char* text, int y, Color color) {
    const int x = (DISPLAY_WIDTH - Display::textWidth(text)) / 2;
    display.drawText(text, x, y, color);
}

void drawLogo(Display& display) {
    const int boxX = 128;
    const int boxY = 28;
    const int boxW = 192;
    const int boxH = 58;


}

} // namespace

BootManager::BootManager(Display& display,
                         SettingsState& settings,
                         StartupBackend& backend)
    : m_display(display)
    , m_settings(settings)
    , m_backend(backend)
    , m_state(State::Idle)
    , m_stage(Stage::DisplayInit)
    , m_completedStages(0)
    , m_bootSucceeded(false)
    , m_continueAllowed(false)
    , m_needsRender(false)
    , m_status{}
    , m_detail{}
{}

void BootManager::begin() {
    m_state = State::AnnouncingStage;
    m_stage = Stage::DisplayInit;
    m_completedStages = 0;
    m_bootSucceeded = false;
    m_continueAllowed = false;
    setStatus(stageLabel(m_stage));
    setDetail(nullptr);
    m_needsRender = true;
}

void BootManager::tick() {
    if (m_state == State::Idle || m_state == State::AwaitingContinue || m_state == State::Finished) {
        return;
    }

    if (m_state == State::AnnouncingStage) {
        m_state = State::RunningStage;
        return;
    }

    StartupCheckResult result{};
    switch (m_stage) {
        case Stage::DisplayInit:
            std::printf("[boot] %s\n", stageLabel(m_stage));
            result.ok = true;
            break;
        case Stage::InputInit:
            std::printf("[boot] %s\n", stageLabel(m_stage));
            result = m_backend.initializeInput();
            break;
        case Stage::LoadSettings:
            std::printf("[boot] %s\n", stageLabel(m_stage));
            result = m_backend.loadSettings(m_settings);
            break;
        case Stage::CheckStorage:
            std::printf("[boot] %s\n", stageLabel(m_stage));
            result = m_backend.checkStorage();
            break;
        case Stage::VerifyResources:
            std::printf("[boot] %s\n", stageLabel(m_stage));
            result = m_backend.verifyResources(m_settings);
            break;
        case Stage::StartRuntime:
            std::printf("[boot] %s\n", stageLabel(m_stage));
            result = m_backend.startRuntime(m_settings);
            break;
        case Stage::Count:
            break;
    }

    if (!result.ok) {
        fail(result);
        return;
    }

    if (result.repaired) {
        setDetail(result.detail);
    } else {
        setDetail(nullptr);
    }

    m_completedStages++;
    advanceStage();
}

void BootManager::handleKey(Key key) {
    if (m_state != State::AwaitingContinue) {
        return;
    }

    if (!m_continueAllowed) {
        return;
    }

    if (key == Key::ENTER || key == Key::CLEAR || key == Key::HOME) {
        finish(false);
    }
}

void BootManager::render() {
    if (!m_needsRender) {
        return;
    }

    if (m_state == State::AwaitingContinue) {
        renderError();
    } else {
        renderNormal();
    }

    m_display.present();
    m_needsRender = false;
}

bool BootManager::needsRender() const {
    return m_needsRender;
}

bool BootManager::isFinished() const {
    return m_state == State::Finished;
}

bool BootManager::bootSucceeded() const {
    return m_bootSucceeded;
}

bool BootManager::canContinue() const {
    return m_state == State::AwaitingContinue && m_continueAllowed;
}

bool BootManager::inputReady() const {
    return m_completedStages >= 2;
}

void BootManager::setStatus(const char* text) {
    copyText(m_status, sizeof(m_status), text);
}

void BootManager::setDetail(const char* text) {
    copyText(m_detail, sizeof(m_detail), text);
}

void BootManager::appendDetail(const char* text) {
    if (!text || text[0] == '\0') {
        return;
    }

    if (m_detail[0] == '\0') {
        setDetail(text);
        return;
    }

    const std::size_t length = std::strlen(m_detail);
    if (length + 2 >= sizeof(m_detail)) {
        return;
    }

    std::snprintf(m_detail + length, sizeof(m_detail) - length, " %s", text);
}

void BootManager::advanceStage() {
    const int nextStage = static_cast<int>(m_stage) + 1;
    if (nextStage >= totalStages()) {
        finish(true);
        return;
    }

    m_stage = static_cast<Stage>(nextStage);
    setStatus(stageLabel(m_stage));
    m_state = State::AnnouncingStage;
    m_needsRender = true;
}

void BootManager::finish(bool success) {
    m_bootSucceeded = success;
    m_state = State::Finished;
    setStatus(success ? "Boot complete" : "Continuing without storage");
    if (!success && m_detail[0] == '\0') {
        setDetail("Runtime is available, persistence is disabled.");
    }
    m_needsRender = true;
}

void BootManager::fail(const StartupCheckResult& result) {
    m_continueAllowed = result.continueAllowed;
    setStatus("Startup issue detected");
    setDetail(result.detail ? result.detail : "An unrecoverable startup check failed.");
    appendDetail(m_continueAllowed ? "Press ENT to continue." : "Restart required.");
    m_state = State::AwaitingContinue;
    m_needsRender = true;
}

int BootManager::totalStages() const {
    return static_cast<int>(Stage::Count);
}

int BootManager::progressPercent() const {
    if (totalStages() == 0) {
        return 0;
    }

    return (m_completedStages * 100) / totalStages();
}

const char* BootManager::stageLabel(Stage stage) const {
    switch (stage) {
        case Stage::DisplayInit: return "Initializing display...";
        case Stage::InputInit: return "Initializing input...";
        case Stage::LoadSettings: return "Loading settings...";
        case Stage::CheckStorage: return "Checking storage...";
        case Stage::VerifyResources: return "Verifying filesystem...";
        case Stage::StartRuntime: return "Starting Axiom...";
        case Stage::Count: return "";
        default: return "";
    }
}

void BootManager::renderNormal() {
    m_display.clear(COLOR_BG);
    drawLogo(m_display);

    drawCenteredText(m_display, m_backend.platformName(), 112, COLOR_MUTED);
    drawCenteredText(m_display, m_backend.firmwareVersion(), 126, COLOR_MUTED);

    m_display.drawText(m_status, 28, 164, COLOR_TEXT);
    if (m_detail[0] != '\0') {
        m_display.drawText(m_detail, 28, 178, COLOR_WARN);
    }

    const int barX = 28;
    const int barY = 198;
    const int barW = DISPLAY_WIDTH - barX * 2;
    const int barH = 14;
    m_display.fillRect(barX, barY, barW, barH, COLOR_PROGRESS_BG);
    m_display.drawRect(barX, barY, barW, barH, COLOR_ACCENT);
    const int fillW = (barW - 2) * progressPercent() / 100;
    if (fillW > 0) {
        m_display.fillRect(barX + 1, barY + 1, fillW, barH - 2, COLOR_PROGRESS_FILL);
    }

    char percent[8] = {};
    std::snprintf(percent, sizeof(percent), "%d%%", progressPercent());
    drawCenteredText(m_display, percent, 217, COLOR_MUTED);
}

void BootManager::renderError() {
    m_display.clear(COLOR_BG);
    drawLogo(m_display);
    drawCenteredText(m_display, "Startup Error", 108, COLOR_ERROR);
    m_display.drawText(m_status, 24, 154, COLOR_TEXT);
    m_display.drawText(m_detail, 24, 170, COLOR_WARN);
    if (m_continueAllowed) {
        m_display.drawText("ENT/CLR continue  HOME continue", 24, 206, COLOR_MUTED);
    } else {
        m_display.drawText("Power cycle required", 24, 206, COLOR_MUTED);
    }
}
