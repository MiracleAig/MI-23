//
// Created by Miracle Aigbogun on 3/22/26.
//

#include "pico/stdlib.h"
#include "display_rp2350.h"
#include "app/home/calculator_home.h"
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
static constexpr int HEADER_HEIGHT   = 22;
static constexpr int CONTENT_Y       = HEADER_HEIGHT;
static constexpr int CONTENT_H       = SCREEN_H - HEADER_HEIGHT;
static constexpr int MARGIN          = 4;
static constexpr int ENTRY_H         = CHAR_H * 2 + 4;
static constexpr int STATUS_H        = 8 + MARGIN;
static constexpr int STATUS_Y        = SCREEN_H - STATUS_H;
static constexpr int HISTORY_AREA_H  = STATUS_Y - CONTENT_Y - MARGIN;
static constexpr int MAX_VISIBLE     = (HISTORY_AREA_H - ENTRY_H) / ENTRY_H;
static constexpr int MAX_CHARS       = (SCREEN_W - MARGIN * 2) / CHAR_W;

static const uint16_t COLOR_SEPARATOR = Display::rgb(70, 70, 90);
static const uint16_t COLOR_HEADER_BG = Display::rgb(22, 35, 48);
static const uint16_t COLOR_HEADER_TEXT = Display::WHITE;
static const uint16_t COLOR_HEADER_MUTED = Display::rgb(150, 160, 172);
static const uint16_t COLOR_HOME_BG = Display::rgb(8, 10, 14);
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
    return CONTENT_Y + MARGIN + (total - firstVisible) * ENTRY_H;
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
static const char* appTitle(AppId app) {
    switch (app) {
        case AppId::Home: return "Home";
        case AppId::Calculator: return "Calculator";
        case AppId::Graphing: return "Graphing";
        default: return "";
    }
}

static int getBatteryLevel() {
    return 100;
}

static void drawBatteryIndicator(DisplayRP2350& display) {
    const int level = getBatteryLevel();
    char label[8] = {};
    snprintf(label, sizeof(label), "%d%%", level);

    const int labelX = SCREEN_W - Display::textWidth(label) - 6;
    const int iconW = 24;
    const int iconH = 10;
    const int iconX = labelX - iconW - 5;
    const int iconY = (HEADER_HEIGHT - iconH) / 2;
    const int fillW = (iconW - 4) * level / 100;

    display.drawRect(iconX, iconY, iconW, 1, COLOR_HEADER_TEXT);
    display.drawRect(iconX, iconY + iconH - 1, iconW, 1, COLOR_HEADER_TEXT);
    display.drawRect(iconX, iconY, 1, iconH, COLOR_HEADER_TEXT);
    display.drawRect(iconX + iconW - 1, iconY, 1, iconH, COLOR_HEADER_TEXT);
    display.drawRect(iconX + iconW, iconY + 3, 2, 4, COLOR_HEADER_TEXT);
    display.drawRect(iconX + 2, iconY + 2, fillW, iconH - 4, Display::GREEN);
    display.drawText(label, labelX, 7, COLOR_HEADER_TEXT);
}

static void drawGlobalHeader(DisplayRP2350& display, AppId app) {
    display.drawRect(0, 0, SCREEN_W, HEADER_HEIGHT, COLOR_HEADER_BG);
    display.drawText(appTitle(app), 8, 7, COLOR_HEADER_TEXT);
    display.drawText("DEG", SCREEN_W / 2 - Display::textWidth("DEG") / 2, 7,
                     COLOR_HEADER_MUTED);
    drawBatteryIndicator(display);
}

static void clearContentBackground(DisplayRP2350& display, uint16_t color) {
    display.drawRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, color);
}

static void requestFullRedraw(DisplayRP2350& display,
                              AppId app,
                              uint16_t contentBackground) {
    display.clear(Display::BLACK);
    drawGlobalHeader(display, app);
    clearContentBackground(display, contentBackground);
}

static void drawSeparator(DisplayRP2350& display, int y) {
    display.drawRect(MARGIN, y, SCREEN_W - MARGIN * 2, 1, COLOR_SEPARATOR);
}

