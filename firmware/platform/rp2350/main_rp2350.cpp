//
// Created by Miracle Aigbogun on 3/22/26.
//

#include "pico/stdlib.h"
#include "display_rp2350.h"
#include "platform/rp2350/keypad_rp2350.h"
#include "keypad_rp2350_2.h"
#include "math/expression.h"
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
static constexpr char SQRT_INSERT_TEXT[] = "sqrt()";
static constexpr char PAREN_PAIR_TEXT[]  = "()";

// ── History storage ──────────────────────────────────────────────────────────
static constexpr int MAX_HISTORY = 20;

struct HistoryEntry {
    char input[MAX_CHARS + 1];
    char result[32];
    bool isError;
};

static HistoryEntry s_history[MAX_HISTORY];
static int s_historyCount = 0;

// ── Live input buffer ────────────────────────────────────────────────────────
static char s_buf[MAX_CHARS + 1] = {};
static int  s_len                = 0;
static int  s_cursor             = 0;  

// ── Ring buffer helpers ──────────────────────────────────────────────────────
static const HistoryEntry* getEntry(int index) {
    int total = s_historyCount < MAX_HISTORY ? s_historyCount : MAX_HISTORY;
    if (index < 0 || index >= total) return nullptr;
    int oldest = s_historyCount >= MAX_HISTORY
                     ? (s_historyCount % MAX_HISTORY) : 0;
    int slot = (oldest + index) % MAX_HISTORY;
    return &s_history[slot];
}

static void pushHistory(const char* input, const char* result, bool isError) {
    int slot = s_historyCount % MAX_HISTORY;
    strncpy(s_history[slot].input,  input,  MAX_CHARS);
    strncpy(s_history[slot].result, result, 31);
    s_history[slot].input[MAX_CHARS] = '\0';
    s_history[slot].result[31]       = '\0';
    s_history[slot].isError          = isError;
    s_historyCount++;
}

// ── Layout helper ────────────────────────────────────────────────────────────
static int getInputY() {
    int total        = s_historyCount < MAX_HISTORY ? s_historyCount : MAX_HISTORY;
    int firstVisible = total - MAX_VISIBLE;
    if (firstVisible < 0) firstVisible = 0;
    return MARGIN + (total - firstVisible) * ENTRY_H;
}

static bool insertTextAtCursor(const char* text) {
    const int insertLen = static_cast<int>(strlen(text));
    if (s_len + insertLen > MAX_CHARS) {
        return false;
    }

    memmove(&s_buf[s_cursor + insertLen],
            &s_buf[s_cursor],
            s_len - s_cursor + 1);
    memcpy(&s_buf[s_cursor], text, insertLen);
    s_len += insertLen;
    s_cursor += insertLen;
    return true;
}

static bool skipExistingCloseParen() {
    if (s_cursor < s_len && s_buf[s_cursor] == ')') {
        s_cursor++;
        return true;
    }
    return false;
}

// ── Drawing helpers ──────────────────────────────────────────────────────────
static void drawSeparator(DisplayRP2350& display, int y) {
    display.drawRect(MARGIN, y, SCREEN_W - MARGIN * 2, 1, COLOR_SEPARATOR);
}

static void drawStatusBar(DisplayRP2350& display) {
    display.drawRect(0, STATUS_Y, SCREEN_W, STATUS_H, Display::BLACK);
    display.drawText("ENT=eval CLR=del ESC=clr",
                     MARGIN, STATUS_Y, Display::GREEN, 1);
}

static void redrawInputLine(DisplayRP2350& display) {
    int inputY = getInputY();
    display.drawRect(0, inputY, SCREEN_W, ENTRY_H, Display::BLACK);
    display.drawText(s_buf, MARGIN, inputY, Display::WHITE, SCALE);
    display.present();
}

