//
// Created by Miracle Aigbogun on 3/10/26.
//

#include "hal/host/display_sdl.h"
#include "hal/host/keypad_host.h"
#include "core/expression.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include <SDL2/SDL.h>

struct HistoryEntry {
    std::string input;
    std::string result;
};

int main() {
    printf("Calculator Simulator Is Starting...\n");

    DisplaySDL display;
    KeypadHost keypad;

    char inputBuffer[128] = {0};
    char resultBuffer[64] = {0};
    int inputLen = 0;
    bool awaitingNewInput = false;

    std::vector<HistoryEntry> history;
    int historyScroll = 0;

    display.init();
    keypad.init();

    printf("Calculator Simulator Has Initialized. Press Escape To Quit. \n");
    
    const int MARGIN = 5;
    const int ROW_HEIGHT = 20;
    const int HISTORY_TOP = 4;
    const int HISTORY_BOTTOM = 100;
    const int HISTORY_HEIGHT = HISTORY_BOTTOM - HISTORY_TOP;
    const int VISIBLE_HISTORY_ROWS = HISTORY_HEIGHT / ROW_HEIGHT;
    const int VISIBLE_HISTORY_COUNT = std::max(0, VISIBLE_HISTORY_ROWS - 1);
    const uint16_t SEPARATOR_COLOR = Display::rgb(70, 70, 90);

    // Main Loop, runs until user closes window, or power is lost to mcu
    while (!display.shouldQuit()) {
        // Handle input events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {display.setQuit();}
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) display.setQuit();

            if (event.type == SDL_MOUSEWHEEL) {
                if (event.wheel.y > 0 && historyScroll > 0) {
                    historyScroll--;
                } else if (event.wheel.y < 0 &&
                           historyScroll + VISIBLE_HISTORY_COUNT < static_cast<int>(history.size())) {
                    historyScroll++;
                }
            }

            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_UP && historyScroll > 0) {
                    historyScroll--;
                } else if (event.key.keysym.sym == SDLK_DOWN &&
                           historyScroll + VISIBLE_HISTORY_COUNT < static_cast<int>(history.size())) {
                    historyScroll++;
                }
            }

            keypad.handleEvent(event);
        }

        // Read key presses
        Key pressed = keypad.getKey();
        if (pressed != Key::NONE) {
            if (pressed == Key::CLEAR && inputLen > 0) {
                inputBuffer[--inputLen] = '\0';
                resultBuffer[0] = '\0';// delete last character
            } else if (pressed == Key::ENTER) {
                ExprResult result = evaluate(inputBuffer);
                if (result.ok) {
                    snprintf(resultBuffer, sizeof(resultBuffer), "%.6g", result.value);
                    awaitingNewInput = true;
                } else {
                    snprintf(resultBuffer, sizeof(resultBuffer), "%s", result.error);
                    awaitingNewInput = true;
                }

                if (inputLen > 0) {
                    int maxHistoryScrollBeforeAppend = std::max(0, static_cast<int>(history.size()) - VISIBLE_HISTORY_COUNT);
                    bool wasNearBottom = historyScroll >= std::max(0, maxHistoryScrollBeforeAppend - 1);

                    history.push_back({inputBuffer, resultBuffer});

                    if (wasNearBottom) {
                        historyScroll = std::max(0, static_cast<int>(history.size()) - VISIBLE_HISTORY_COUNT);
                    }

                    inputLen = 0;
                    inputBuffer[0] = '\0';
                    resultBuffer[0] = '\0';
                }
            } else if (isPrintable(pressed) && inputLen < 127) {
                if (awaitingNewInput) {
                    inputLen = 0;
                    inputBuffer[0] = '\0';
                    resultBuffer[0] = '\0';
                    awaitingNewInput = false;
                }
                inputBuffer[inputLen++] = toChar(pressed);
                inputBuffer[inputLen] = '\0';
            }
        }

        if (historyScroll < 0) {
            historyScroll = 0;
        }
        int maxHistoryScroll = std::max(0, static_cast<int>(history.size()) - VISIBLE_HISTORY_COUNT);
        if (historyScroll > maxHistoryScroll) {
            historyScroll = maxHistoryScroll;
        }

        display.clear(Display::BLACK);

        display.drawRect(0, HISTORY_TOP, DISPLAY_WIDTH, HISTORY_HEIGHT, Display::rgb(10, 10, 18));

        int startIndex = historyScroll;
        int endIndex = std::min(startIndex + VISIBLE_HISTORY_COUNT, static_cast<int>(history.size()));
        for (int i = startIndex; i < endIndex; i++) {
            int row = i - startIndex;
            int y = HISTORY_TOP + row * ROW_HEIGHT;

            if (y > HISTORY_TOP) {
                display.drawRect(MARGIN, y - 2, DISPLAY_WIDTH - (MARGIN * 2), 1, SEPARATOR_COLOR);
            }

            display.drawText(history[i].input.c_str(), MARGIN, y, Display::WHITE);

            int historyResultX = DISPLAY_WIDTH - Display::textWidth(history[i].result.c_str()) - MARGIN;
            if (historyResultX < 0) historyResultX = 0;
            display.drawText(history[i].result.c_str(), historyResultX, y + 8, Display::GREEN);
        }

        int inputRow = endIndex - startIndex;
        int inputY = HISTORY_TOP + inputRow * ROW_HEIGHT;
        int resultX = DISPLAY_WIDTH - Display::textWidth(resultBuffer) - MARGIN;
        if (resultX < 0) resultX = 0;

        if (inputY > HISTORY_TOP) {
            display.drawRect(MARGIN, inputY - 2, DISPLAY_WIDTH - (MARGIN * 2), 1, SEPARATOR_COLOR);
        }

        display.drawText(inputBuffer, MARGIN, inputY, Display::WHITE);
        display.drawText(resultBuffer, resultX, inputY + 10, Display::GREEN);

        bool showCursor = ((SDL_GetTicks() / 500) % 2) == 0;
        if (showCursor) {
            int cursorX = MARGIN + Display::textWidth(inputBuffer);
            int cursorY = inputY;
            int cursorWidth = 2;
            int cursorHeight = FONT_CHAR_HEIGHT;

            if (cursorX < DISPLAY_WIDTH - MARGIN) {
                display.drawRect(cursorX, cursorY, cursorWidth, cursorHeight, Display::WHITE);
            }
        }

        if (static_cast<int>(history.size()) > VISIBLE_HISTORY_COUNT) {
            int scrollbarX = DISPLAY_WIDTH - 4;
            int scrollbarHeight = std::max(8, (HISTORY_HEIGHT * VISIBLE_HISTORY_COUNT) / static_cast<int>(history.size()));
            int scrollbarY = HISTORY_TOP + ((HISTORY_HEIGHT - scrollbarHeight) * historyScroll) / std::max(1, maxHistoryScroll);

            display.drawRect(scrollbarX, HISTORY_TOP, 3, HISTORY_HEIGHT, Display::rgb(40, 40, 50));
            display.drawRect(scrollbarX, scrollbarY, 3, scrollbarHeight, Display::WHITE);
        }

        // Draw placeholder buttons 4x4 grid
        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 4; col++) {
                int x = 15 + col * 73;
                int y = 105 + row * 33;
                display.drawRect(x, y, 63, 25, Display::rgb(60, 60, 80));
            }
        }

        display.present();

        // Small delay to avoid using 100% CPU
        // ~60FPS = one frame every ~16ms
        SDL_Delay(16);
    }

    printf("Simulator Closed.\n");
    return 0;
}