static void drawStatusBar(DisplayRP2350& display) {
    display.drawRect(0, STATUS_Y, SCREEN_W, STATUS_H, Display::BLACK);
    display.drawText("ENT=eval CLR=del HOME=menu",
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

    requestFullRedraw(display, AppId::Calculator, Display::BLACK);

    for (int i = firstVisible; i < total; i++) {
        const HistoryEntry* e = getEntry(i);
        if (!e) continue;

        int row = i - firstVisible;
        int y   = CONTENT_Y + MARGIN + row * ENTRY_H;

        if (row > 0) drawSeparator(display, y - 2);

        display.drawText(e->input, MARGIN, y, Display::WHITE, SCALE);

        int resultW = static_cast<int>(strlen(e->result)) * CHAR_W;
        int resultX = SCREEN_W - resultW - MARGIN;
        if (resultX < MARGIN) resultX = MARGIN;
        display.drawText(e->result, resultX, y + CHAR_H + 2,
                         e->isError ? Display::RED : Display::GREEN, SCALE);
    }

    int inputY = CONTENT_Y + MARGIN + visibleCount * ENTRY_H;
    if (visibleCount > 0) drawSeparator(display, inputY - 2);

    display.drawRect(0, inputY, SCREEN_W, ENTRY_H, Display::BLACK);
    display.drawText(s_buf, MARGIN, inputY, Display::WHITE, SCALE);

    drawStatusBar(display);
    display.present();
}

static void drawGraphingPlaceholder(DisplayRP2350& display) {
    const uint16_t bg = Display::rgb(8, 10, 14);
    const uint16_t muted = Display::rgb(150, 160, 172);
    const uint16_t focus = Display::rgb(255, 230, 95);

    requestFullRedraw(display, AppId::Graphing, bg);

    const int iconX = SCREEN_W / 2 - 38;
    const int iconY = CONTENT_Y + 40;
    display.drawRect(iconX, iconY, 76, 2, focus);
    display.drawRect(iconX, iconY + 56, 76, 2, focus);
    display.drawRect(iconX, iconY, 2, 58, focus);
    display.drawRect(iconX + 74, iconY, 2, 58, focus);

    display.drawRect(iconX + 16, iconY + 12, 2, 34, Display::WHITE);
    display.drawRect(iconX + 16, iconY + 44, 44, 2, Display::WHITE);
    display.drawRect(iconX + 22, iconY + 36, 4, 4, Display::WHITE);
    display.drawRect(iconX + 30, iconY + 30, 4, 4, Display::WHITE);
    display.drawRect(iconX + 38, iconY + 22, 4, 4, Display::WHITE);
    display.drawRect(iconX + 46, iconY + 18, 4, 4, Display::WHITE);
    display.drawRect(iconX + 54, iconY + 24, 4, 4, Display::WHITE);

    display.drawText("Graphing",
                     SCREEN_W / 2 - Display::textWidth("Graphing") / 2,
                     138,
                     Display::WHITE);
    display.drawText("Coming soon",
                     SCREEN_W / 2 - Display::textWidth("Coming soon") / 2,
                     152,
                     muted);
    display.drawText("Press Home to return", 8, SCREEN_H - 12, muted);
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

static void enterHome(HomeScreen& home, AppId& activeApp) {
    activeApp = AppId::Home;
    home.enter();
}

static void renderHome(DisplayRP2350& display, HomeScreen& home) {
    requestFullRedraw(display, AppId::Home, COLOR_HOME_BG);
    home.renderContent(CONTENT_Y, CONTENT_H);
}

static void enterCalculator(DisplayRP2350& display,
                            AppId& activeApp,
                            bool& cursorVisible,
                            uint32_t& lastBlink) {
    activeApp = AppId::Calculator;
    redrawFull(display);
    cursorVisible = true;
    lastBlink = to_ms_since_boot(get_absolute_time());
    drawCursor(display, cursorVisible);
}

static void enterGraphing(DisplayRP2350& display,
                          AppId& activeApp,
                          bool& cursorVisible) {
    activeApp = AppId::Graphing;
    cursorVisible = false;
    drawGraphingPlaceholder(display);
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

    HomeScreen home(display);
    AppId activeApp = AppId::Home;
    bool     cursorVisible = false;
    uint32_t lastBlink     = to_ms_since_boot(get_absolute_time());
    enterHome(home, activeApp);
    renderHome(display, home);

    while (true) {
        // Poll both keypads — keypad2 only checked if keypad1 returns nothing
        Key k = keypad1.getKey();
        if (k == Key::NONE) k = keypad2.getKey();   

        if (k != Key::NONE) {
            if (k == Key::HOME) {
                if (activeApp != AppId::Home) {
                    if (activeApp == AppId::Calculator) {
                        drawCursor(display, false);
                    }
                    cursorVisible = false;
                    enterHome(home, activeApp);
                    renderHome(display, home);
                }
            } else if (activeApp == AppId::Home) {
                const AppId launchTarget = home.handleKey(k);
                if (launchTarget == AppId::Calculator) {
                    enterCalculator(display, activeApp, cursorVisible, lastBlink);
                } else if (launchTarget == AppId::Graphing) {
                    enterGraphing(display, activeApp, cursorVisible);
                } else {
                    renderHome(display, home);
                }
            } else if (activeApp == AppId::Calculator) {
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
        }

        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (activeApp == AppId::Calculator && now - lastBlink >= 500) {
            lastBlink     = now;
            cursorVisible = !cursorVisible;
            drawCursor(display, cursorVisible);
        }

        sleep_ms(16);
    }
}
