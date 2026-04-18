//
// Created by Miracle Aigbogun on 3/21/26.
//

#include "app/calculator/calculator_app.h"
#include "math/expression.h"
#include "math/math_typeset.h"
#include "platform/host/display_sdl.h"
#include "graphics/font.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

// ── Button table ─────────────────────────────────────────────────────────────
static constexpr char PI_LABEL[] = { (char)128, '\0' };

const Button CalculatorApp::BUTTONS[BTN_ROWS][BTN_COLS] = {
    { {"sin", Key::SIN},  {"cos", Key::COS},  {"tan", Key::TAN},  {"cot", Key::COT},  {"sec", Key::SEC},  {"csc", Key::CSC} },
    { {"asin", Key::ASIN},{"acos", Key::ACOS},{"atan", Key::ATAN},{"acot", Key::ACOT},{"asec", Key::ASEC},{"acsc", Key::ACSC} },
    { {"sqrt", Key::SQRT},{PI_LABEL, Key::PI},{"^",   Key::POWER},{"(",   Key::OPEN_PAREN}, {")", Key::CLOSE_PAREN}, {"CLR", Key::CLEAR} },
    { {"7",   Key::NUM_7},{"8",   Key::NUM_8},{"9",   Key::NUM_9},{"/",   Key::DIVIDE},   {"",    Key::NONE}, {"", Key::NONE} },
    { {"4",   Key::NUM_4},{"5",   Key::NUM_5},{"6",   Key::NUM_6},{"*",   Key::MULTIPLY}, {"",    Key::NONE}, {"", Key::NONE} },
    { {"1",   Key::NUM_1},{"2",   Key::NUM_2},{"3",   Key::NUM_3},{"-",   Key::MINUS},    {"",    Key::NONE}, {"", Key::NONE} },
    { {"0",   Key::NUM_0},{".",   Key::DOT},  {"+",   Key::PLUS}, {"ENT", Key::ENTER},    {"",    Key::NONE}, {"", Key::NONE} },
};

static const uint16_t COLOR_BTN_NORMAL  = Display::rgb( 55,  55,  75);
static const uint16_t COLOR_BTN_ACTION  = Display::rgb( 30,  90, 140); // ENT / CLR
static const uint16_t COLOR_BTN_FN      = Display::rgb( 80,  50, 100); // SIN/COS/TAN/π
static const uint16_t COLOR_BTN_TEXT    = Display::WHITE;
static constexpr char PAREN_PAIR_TEXT[]  = "()";

static const char* functionInsertText(Key key) {
    switch (key) {
        case Key::SQRT: return "sqrt()";
        case Key::SIN:  return "sin()";
        case Key::COS:  return "cos()";
        case Key::TAN:  return "tan()";
        case Key::COT:  return "cot()";
        case Key::SEC:  return "sec()";
        case Key::CSC:  return "csc()";
        case Key::ASIN: return "asin()";
        case Key::ACOS: return "acos()";
        case Key::ATAN: return "atan()";
        case Key::ACOT: return "acot()";
        case Key::ASEC: return "asec()";
        case Key::ACSC: return "acsc()";
        default: return nullptr;
    }
}

static bool insertTextAtCursor(char* buffer, int capacity, int& length,
                               int& cursorPos, const char* text) {
    const int insertLen = static_cast<int>(strlen(text));
    if (length + insertLen >= capacity) {
        return false;
    }

    memmove(&buffer[cursorPos + insertLen],
            &buffer[cursorPos],
            length - cursorPos + 1);
    memcpy(&buffer[cursorPos], text, insertLen);
    length += insertLen;
    cursorPos += insertLen;
    return true;
}

static bool skipExistingCloseParen(const char* buffer, int length, int& cursorPos) {
    if (cursorPos < length && buffer[cursorPos] == ')') {
        cursorPos++;
        return true;
    }
    return false;
}

static void clearInputAfterResult(char* inputBuffer, int& inputLen, int& cursorPos,
                                  char* resultBuffer, bool& resultIsError,
                                  bool& awaitingNewInput) {
    if (!awaitingNewInput) {
        return;
    }

    inputLen = 0;
    cursorPos = 0;
    inputBuffer[0] = '\0';
    resultBuffer[0] = '\0';
    resultIsError = false;
    awaitingNewInput = false;
}


CalculatorApp::CalculatorApp(DisplaySDL& display, KeypadHost& keypad)
    : m_display(display)
    , m_keypad(keypad)
    , m_inputBuffer{}
    , m_resultBuffer{}
    , m_resultIsError(false)
    , m_inputLen(0)
    , m_cursorPos(0)
    , m_inputViewportX(0)
    , m_awaitingNewInput(false)
    , m_historyScroll(0)
    , m_injectedKey(Key::NONE)
{}

void CalculatorApp::init() {
    m_display.init();
    m_keypad.init();
}

