//
// Created by Miracle Aigbogun on 3/22/26.
//

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "display_rp2350.h"
int main() {
    stdio_init_all();

    DisplayRP2350 display;
    display.init();

    // Test each method from the Display interface
    display.clear(Display::BLACK);

    // Draw colored rectangles
    display.drawRect(10, 10, 100, 50, Display::RED);
    display.drawRect(10, 70, 100, 50, Display::GREEN);
    display.drawRect(10, 130, 100, 50, Display::BLUE);

    // Draw text
    display.drawText("MI-23", 120, 10, Display::WHITE);
    display.drawText("HAL OK", 120, 30, Display::WHITE);

    display.present();

    while (true) {
        tight_loop_contents();
    }
}