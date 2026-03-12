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

    display.init();
    keypad.init();

    printf("Calculator Simulator Has Initialized. Press Escape To Quit. \n");

    // Main Loop, runs until user closes window, or power is lost to mcu
    while (!display.shouldQuit()) {
        // Handle input events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            display.pollEvents();
            keypad.handleEvent(event);
        }

        // Read key presses
        Key pressed = keypad.getKey();
        if (pressed != Key::NONE) {
            printf("Key Pressed: %d\n", static_cast<int>(pressed));
        }

        // Draw the frame
        display.clear(DisplaySDL::rgb(30,30,30)); // DARK GRAY

        display.drawRect(10, 10, 300, 80, Display::rgb(20, 20, 20));
        display.drawText("Calculator Simulator", 10, 10, Display::WHITE);
        display.drawText("Press Escape To Quit", 10, 30, Display::GREEN);
        display.drawText("Hello, world!", 10, 50, Display::RED);
        display.drawText("123456789.{}[]/+=-", 10, 70, Display::BLUE);

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