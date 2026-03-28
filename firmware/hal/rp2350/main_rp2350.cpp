//
// Created by Miracle Aigbogun on 3/22/26.
//

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hal/display_rp2350.h"
#include "hal/keypad_rp2350.h"
#include "core/expression.h"
#include <cstdio>
#include <cstring>

// ── Display layout constants ─────────────────────────────────────────────────
static constexpr int SCREEN_W        = 320;
static constexpr int SCREEN_H        = 240;
static constexpr int SCALE           = 2;
static constexpr int CHAR_W          = 6 * SCALE;
static constexpr int CHAR_H          = 8 * SCALE;
static constexpr int MARGIN          = 4;
static constexpr int ENTRY_H         = CHAR_H * 2 + 4;
static constexpr int STATUS_H        = 8 + MARGIN;
static constexpr int STATUS_Y        = SCREEN_H - STATUS_H;
static constexpr int HISTORY_AREA_H  = STATUS_Y - MARGIN;
static constexpr int MAX_VISIBLE     = (HISTORY_AREA_H - ENTRY_H) / ENTRY_H;
static constexpr int MAX_CHARS       = (SCREEN_W - MARGIN * 2) / CHAR_W;


static const uint16_t COLOR_SEPARATOR = Display::rgb(70, 70, 90);


static constexpr int MAX_HISTORY = 20;

struct HistoryEntry {
    char input[MAX_CHARS + 1];
    char result[32];
};

static HistoryEntry s_history[MAX_HISTORY];
static int s_historyCount = 0;

// ── Live input buffer ────────────────────────────────────────────────────────
static char s_buf[MAX_CHARS + 1] = {};
static int  s_len                = 0;

// ── Ring buffer index helper ─────────────────────────────────────────────────
static const HistoryEntry* getEntry(int index) {
    int total = s_historyCount < MAX_HISTORY ? s_historyCount : MAX_HISTORY;
    if (index < 0 || index >= total) return nullptr;
    int oldest = s_historyCount >= MAX_HISTORY
                     ? (s_historyCount % MAX_HISTORY) : 0;
    int slot = (oldest + index) % MAX_HISTORY;
    return &s_history[slot];
}

static void pushHistory(const char* input, const char* result) {
    int slot = s_historyCount % MAX_HISTORY;
    strncpy(s_history[slot].input,  input,  MAX_CHARS);
    strncpy(s_history[slot].result, result, 31);
    s_history[slot].input[MAX_CHARS] = '\0';
    s_history[slot].result[31]       = '\0';
    s_historyCount++;
}

// ── Layout helper: where does the live input line start? ─────────────────────
// Both redrawFull() and drawCursor() must agree on this value.
static int getInputY() {
    int total        = s_historyCount < MAX_HISTORY ? s_historyCount : MAX_HISTORY;
    int firstVisible = total - MAX_VISIBLE;
    if (firstVisible < 0) firstVisible = 0;
    return MARGIN + (total - firstVisible) * ENTRY_H;
}

// ── Separator line ───────────────────────────────────────────────────────────
static void drawSeparator(DisplayRP2350& display, int y) {
    display.drawRect(MARGIN, y, SCREEN_W - MARGIN * 2, 1, COLOR_SEPARATOR);
}

// ── Status bar ───────────────────────────────────────────────────────────────
// Drawn once and never changes, so it has its own function so redrawInputLine()
// doesn't need to touch it.
static void drawStatusBar(DisplayRP2350& display) {
    // Erase the status bar region first so old text doesn't bleed through
    display.drawRect(0, STATUS_Y, SCREEN_W, STATUS_H, Display::BLACK);
    display.drawText("ENT=eval CLR=del ESC=clr",
                     MARGIN, STATUS_Y, Display::GREEN, 1);
}

// ── Input line redraw ────────────────────────────────────────────────────────
// Called on every keypress except ENTER.
// Only touches the input line region — history and status bar are untouched.
static void redrawInputLine(DisplayRP2350& display) {
    int inputY = getInputY();

    // Erase the entire input line region (expression row + result row + gap)
    // by painting it black. This is the ONLY black rect we draw — no full clear.
    display.drawRect(0, inputY, SCREEN_W, ENTRY_H, Display::BLACK);

    // Redraw the current input buffer contents
    display.drawText(s_buf, MARGIN, inputY, Display::WHITE, SCALE);

    display.present();
}

