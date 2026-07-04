//
// Created by Miracle Aigbogun on 3/21/26.
//

#pragma once

#include "history.h"
#include "app/settings/settings_state.h"
#include "hal/display.h"
#include "hal/keypad.h"
#include "math/math_typeset.h"
#include <array>
#include <cstdint>

static constexpr int MARGIN       = 5;
static constexpr int ROW_HEIGHT   = 20;
static constexpr int HISTORY_TOP  = 4;
static constexpr int HISTORY_BOTTOM_WITH_KEYPAD = 100;

static const uint16_t COLOR_HISTORY_BG   = Display::BLACK;
static const uint16_t COLOR_SEPARATOR    = Display::rgb(70, 70, 90);
static const uint16_t COLOR_SCROLLBAR_BG = Display::rgb(40, 40, 50);

// ── Button grid layout ───────────────────────────────────────────────────────
static constexpr int BTN_COLS      = 6;
static constexpr int BTN_ROWS      = 7;
static constexpr int BTN_MARGIN    = 4;   // gap between buttons in pixels
static constexpr int BTN_AREA_TOP  = 102; // y where the button grid starts
static constexpr int BTN_AREA_H    = DISPLAY_HEIGHT - BTN_AREA_TOP - 2;
static constexpr int BTN_W         = (DISPLAY_WIDTH  - BTN_MARGIN * (BTN_COLS + 1)) / BTN_COLS;
static constexpr int BTN_H         = (BTN_AREA_H     - BTN_MARGIN * (BTN_ROWS + 1)) / BTN_ROWS;

struct Button {
    const char* label;   // text drawn on the button face
    Key         key;     // key injected when clicked
};

struct CalculatorAppConfig {
    bool showOnScreenKeypad = false;
    int uiScale = 1;
    const SettingsState* settings = nullptr;
};

class CalculatorApp {
public:
    static constexpr int MAX_HISTORY = 64;

    CalculatorApp(Display& display, Keypad& keypad,
                  const CalculatorAppConfig& config = {});

    void init();
    void update();
    void handleKey(Key pressed);
    void handlePointerDown(int logicalX, int logicalY);
    void scrollHistory(int delta);
    void render();
    void renderContent(int contentY, int contentHeight);
    bool updateBlink(uint64_t nowMs);
    void requestRender();
    void requestInputRender();
    const char* input() const { return m_inputBuffer; }
    int cursorPos() const { return m_cursorPos; }
    int historySize() const;
    const HistoryEntry& historyAt(int index) const;

private:
    Display& m_display;
    Keypad& m_keypad;

    char m_inputBuffer[128];
    char m_resultBuffer[64];
    bool m_resultIsError;
    float m_lastAnswer;
    int  m_inputLen;
    int  m_cursorPos;          // NEW — index within m_inputBuffer
    int  m_inputViewportX;
    bool m_inputLayoutDirty;
    bool m_cachedInputMeasured;
    math_typeset::LayoutMetrics m_cachedInputMetrics;
    bool m_awaitingNewInput;
    int m_historyBottom;
    int m_historyHeight;
    int m_contentY;
    int m_contentHeight;

    std::array<HistoryEntry, MAX_HISTORY> m_history;
    int m_historyCount;
    int m_historyStart;
    int m_historyScroll;

    // Injected key from a mouse click — checked each update() tick
    Key m_injectedKey;
    CalculatorAppConfig m_config;
    bool m_needsRender;
    int m_lastBlinkPhase;
    bool m_cursorVisible;
    DirtyRegionList m_dirtyRegions;

    void processKey(Key pressed);
    void pushHistory();
    void clampScroll();

    void drawHistory();
    void drawInputRow();
    void markInputLayoutDirty();
    bool ensureInputLayout();
    void drawCursor(int originX, int baselineY, float expressionScale,
                    bool usedMathLayout);
    void drawScrollbar(int maxScroll, int viewportHeight);
    void drawButtonGrid();
    int currentUiScale() const;
    int currentPrecision() const;
    int contentBottom() const;
    int historyTop() const;
    void setContentArea(int contentY, int contentHeight);
    void updateContentMetrics();
    DisplayRect historyRect() const;
    DisplayRect inputRowRect() const;
    DisplayRect keypadRect() const;
    DisplayRect cursorDebugRect() const;
    DisplayRect cursorRect();
    void invalidateRect(DisplayRect rect);
    void invalidateFullScreen();

    // Maps grid position (col, row) to pixel rect top-left
    static int btnX(int col) {
        return BTN_MARGIN + col * (BTN_W + BTN_MARGIN);
    }
    int buttonY(int row) const;

    // Returns which button was clicked, or nullptr if click missed all buttons
    const Button* hitTest(int mouseX, int mouseY) const;

    static const Button BUTTONS[BTN_ROWS][BTN_COLS];
};