void CalculatorApp::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            m_display.setQuit();
        }
        if (event.type == SDL_KEYDOWN &&
            event.key.keysym.sym == SDLK_ESCAPE) {
            m_display.setQuit();
        }
        if (event.type == SDL_MOUSEWHEEL) {
            m_historyScroll += (event.wheel.y > 0) ? -1 : 1;
        }
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_UP) {
                m_injectedKey = Key::CURSOR_UP;
            }
            if (event.key.keysym.sym == SDLK_DOWN) {
                m_injectedKey = Key::CURSOR_DOWN;
            }
            if (event.key.keysym.sym == SDLK_LEFT) {
                m_injectedKey = Key::CURSOR_LEFT;
            }
            if (event.key.keysym.sym == SDLK_RIGHT) {
                m_injectedKey = Key::CURSOR_RIGHT;
            }
        }

        // Mouse click — hit test against the button grid
        if (event.type == SDL_MOUSEBUTTONDOWN &&
            event.button.button == SDL_BUTTON_LEFT) {
            // SDL mouse coords are in window pixels (640×480).
            // Our renderer is scaled 2×, so divide by 2 to get logical coords.
            int lx = event.button.x / 2;
            int ly = event.button.y / 2;
            const Button* btn = hitTest(lx, ly);
            if (btn) {
                m_injectedKey = btn->key;
            }
        }

        m_keypad.handleEvent(event);
    }
}

// ── Hit test ─────────────────────────────────────────────────────────────────
const Button* CalculatorApp::hitTest(int mx, int my) const {
    for (int row = 0; row < BTN_ROWS; row++) {
        for (int col = 0; col < BTN_COLS; col++) {
            int x = btnX(col);
            int y = btnY(row);
            if (mx >= x && mx < x + BTN_W &&
                my >= y && my < y + BTN_H) {
                return &BUTTONS[row][col];
            }
        }
    }
    return nullptr;
}

// ── Update ───────────────────────────────────────────────────────────────────
void CalculatorApp::update() {
    // Physical keyboard first, then injected mouse click
    Key pressed = m_keypad.getKey();
    if (pressed == Key::NONE) {
        pressed       = m_injectedKey;
        m_injectedKey = Key::NONE;
    }
    if (pressed != Key::NONE) {
        processKey(pressed);
    }
    clampScroll();
}

// ── Key processing ───────────────────────────────────────────────────────────
void CalculatorApp::processKey(Key pressed) {

    // Starting fresh after an ENTER — any printable key clears the result first
    if (m_awaitingNewInput && isPrintable(pressed)) {
        m_inputLen        = 0;
        m_cursorPos       = 0;
        m_inputBuffer[0]  = '\0';
        m_resultBuffer[0] = '\0';
        m_resultIsError   = false;
        m_awaitingNewInput = false;
        m_inputViewportX = 0;
    }

    if (pressed == Key::CURSOR_LEFT) {
        m_cursorPos = math_typeset::moveCursor(m_inputBuffer,
                                               m_cursorPos,
                                               math_typeset::CursorMove::Left);

    } else if (pressed == Key::CURSOR_RIGHT) {
        m_cursorPos = math_typeset::moveCursor(m_inputBuffer,
                                               m_cursorPos,
                                               math_typeset::CursorMove::Right);

    } else if (pressed == Key::CURSOR_UP) {
        m_cursorPos = math_typeset::moveCursor(m_inputBuffer,
                                               m_cursorPos,
                                               math_typeset::CursorMove::Up);

    } else if (pressed == Key::CURSOR_DOWN) {
        m_cursorPos = math_typeset::moveCursor(m_inputBuffer,
                                               m_cursorPos,
                                               math_typeset::CursorMove::Down);

    } else if (pressed == Key::CLEAR) {
        // Backspace at cursor position
        if (m_cursorPos > 0) {
            // Shift everything from cursorPos left by one
            memmove(&m_inputBuffer[m_cursorPos - 1],
                    &m_inputBuffer[m_cursorPos],
                    m_inputLen - m_cursorPos + 1); // +1 includes the '\0'
            m_inputLen--;
            m_cursorPos--;
            m_resultBuffer[0] = '\0';
            m_resultIsError   = false;
        }

    } else if (pressed == Key::ENTER) {
        if (m_inputLen > 0) {
            ExprResult result = evaluate(m_inputBuffer);
            if (result.ok) {
                snprintf(m_resultBuffer, sizeof(m_resultBuffer),
                         "%.6g", result.value);
                m_resultIsError = false;
            } else {
                snprintf(m_resultBuffer, sizeof(m_resultBuffer),
                         "%s", result.error);
                m_resultIsError = true;
            }
            m_awaitingNewInput = true;
            pushHistory();
        }

    } else if (functionInsertText(pressed) != nullptr) {
        clearInputAfterResult(m_inputBuffer, m_inputLen, m_cursorPos,
                              m_resultBuffer, m_resultIsError,
                              m_awaitingNewInput);

        const char* insertText = functionInsertText(pressed);
        if (insertTextAtCursor(m_inputBuffer, sizeof(m_inputBuffer),
                               m_inputLen, m_cursorPos, insertText)) {
            m_cursorPos--;
        }
    } else if (pressed == Key::OPEN_PAREN) {
        if (insertTextAtCursor(m_inputBuffer, sizeof(m_inputBuffer),
                               m_inputLen, m_cursorPos, PAREN_PAIR_TEXT)) {
            m_cursorPos--;
        }
    } else if (pressed == Key::CLOSE_PAREN) {
        if (!skipExistingCloseParen(m_inputBuffer, m_inputLen, m_cursorPos) &&
            m_inputLen < 127) {
            memmove(&m_inputBuffer[m_cursorPos + 1],
                    &m_inputBuffer[m_cursorPos],
                    m_inputLen - m_cursorPos + 1);
            m_inputBuffer[m_cursorPos] = toChar(pressed);
            m_inputLen++;
            m_cursorPos++;
        }
    } else if (isPrintable(pressed) && m_inputLen < 127) {
        // Insert character at cursor position (not just append)
        // First make room by shifting everything from cursorPos right by one
        memmove(&m_inputBuffer[m_cursorPos + 1],
                &m_inputBuffer[m_cursorPos],
                m_inputLen - m_cursorPos + 1); // +1 includes the '\0'
        m_inputBuffer[m_cursorPos] = toChar(pressed);
        m_inputLen++;
        m_cursorPos++;
    }
}

