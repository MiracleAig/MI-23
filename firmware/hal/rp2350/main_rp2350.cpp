//
// Created by Miracle Aigbogun on 3/22/26.
//

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "display_rp2350.h"
#include "hal/rp2350/keypad_rp2350.h"
#include "core/expression.h"
#include <cstdio>


static constexpr int SCREEN_W  = 320;
static constexpr int SCREEN_H  = 240;
static constexpr int CHAR_W    = 6;
static constexpr int CHAR_H    = 8;
static constexpr int MARGIN    = 4;
static constexpr int INPUT_Y   = MARGIN;
static constexpr int STATUS_Y  = SCREEN_H - CHAR_H - MARGIN;
static constexpr int MAX_CHARS = (SCREEN_W - MARGIN * 2) / CHAR_W;

static char s_buf[MAX_CHARS + 1] = {};
static int  s_len                = 0;

static void redrawFull(DisplayRP2350& display) {
    display.clear(Display::BLACK);
    display.drawText(s_buf, MARGIN, INPUT_Y, Display::WHITE);
    display.drawText("ENTER=eval  CLEAR=del  ESC=clr",
                     MARGIN, STATUS_Y, Display::GREEN);
    display.present();
}


static void drawCursor(DisplayRP2350& display, bool visible) {
    int cursorX = MARGIN + s_len * CHAR_W;
    if (cursorX >= SCREEN_W - MARGIN) return;

    uint16_t color = visible ? Display::WHITE : Display::BLACK;
    display.drawRect(cursorX, INPUT_Y, 2, CHAR_H, color);
    display.present();
}

int main() {
    stdio_init_all();

    DisplayRP2350 display;
    display.init();

    KeypadRP2350 keypad;
    keypad.init();

    // Draw the screen once at startup — never clear it unless something changes
    redrawFull(display);

    bool     cursorVisible = true;
    uint32_t lastBlink     = to_ms_since_boot(get_absolute_time());
    drawCursor(display, cursorVisible);

    while (true) {
        Key k = keypad.getKey();
        bool bufferChanged = false;

        if (k != Key::NONE) {
            if (k == Key::CLEAR) {
                if (s_len > 0) {
                    s_buf[--s_len] = '\0';
                    bufferChanged  = true;
                }
            }
            else if (k == Key::ESCAPE) {
                if (s_len > 0) {
                    s_len    = 0;
                    s_buf[0] = '\0';
                    bufferChanged = true;
                }
            }
            else if (k == Key::ENTER) {
                if (s_len > 0) {
                    ExprResult result = evaluate(s_buf);

                    // Format result into a temporary buffer
                    char numBuf[32] = {};
                    if (result.ok) {
                        snprintf(numBuf, sizeof(numBuf), "%.6g", result.value);
                    }
                    const char* resultStr = result.ok ? numBuf : result.error;

                    // Show expression on top line, result right-aligned below it —
                    // same layout as the SDL simulator
                    display.clear(Display::BLACK);
                    display.drawText(s_buf, MARGIN, INPUT_Y, Display::WHITE);

                    int textW   = Display::textWidth(resultStr);
                    int resultX = SCREEN_W - textW - MARGIN;
                    if (resultX < MARGIN) resultX = MARGIN;
                    display.drawText(resultStr, resultX, INPUT_Y + CHAR_H + 2,
                                     result.ok ? Display::GREEN : Display::RED);

                    display.drawText("ENTER=eval  CLEAR=del  ESC=clr",
                                     MARGIN, STATUS_Y, Display::GREEN);
                    display.present();

                    // Hold result on screen, then clear for next input
                    sleep_ms(2000);

                    s_len    = 0;
                    s_buf[0] = '\0';
                    bufferChanged = true;
                }
            }
            else if (isPrintable(k) && s_len < MAX_CHARS) {
                s_buf[s_len++] = toChar(k);
                s_buf[s_len]   = '\0';
                bufferChanged  = true;
            }
        }

        if (bufferChanged) {
            // Full repaint, then immediately show cursor so it feels responsive
            redrawFull(display);
            cursorVisible = true;
            lastBlink     = to_ms_since_boot(get_absolute_time());
            drawCursor(display, cursorVisible);
        }

        // Cursor blink — only sends pixels to the display when state flips
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - lastBlink >= 500) {
            lastBlink     = now;
            cursorVisible = !cursorVisible;
            drawCursor(display, cursorVisible);
        }

        sleep_ms(16);
    }
}