// ── Full redraw ──────────────────────────────────────────────────────────────
// Called only when history changes (i.e. after ENTER) and on first boot.
// Does NOT call display.clear() — every region is explicitly overdrawn
// so the screen never flashes black.
static void redrawFull(DisplayRP2350& display) {
    int total        = s_historyCount < MAX_HISTORY ? s_historyCount : MAX_HISTORY;
    int firstVisible = total - MAX_VISIBLE;
    if (firstVisible < 0) firstVisible = 0;
    int visibleCount = total - firstVisible;

    // ── History region ───────────────────────────────────────────────────────
    // Paint the entire history area black first, then draw entries on top.
    // We do this as one big rect rather than pixel-by-pixel so it's fast,
    // and it only covers the history area — not the input line or status bar.
    int historyAreaBottom = MARGIN + visibleCount * ENTRY_H;
    display.drawRect(0, MARGIN, SCREEN_W, historyAreaBottom, Display::BLACK);

    for (int i = firstVisible; i < total; i++) {
        const HistoryEntry* e = getEntry(i);
        if (!e) continue;

        int row = i - firstVisible;
        int y   = MARGIN + row * ENTRY_H;

        if (row > 0) {
            drawSeparator(display, y - 2);
        }

        // Expression — left-aligned
        display.drawText(e->input, MARGIN, y, Display::WHITE, SCALE);

        // Result — right-aligned, one char-height below expression
        int resultW = static_cast<int>(strlen(e->result)) * CHAR_W;
        int resultX = SCREEN_W - resultW - MARGIN;
        if (resultX < MARGIN) resultX = MARGIN;
        display.drawText(e->result, resultX, y + CHAR_H + 2,
                         Display::GREEN, SCALE);
    }

    // ── Separator above input line ───────────────────────────────────────────
    int inputY = MARGIN + visibleCount * ENTRY_H;
    if (visibleCount > 0) {
        drawSeparator(display, inputY - 2);
    }

    // ── Input line ───────────────────────────────────────────────────────────
    // Erase and redraw, same as redrawInputLine() but inline here so we
    // don't call getInputY() twice (it would give the same answer, but
    // keeping it explicit makes the flow easier to follow).
    display.drawRect(0, inputY, SCREEN_W, ENTRY_H, Display::BLACK);
    display.drawText(s_buf, MARGIN, inputY, Display::WHITE, SCALE);


    drawStatusBar(display);

    display.present();
}

static void drawCursor(DisplayRP2350& display, bool visible) {
    int inputY  = getInputY();
    int cursorX = MARGIN + s_len * CHAR_W;
    if (cursorX >= SCREEN_W - MARGIN) return;

    uint16_t color = visible ? Display::WHITE : Display::BLACK;
    display.drawRect(cursorX, inputY, 2 * SCALE, CHAR_H, color);
    display.present();
}

int main() {
    stdio_init_all();

    DisplayRP2350 display;
    display.init();

    KeypadRP2350 keypad;
    keypad.init();

    redrawFull(display);

    bool     cursorVisible = true;
    uint32_t lastBlink     = to_ms_since_boot(get_absolute_time());
    drawCursor(display, cursorVisible);

    while (true) {
        Key k = keypad.getKey();

        if (k != Key::NONE) {

            drawCursor(display, false);

            if (k == Key::CLEAR) {
                if (s_len > 0) {
                    s_buf[--s_len] = '\0';
                    redrawInputLine(display);
                }
            }

            else if (k == Key::ESCAPE) {
                if (s_len > 0) {
                    s_len    = 0;
                    s_buf[0] = '\0';
                    redrawInputLine(display);
                }
            }

            else if (k == Key::ENTER) {
                if (s_len > 0) {
                    ExprResult result = evaluate(s_buf);

                    char numBuf[32] = {};
                    if (result.ok) {
                        snprintf(numBuf, sizeof(numBuf), "%.6g", result.value);
                    } else {
                        strncpy(numBuf, result.error, 31);
                        numBuf[31] = '\0';
                    }

                    pushHistory(s_buf, numBuf);

                    s_len    = 0;
                    s_buf[0] = '\0';

                    redrawFull(display);
                }
            }

            else if (isPrintable(k) && s_len < MAX_CHARS) {
                s_buf[s_len++] = toChar(k);
                s_buf[s_len]   = '\0';
                redrawInputLine(display);
            }

            // Reset blink timer and show cursor after redraw
            cursorVisible = true;
            lastBlink     = to_ms_since_boot(get_absolute_time());
            drawCursor(display, cursorVisible);
        }


        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - lastBlink >= 500) {
            lastBlink     = now;
            cursorVisible = !cursorVisible;
            drawCursor(display, cursorVisible);
        }

        sleep_ms(16);
    }
}