// ── History ──────────────────────────────────────────────────────────────────
void CalculatorApp::pushHistory() {
    int maxScrollBefore = std::max(0,
        static_cast<int>(m_history.size()) - VISIBLE_HISTORY_COUNT);
    bool wasNearBottom = (m_historyScroll >= std::max(0, maxScrollBefore - 1));

    m_history.push_back({m_inputBuffer, m_resultBuffer, m_resultIsError});

    if (wasNearBottom) {
        m_historyScroll = std::max(0,
            static_cast<int>(m_history.size()) - VISIBLE_HISTORY_COUNT);
    }

    m_inputLen        = 0;
    m_cursorPos       = 0;
    m_inputBuffer[0]  = '\0';
    m_resultBuffer[0] = '\0';
    m_resultIsError   = false;
    m_inputViewportX  = 0;
}

void CalculatorApp::clampScroll() {
    if (m_historyScroll < 0) m_historyScroll = 0;
    int maxScroll = std::max(0,
        static_cast<int>(m_history.size()) - VISIBLE_HISTORY_COUNT);
    if (m_historyScroll > maxScroll) m_historyScroll = maxScroll;
}

// ── Render ───────────────────────────────────────────────────────────────────
void CalculatorApp::render() {
    m_display.clear(Display::BLACK);
    m_display.drawRect(0, HISTORY_TOP, DISPLAY_WIDTH, HISTORY_HEIGHT,
                       COLOR_HISTORY_BG);
    drawHistory();
    drawInputRow();
    drawButtonGrid();
    m_display.present();
    SDL_Delay(16);
}

void CalculatorApp::drawHistory() {
    int startIndex = m_historyScroll;
    int endIndex   = std::min(startIndex + VISIBLE_HISTORY_COUNT,
                              static_cast<int>(m_history.size()));

    for (int i = startIndex; i < endIndex; i++) {
        int row = i - startIndex;
        int y   = HISTORY_TOP + row * ROW_HEIGHT;

        if (y > HISTORY_TOP) {
            m_display.drawRect(MARGIN, y - 2,
                               DISPLAY_WIDTH - MARGIN * 2, 1,
                               COLOR_SEPARATOR);
        }

        const bool drewMath = math_typeset::draw(m_history[i].input.c_str(),
                                                 m_display,
                                                 MARGIN,
                                                 y + (FONT_CHAR_HEIGHT - 1),
                                                 Display::WHITE);
        if (!drewMath) {
            m_display.drawText(m_history[i].input.c_str(), MARGIN, y,
                               Display::WHITE);
        }

        int resultX = DISPLAY_WIDTH
                      - Display::textWidth(m_history[i].result.c_str())
                      - MARGIN;
        if (resultX < 0) resultX = 0;
        m_display.drawText(m_history[i].result.c_str(), resultX, y + 8,
                           m_history[i].isError ? Display::RED : Display::GREEN);
    }

    int maxScroll = std::max(0,
        static_cast<int>(m_history.size()) - VISIBLE_HISTORY_COUNT);
    if (static_cast<int>(m_history.size()) > VISIBLE_HISTORY_COUNT) {
        drawScrollbar(maxScroll);
    }
}