static void redrawFull(DisplayRP2350& display) {
    int total        = s_historyCount < MAX_HISTORY ? s_historyCount : MAX_HISTORY;
    int firstVisible = total - MAX_VISIBLE;
    if (firstVisible < 0) firstVisible = 0;
    int visibleCount = total - firstVisible;

    int historyAreaBottom = MARGIN + visibleCount * ENTRY_H;
    display.drawRect(0, MARGIN, SCREEN_W, historyAreaBottom, Display::BLACK);

    for (int i = firstVisible; i < total; i++) {
        const HistoryEntry* e = getEntry(i);
        if (!e) continue;

        int row = i - firstVisible;
        int y   = MARGIN + row * ENTRY_H;

        if (row > 0) drawSeparator(display, y - 2);

        display.drawText(e->input, MARGIN, y, Display::WHITE, SCALE);

        int resultW = static_cast<int>(strlen(e->result)) * CHAR_W;
        int resultX = SCREEN_W - resultW - MARGIN;
        if (resultX < MARGIN) resultX = MARGIN;
        display.drawText(e->result, resultX, y + CHAR_H + 2,
                         e->isError ? Display::RED : Display::GREEN, SCALE);
    }

    int inputY = MARGIN + visibleCount * ENTRY_H;
    if (visibleCount > 0) drawSeparator(display, inputY - 2);

    display.drawRect(0, inputY, SCREEN_W, ENTRY_H, Display::BLACK);
    display.drawText(s_buf, MARGIN, inputY, Display::WHITE, SCALE);

    drawStatusBar(display);
    display.present();
}

// ── Cursor ───────────────────────────────────────────────────────────────────
// Now uses s_cursor instead of s_len so it sits at the cursor position
static void drawCursor(DisplayRP2350& display, bool visible) {
    int inputY  = getInputY();
    int cursorX = MARGIN + s_cursor * CHAR_W;   // ← was s_len
    if (cursorX >= SCREEN_W - MARGIN) return;

    uint16_t color = visible ? Display::WHITE : Display::BLACK;
    display.drawRect(cursorX, inputY, 2 * SCALE, CHAR_H, color);
    display.present();
}

// ── Entry point ──────────────────────────────────────────────────────────────
int main() {
    stdio_init_all();

    DisplayRP2350  display;
    display.init();

    KeypadRP2350   keypad1;
    KeypadRP2350_2 keypad2;   
    keypad1.init();
    keypad2.init();            

    redrawFull(display);

    bool     cursorVisible = true;
    uint32_t lastBlink     = to_ms_since_boot(get_absolute_time());
    drawCursor(display, cursorVisible);

    while (true) {
        // Poll both keypads — keypad2 only checked if keypad1 returns nothing
        Key k = keypad1.getKey();
        if (k == Key::NONE) k = keypad2.getKey();   

        if (k != Key::NONE) {
            drawCursor(display, false);

            if (k == Key::CURSOR_LEFT) {             
                if (s_cursor > 0) {
                    s_cursor--;
                    redrawInputLine(display);
                }
            }

            else if (k == Key::CURSOR_RIGHT) {       
                if (s_cursor < s_len) {
                    s_cursor++;
                    redrawInputLine(display);
                }
            }

            else if (k == Key::CLEAR) {
                // Backspace at cursor position — remove character to the left
                if (s_cursor > 0) {
                    memmove(&s_buf[s_cursor - 1],
                            &s_buf[s_cursor],
                            s_len - s_cursor + 1);
                    s_len--;
                    s_cursor--;
                    redrawInputLine(display);
                }
            }

            else if (k == Key::ESCAPE) {
                if (s_len > 0) {
                    s_len    = 0;
                    s_cursor = 0;                    
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

                    pushHistory(s_buf, numBuf, !result.ok);

                    s_len    = 0;
                    s_cursor = 0;                    
                    s_buf[0] = '\0';

                    redrawFull(display);
                }
            }

            else if (k == Key::SQRT) {
                if (insertTextAtCursor(SQRT_INSERT_TEXT)) {
                    s_cursor--;
                    redrawInputLine(display);
                }
            }

            else if (k == Key::OPEN_PAREN) {
                if (insertTextAtCursor(PAREN_PAIR_TEXT)) {
                    s_cursor--;
                    redrawInputLine(display);
                }
            }

            else if (k == Key::CLOSE_PAREN) {
                if (skipExistingCloseParen()) {
                    redrawInputLine(display);
                } else if (s_len < MAX_CHARS) {
                    memmove(&s_buf[s_cursor + 1],
                            &s_buf[s_cursor],
                            s_len - s_cursor + 1);
                    s_buf[s_cursor] = toChar(k);
                    s_len++;
                    s_cursor++;
                    redrawInputLine(display);
                }
            }

            else if (isPrintable(k) && s_len < MAX_CHARS) {
                // Insert at cursor position instead of appending   
                memmove(&s_buf[s_cursor + 1],
                        &s_buf[s_cursor],
                        s_len - s_cursor + 1);
                s_buf[s_cursor] = toChar(k);
                s_len++;
                s_cursor++;
                redrawInputLine(display);
            }

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
