//
// Created by Miracle Aigbogun on 3/21/26.
//

#pragma once

#include "app/history.h"
#include "platform/host/display_sdl.h"
#include "platform/host/keypad_host.h"
#include <vector>

static constexpr int MARGIN       = 5;
static constexpr int ROW_HEIGHT   = 20;
static constexpr int HISTORY_TOP  = 4;
static constexpr int HISTORY_BOTTOM = 100;
static constexpr int HISTORY_HEIGHT = HISTORY_BOTTOM - HISTORY_TOP;
static constexpr int VISIBLE_HISTORY_ROWS  = HISTORY_HEIGHT / ROW_HEIGHT;
static constexpr int VISIBLE_HISTORY_COUNT =
    (VISIBLE_HISTORY_ROWS - 1 > 0) ? VISIBLE_HISTORY_ROWS - 1 : 0;

static const uint16_t COLOR_HISTORY_BG   = Display::rgb(10, 10, 18);
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

class CalculatorApp {
public:
    CalculatorApp(DisplaySDL& display, KeypadHost& keypad);

    void init();
    void handleEvents();
    void update();
    void render();

private:
    DisplaySDL& m_display;
    KeypadHost& m_keypad;

    char m_inputBuffer[128];
    char m_resultBuffer[64];
    bool m_resultIsError;
    int  m_inputLen;
    int  m_cursorPos;          // NEW — index within m_inputBuffer
    bool m_awaitingNewInput;

    std::vector<HistoryEntry> m_history;
    int m_historyScroll;

    // Injected key from a mouse click — checked each update() tick
    Key m_injectedKey;

    void processKey(Key pressed);
    void pushHistory();
    void clampScroll();

    void drawHistory();
    void drawInputRow();
    void drawCursor(int inputY);
    void drawScrollbar(int maxScroll);
    void drawButtonGrid();

    // Maps grid position (col, row) to pixel rect top-left
    static int btnX(int col) {
        return BTN_MARGIN + col * (BTN_W + BTN_MARGIN);
    }
    static int btnY(int row) {
        return BTN_AREA_TOP + BTN_MARGIN + row * (BTN_H + BTN_MARGIN);
    }

    // Returns which button was clicked, or nullptr if click missed all buttons
    const Button* hitTest(int mouseX, int mouseY) const;

    static const Button BUTTONS[BTN_ROWS][BTN_COLS];
};
