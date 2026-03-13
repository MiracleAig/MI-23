//
// Created by Miracle Aigbogun on 3/10/26.
//

#include "hal/host/display_sdl.h"
#include "hal/host/keypad_host.h"
#include "core/expression.h"

#include <cstdio>
#include <SDL2/SDL.h>

int main() {
    printf("Calculator Simulator Is Starting...\n");

    DisplaySDL display;
    KeypadHost keypad;

    char inputBuffer[32] = {0};
    char resultBuffer[32] = {0};
    int inputLen = 0;
    bool awaitingNewInput = false;


    display.init();
    keypad.init();

    printf("Calculator Simulator Has Initialized. Press Escape To Quit. \n");

    // Main Loop, runs until user closes window, or power is lost to mcu
    while (!display.shouldQuit()) {
        // Handle input events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {display.setQuit();}
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) display.setQuit();
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
                }
            } else if (isPrintable(pressed) && inputLen < 31) {
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


        // Background for the expression area
        display.drawRect(0, 0, DISPLAY_WIDTH, 30, Display::BLACK);


        int margin = 5;
        int resultX = DISPLAY_WIDTH - Display::textWidth(resultBuffer) - margin;
        if (resultX < 0) resultX = 0;

        display.drawText(inputBuffer,  margin,  8, Display::WHITE);
        display.drawText(resultBuffer, resultX, 18, Display::GREEN);

        bool showCursor = ((SDL_GetTicks() / 500) % 2) == 0;
        if (!awaitingNewInput && showCursor) {
            int cursorX = margin + Display::textWidth(inputBuffer);
            int cursorY = 8;
            int cursorWidth = 2;
            int cursorHeight = FONT_CHAR_HEIGHT;

            if (cursorX < DISPLAY_WIDTH - margin) {
                display.drawRect(cursorX, cursorY, cursorWidth, cursorHeight, Display::WHITE);
            }
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