void CalculatorApp::drawInputRow() {
    int visibleCount = std::min(VISIBLE_HISTORY_COUNT,
                                static_cast<int>(m_history.size()) - m_historyScroll);
    int inputY = HISTORY_TOP + visibleCount * ROW_HEIGHT;

    if (inputY > HISTORY_TOP) {
        m_display.drawRect(MARGIN, inputY - 2,
                           DISPLAY_WIDTH - MARGIN * 2, 1,
                           COLOR_SEPARATOR);
    }

    const int prefixWidth = math_typeset::measurePrefixWidth(m_inputBuffer, m_cursorPos);
    const int viewportWidth = DISPLAY_WIDTH - MARGIN * 2;
    if (prefixWidth - m_inputViewportX > viewportWidth - 8) {
        m_inputViewportX = prefixWidth - (viewportWidth - 8);
    }
    if (prefixWidth - m_inputViewportX < 0) {
        m_inputViewportX = prefixWidth;
    }
    if (m_inputViewportX < 0) {
        m_inputViewportX = 0;
    }

    const int inputOriginX = MARGIN - m_inputViewportX;
    const bool drewMath = math_typeset::draw(m_inputBuffer,
                                             m_display,
                                             inputOriginX,
                                             inputY + (FONT_CHAR_HEIGHT - 1),
                                             Display::WHITE);
    if (!drewMath) {
        m_display.drawText(m_inputBuffer, inputOriginX, inputY, Display::WHITE);
    }

    int resultX = DISPLAY_WIDTH
                  - Display::textWidth(m_resultBuffer) - MARGIN;
    if (resultX < 0) resultX = 0;
    m_display.drawText(m_resultBuffer, resultX, inputY + 10,
                       m_resultIsError ? Display::RED : Display::GREEN);

    drawCursor(inputY);
}

void CalculatorApp::drawCursor(int inputY) {
    bool showCursor = ((SDL_GetTicks() / 500) % 2) == 0;
    if (!showCursor) return;

    // Cursor sits at cursorPos, not necessarily the end of the string
    const int prefixWidth = math_typeset::measurePrefixWidth(m_inputBuffer, m_cursorPos);
    int cursorX = MARGIN + prefixWidth - m_inputViewportX;

    math_typeset::LayoutMetrics metrics{};
    const bool hasMathMetrics = math_typeset::measure(m_inputBuffer, metrics);
    const int cursorTop = hasMathMetrics
        ? inputY + (FONT_CHAR_HEIGHT - 1) - metrics.ascent
        : inputY;
    const int cursorHeight = hasMathMetrics
        ? std::max(FONT_CHAR_HEIGHT, metrics.ascent + metrics.descent)
        : FONT_CHAR_HEIGHT;

    if (cursorX < DISPLAY_WIDTH - MARGIN) {
        m_display.drawRect(cursorX, cursorTop, 2, cursorHeight, Display::WHITE);
    }
}

void CalculatorApp::drawScrollbar(int maxScroll) {
    int scrollbarX = DISPLAY_WIDTH - 4;
    int scrollbarHeight = std::max(8,
        (HISTORY_HEIGHT * VISIBLE_HISTORY_COUNT)
            / static_cast<int>(m_history.size()));
    int scrollbarY = HISTORY_TOP
        + ((HISTORY_HEIGHT - scrollbarHeight) * m_historyScroll)
            / std::max(1, maxScroll);

    m_display.drawRect(scrollbarX, HISTORY_TOP, 3, HISTORY_HEIGHT,
                       COLOR_SCROLLBAR_BG);
    m_display.drawRect(scrollbarX, scrollbarY, 3, scrollbarHeight,
                       Display::WHITE);
}


void CalculatorApp::drawButtonGrid() {
    for (int row = 0; row < BTN_ROWS; row++) {
        for (int col = 0; col < BTN_COLS; col++) {
            const Button& btn = BUTTONS[row][col];

            int x = btnX(col);
            int y = btnY(row);


            uint16_t bgColor;
            if (row <= 2) {
                bgColor = COLOR_BTN_FN;       // function row plus π / power / parens
            } else if (btn.key == Key::ENTER || btn.key == Key::CLEAR) {
                bgColor = COLOR_BTN_ACTION;   // ENT / CLR stand out
            } else {
                bgColor = COLOR_BTN_NORMAL;
            }

            m_display.drawRect(x, y, BTN_W, BTN_H, bgColor);

            // Center the label text inside the button
            int labelW  = Display::textWidth(btn.label);
            int labelX  = x + (BTN_W - labelW) / 2;
            int labelY  = y + (BTN_H - FONT_CHAR_HEIGHT) / 2;
            m_display.drawText(btn.label, labelX, labelY, COLOR_BTN_TEXT);
        }
    }
}
