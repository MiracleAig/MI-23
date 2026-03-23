//
// Created by Miracle Aigbogun on 3/10/26.
//

#include "hal/host/display_sdl.h"
#include "hal/host/keypad_host.h"
#include "ui/calculator_app.h"
#include <cstdio>

int main() {
    printf("Calculator Simulator Is Starting...\n");

    DisplaySDL display;
    KeypadHost  keypad;

    CalculatorApp app(display, keypad);
    app.init();

    printf("Calculator Simulator Initialized. Press Escape To Quit.\n");

    while (!display.shouldQuit()) {
        app.handleEvents();
        app.update();
        app.render();
    }

    printf("Simulator Closed.\n");
    return 0;
}
