//
// Created by miracleaigbogun on 3/21/26.
//

#include "calculator_app.h"
#include "core/expression.h"
#include "hal/host/display_sdl.h" // for DISPLAY_WIDTH, FONT_CHAR_HEIGHT
#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdio>


CalculatorApp::CalculatorApp(DisplaySDL& display, KeypadHost& keypad)
    : m_display(display)
    , m_keypad(keypad)
    , m_inputBuffer{}
    , m_resultBuffer{}
    , m_inputLen(0)
    , m_awaitingNewInput(false)
    , m_historyScroll(0)
{}

void CalculatorApp::init() {
    m_display.init();
    m_keypad.init();
}

void CalculatorApp::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Window close button
        if (event.type == SDL_QUIT) {
            m_display.setQuit();
        }

        // Escape key also closes the simulator
        if (event.type == SDL_KEYDOWN &&
            event.key.keysym.sym == SDLK_ESCAPE) {
            m_display.setQuit();
        }

        // Mouse wheel scrolls history
        if (event.type == SDL_MOUSEWHEEL) {
            if (event.wheel.y > 0) {
                m_historyScroll--;
            } else if (event.wheel.y < 0) {
                m_historyScroll++;
            }
        }

        // Arrow keys also scroll history
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_UP)   m_historyScroll--;
            if (event.key.keysym.sym == SDLK_DOWN)  m_historyScroll++;
        }

        // Pass the event to the keypad so it can update its internal key state
        m_keypad.handleEvent(event);
    }
}

void CalculatorApp::update() {
    Key pressed = m_keypad.getKey();
    if (pressed != Key::NONE) {
        processKey(pressed);
    }
    clampScroll();
}

void CalculatorApp::render() {
    m_display.clear(Display::BLACK);

    // History region background
    m_display.drawRect(0, HISTORY_TOP, DISPLAY_WIDTH, HISTORY_HEIGHT,
                       COLOR_HISTORY_BG);

    drawHistory();
    drawInputRow();
    drawButtonGrid();

    m_display.present();

    // ~60 FPS cap — keeps CPU usage low; on hardware this will be replaced by
    // a vsync or FreeRTOS task delay.
    SDL_Delay(16);
}

void CalculatorApp::processKey(Key pressed) {
    if (pressed == Key::CLEAR && m_inputLen > 0) {
        // Backspace: remove the last character
        m_inputBuffer[--m_inputLen] = '\0';
        m_resultBuffer[0] = '\0';

    } else if (pressed == Key::ENTER) {
        if (m_inputLen > 0) {
            // Evaluate the expression and store result
            ExprResult result = evaluate(m_inputBuffer);
            if (result.ok) {
                snprintf(m_resultBuffer, sizeof(m_resultBuffer),
                         "%.6g", result.value);
            } else {
                snprintf(m_resultBuffer, sizeof(m_resultBuffer),
                         "%s", result.error);
            }
            m_awaitingNewInput = true;
            pushHistory();
        }

    } else if (isPrintable(pressed) && m_inputLen < 127) {
        // Any printable key: if we just pressed ENTER, clear first
        if (m_awaitingNewInput) {
            m_inputLen = 0;
            m_inputBuffer[0]  = '\0';
            m_resultBuffer[0] = '\0';
            m_awaitingNewInput = false;
        }
        m_inputBuffer[m_inputLen++] = toChar(pressed);
        m_inputBuffer[m_inputLen]   = '\0';
    }
}

void CalculatorApp::pushHistory() {
    // Before appending, check if the scroll was already near the bottom so
    // we can auto-scroll to follow the new entry.
    int maxScrollBefore = std::max(0,
        static_cast<int>(m_history.size()) - VISIBLE_HISTORY_COUNT);
    bool wasNearBottom = (m_historyScroll >= std::max(0, maxScrollBefore - 1));

    m_history.push_back({m_inputBuffer, m_resultBuffer});

    if (wasNearBottom) {
        m_historyScroll = std::max(0,
            static_cast<int>(m_history.size()) - VISIBLE_HISTORY_COUNT);
    }

    // Clear the live input row
    m_inputLen        = 0;
    m_inputBuffer[0]  = '\0';
    m_resultBuffer[0] = '\0';
}

void CalculatorApp::clampScroll() {
    if (m_historyScroll < 0) m_historyScroll = 0;

    int maxScroll = std::max(0,
        static_cast<int>(m_history.size()) - VISIBLE_HISTORY_COUNT);
    if (m_historyScroll > maxScroll) m_historyScroll = maxScroll;
}

