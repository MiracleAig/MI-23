//
// Created by miracleaigbogun on 3/10/26.
//

#include "hal/host/display_sdl.h"
#include "hal/host/keypad_host.h"

#include <cstdio>

int main() {
    printf("Calculator Simulator Is Starting...\n");

    DisplaySDL display;
    KeypadHost keypad;

    char inputBuffer[32] = {0};
    int inputLen = 0;

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
                inputBuffer[--inputLen] = '\0';  // delete last character
            } else if (pressed == Key::ENTER) {
                printf("Expression: %s\n", inputBuffer);  // placeholder for now
            } else if (isPrintable(pressed) && inputLen < 31) {
                inputBuffer[inputLen++] = static_cast<char>(pressed);
                inputBuffer[inputLen]   = '\0';  // always keep null terminated
            }
        }

        // Draw the frame
        display.clear(DisplaySDL::rgb(30,30,30)); // DARK GRAY

        display.drawRect(10, 10, 300, 80, Display::BLACK);
        display.drawText("Calculator Simulator", 10, 10, Display::WHITE);
        display.drawText("Press Escape To Quit", 10, 30, Display::GREEN);
        display.drawText("Hello, world!", 10, 50, Display::RED);
        display.drawText(inputBuffer, 10, 70, Display::WHITE);

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

