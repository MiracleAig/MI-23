//
// Created by Miracle Aigbogun on 3/21/26.
//

#pragma once

#include "history.h"
#include "hal/host/display_sdl.h"
#include "hal/host/keypad_host.h"
#include <vector>

static constexpr int MARGIN = 5;
static constexpr int ROW_HEIGHT = 20;
static constexpr int HISTORY_TOP = 4;
static constexpr int HISTORY_BOTTOM = 100;
static constexpr int HISTORY_HEIGHT = HISTORY_BOTTOM - HISTORY_TOP;
static constexpr int VISIBLE_HISTORY_ROWS = HISTORY_HEIGHT / ROW_HEIGHT;

// VISIBLE_HISTORY_COUNT: how many completed history entries fit on screen at once
// we subtract one because that line is reserved for the live input line.
static constexpr int VISIBLE_HISTORY_COUNT = (VISIBLE_HISTORY_ROWS - 1 > 0) ? VISIBLE_HISTORY_ROWS - 1 : 0;

static const uint16_t COLOR_HISTORY_BG   = Display::rgb(10, 10, 18);
static const uint16_t COLOR_SEPARATOR    = Display::rgb(70, 70, 90);
static const uint16_t COLOR_BUTTON       = Display::rgb(60, 60, 80);
static const uint16_t COLOR_SCROLLBAR_BG = Display::rgb(40, 40, 50);

class CalculatorApp {
public:
    CalculatorApp(DisplaySDL& display, KeypadHost& keypad);

    // init() must be called once before the main loop.
    void init();

    void handleEvents(); // Poll SDL events: quit, scroll, keypad
    void update();       // Process key presses, update state
    void render();       // Draw Everything to the back-buffer, then present

private:
    DisplaySDL& m_display;
    KeypadHost& m_keypad;

    char m_inputBuffer[128];
    char m_resultBuffer[64];
    int m_inputLen;
    bool m_awaitingNewInput;

    std::vector<HistoryEntry> m_history;
    int m_historyScroll; // Index of the first visible history entry

    void processKey(Key pressed);
    void pushHistory();
    void clampScroll();

    void drawHistory();
    void drawInputRow();
    void drawCursor(int inputY);
    void drawScrollbar(int maxScroll);
    void drawButtonGrid();
};