void CalculatorApp::drawHistory() {
    int startIndex = m_historyScroll;
    int endIndex   = std::min(startIndex + VISIBLE_HISTORY_COUNT,
                              static_cast<int>(m_history.size()));

    for (int i = startIndex; i < endIndex; i++) {
        int row = i - startIndex;
        int y   = HISTORY_TOP + row * ROW_HEIGHT;

        // Separator line above every entry except the very first visible row
        if (y > HISTORY_TOP) {
            m_display.drawRect(MARGIN, y - 2,
                               DISPLAY_WIDTH - (MARGIN * 2), 1,
                               COLOR_SEPARATOR);
        }

        // Left-aligned expression
        m_display.drawText(m_history[i].input.c_str(), MARGIN, y,
                           Display::WHITE);

        // Right-aligned result, 8px below the expression (mirrors TI-84 layout)
        int resultX = DISPLAY_WIDTH
                      - Display::textWidth(m_history[i].result.c_str())
                      - MARGIN;
        if (resultX < 0) resultX = 0;
        m_display.drawText(m_history[i].result.c_str(), resultX, y + 8,
                           Display::GREEN);
    }

    // Draw scrollbar only when history overflows the visible area
    int maxScroll = std::max(0,
        static_cast<int>(m_history.size()) - VISIBLE_HISTORY_COUNT);
    if (static_cast<int>(m_history.size()) > VISIBLE_HISTORY_COUNT) {
        drawScrollbar(maxScroll);
    }
}

void CalculatorApp::drawInputRow() {
    // The input row sits directly below the last visible history entry
    int visibleCount = std::min(VISIBLE_HISTORY_COUNT,static_cast<int>(m_history.size()) - m_historyScroll);
    int inputY = HISTORY_TOP + visibleCount * ROW_HEIGHT;

    // Separator above input row (if there's at least one history entry showing)
    if (inputY > HISTORY_TOP) {
        m_display.drawRect(MARGIN, inputY - 2,
                           DISPLAY_WIDTH - (MARGIN * 2), 1,
                           COLOR_SEPARATOR);
    }

    // Left-aligned expression being typed
    m_display.drawText(m_inputBuffer, MARGIN, inputY, Display::WHITE);

    // Right-aligned result (shown after ENTER while awaitingNewInput)
    int resultX = DISPLAY_WIDTH
                  - Display::textWidth(m_resultBuffer) - MARGIN;
    if (resultX < 0) resultX = 0;
    m_display.drawText(m_resultBuffer, resultX, inputY + 10, Display::GREEN);

    drawCursor(inputY);
}

void CalculatorApp::drawCursor(int inputY) {
    // Blink at ~1Hz: SDL_GetTicks() returns milliseconds since SDL init.
    // Dividing by 500 gives a value that flips every half-second; mod 2
    // alternates between 0 (hidden) and 1 (visible).
    bool showCursor = ((SDL_GetTicks() / 500) % 2) == 0;
    if (!showCursor) return;

    int cursorX = MARGIN + Display::textWidth(m_inputBuffer);
    if (cursorX < DISPLAY_WIDTH - MARGIN) {
        m_display.drawRect(cursorX, inputY, 2, FONT_CHAR_HEIGHT, Display::WHITE);
    }
}

void CalculatorApp::drawScrollbar(int maxScroll) {
    int scrollbarX = DISPLAY_WIDTH - 4;

    // Scale the thumb height proportionally to visible / total entries
    int scrollbarHeight = std::max(8,
        (HISTORY_HEIGHT * VISIBLE_HISTORY_COUNT)
            / static_cast<int>(m_history.size()));

    // Position the thumb within the track
    int scrollbarY = HISTORY_TOP
        + ((HISTORY_HEIGHT - scrollbarHeight) * m_historyScroll)
            / std::max(1, maxScroll);

    // Track (dark background strip)
    m_display.drawRect(scrollbarX, HISTORY_TOP, 3, HISTORY_HEIGHT,
                       COLOR_SCROLLBAR_BG);
    // Thumb (bright indicator of current position)
    m_display.drawRect(scrollbarX, scrollbarY, 3, scrollbarHeight,
                       Display::WHITE);
}

void CalculatorApp::drawButtonGrid() {
    // 4×4 placeholder button grid — will be replaced by the real 40+ key layout
    // once the physical keypad and its HAL implementation are ready.
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            int x = 15  + col * 73;
            int y = 105 + row * 33;
            m_display.drawRect(x, y, 63, 25, COLOR_BUTTON);
        }
    }
}
