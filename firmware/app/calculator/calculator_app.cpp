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
static constexpr char SQRT_LABEL[] = { (char)129, '\0' };
static constexpr char NTH_ROOT_LABEL[] = { 'n', (char)129, '\0' };

const Button CalculatorApp::BUTTONS[BTN_ROWS][BTN_COLS] = {
    { {"sin", Key::SIN},  {"cos", Key::COS},  {"tan", Key::TAN},  {"cot", Key::COT},  {"sec", Key::SEC},  {"csc", Key::CSC} },
    { {"asin", Key::ASIN},{"acos", Key::ACOS},{"atan", Key::ATAN},{"acot", Key::ACOT},{"asec", Key::ASEC},{"acsc", Key::ACSC} },
    { {SQRT_LABEL, Key::SQRT},{PI_LABEL, Key::PI},{"^",   Key::POWER},{"(",   Key::OPEN_PAREN}, {")", Key::CLOSE_PAREN}, {"CLR", Key::CLEAR} },
    { {"7",   Key::NUM_7},{"8",   Key::NUM_8},{"9",   Key::NUM_9},{"/",   Key::DIVIDE},   {"%",   Key::PERCENT}, {"!", Key::FACTORIAL} },
    { {"4",   Key::NUM_4},{"5",   Key::NUM_5},{"6",   Key::NUM_6},{"*",   Key::MULTIPLY}, {"e",   Key::E_CONST}, {"log", Key::LOG} },
    { {"1",   Key::NUM_1},{"2",   Key::NUM_2},{"3",   Key::NUM_3},{"-",   Key::MINUS},    {"ln",  Key::LN}, {",", Key::COMMA} },
    { {"0",   Key::NUM_0},{".",   Key::DOT},  {"+",   Key::PLUS}, {"ENT", Key::ENTER},    {"Ans", Key::ANS}, {NTH_ROOT_LABEL, Key::ROOT} },
};

static const uint16_t COLOR_BTN_NORMAL  = Display::rgb( 55,  55,  75);
static const uint16_t COLOR_BTN_ACTION  = Display::rgb( 30,  90, 140); // ENT / CLR
static const uint16_t COLOR_BTN_FN      = Display::rgb( 80,  50, 100); // SIN/COS/TAN/π
static const uint16_t COLOR_BTN_TEXT    = Display::WHITE;
static constexpr char PAREN_PAIR_TEXT[]  = "()";
static constexpr int ENTRY_VERTICAL_PADDING = 2;

static const char* functionInsertText(Key key) {
    switch (key) {
        case Key::SQRT: return "sqrt()";
        case Key::ROOT: return "root(,)";
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
        case Key::LOG:  return "log(,)";
        case Key::LN:   return "ln()";
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

static int baselineForEntry(int topY,
                            const math_typeset::LayoutMetrics& metrics,
                            float scale) {
    return topY + ENTRY_VERTICAL_PADDING
        + math_typeset::scaleLength(metrics.ascent, scale);
}

static int historyEntryHeight(const HistoryEntry& entry) {
    math_typeset::LayoutMetrics metrics{};
    if (math_typeset::measure(entry.input.c_str(), metrics)) {
        return std::max(ROW_HEIGHT,
                        metrics.ascent + metrics.descent
                            + ENTRY_VERTICAL_PADDING * 2);
    }

    return ROW_HEIGHT;
}

static int inputEntryHeight(const char* input) {
    math_typeset::LayoutMetrics metrics{};
    if (math_typeset::measure(input, metrics)) {
        return std::max(ROW_HEIGHT,
                        metrics.ascent + metrics.descent
                            + ENTRY_VERTICAL_PADDING * 2);
    }

    return ROW_HEIGHT;
}

static int historyViewportHeightForInput(const char* input) {
    return std::max(0, HISTORY_HEIGHT - inputEntryHeight(input));
}

static int bottomHistoryScroll(const std::vector<HistoryEntry>& history,
                               int viewportHeight) {
    if (viewportHeight <= 0) {
        return static_cast<int>(history.size());
    }

    int scrollIndex = static_cast<int>(history.size());
    int usedHeight = 0;

    while (scrollIndex > 0) {
        const int entryHeight = historyEntryHeight(history[scrollIndex - 1]);
        if (usedHeight + entryHeight > viewportHeight) {
            break;
        }

        scrollIndex--;
        usedHeight += entryHeight;
        if (usedHeight >= viewportHeight) {
            break;
        }
    }

    return scrollIndex;
}

static int visibleHistoryHeight(const std::vector<HistoryEntry>& history,
                                int scrollIndex,
                                int viewportHeight) {
    if (viewportHeight <= 0) {
        return 0;
    }

    int usedHeight = 0;

    for (int i = scrollIndex; i < static_cast<int>(history.size()); i++) {
        const int entryHeight = historyEntryHeight(history[i]);
        if (usedHeight + entryHeight > viewportHeight) {
            break;
        }

        usedHeight += entryHeight;
        if (usedHeight >= viewportHeight) {
            break;
        }
    }

    return std::min(usedHeight, viewportHeight);
}

static bool cursorMoveForKey(Key key, math_typeset::CursorMove& move) {
    switch (key) {
        case Key::CURSOR_LEFT:
            move = math_typeset::CursorMove::Left;
            return true;
        case Key::CURSOR_RIGHT:
            move = math_typeset::CursorMove::Right;
            return true;
        case Key::CURSOR_UP:
            move = math_typeset::CursorMove::Up;
            return true;
        case Key::CURSOR_DOWN:
            move = math_typeset::CursorMove::Down;
            return true;
        default:
            return false;
    }
}

static void drawButtonRootIcon(Display& display,
                               int x,
                               int y,
                               bool showIndex,
                               uint16_t color) {
    const int radicalWidth = showIndex ? 17 : 15;
    const int radicalHeight = 12;
    const int totalWidth = radicalWidth + (showIndex ? 6 : 0);
    const int originX = x + (BTN_W - totalWidth) / 2 + (showIndex ? 5 : 0);
    const int top = y + (BTN_H - radicalHeight) / 2 + 1;
    const int baseline = top + radicalHeight - 3;
    const int hookX = originX;
    const int stemX = originX + 5;

    if (showIndex) {
        display.drawText("n", originX - 6, top - 1, color);
    }

    display.drawPixel(hookX, baseline - 2, color);
    display.drawPixel(hookX + 1, baseline - 1, color);
    display.drawPixel(hookX + 2, baseline, color);
    display.drawPixel(hookX + 3, baseline - 1, color);
    display.drawPixel(hookX + 4, baseline - 3, color);
    display.drawRect(stemX, top + 1, 1, baseline - top - 3, color);
    display.drawRect(stemX, top, radicalWidth - 5, 1, color);
}


CalculatorApp::CalculatorApp(DisplaySDL& display, KeypadHost& keypad)
    : m_display(display)
    , m_keypad(keypad)
    , m_inputBuffer{}
    , m_resultBuffer{}
    , m_resultIsError(false)
    , m_lastAnswer(0.0f)
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
    handleKey(pressed);
}

void CalculatorApp::handleKey(Key pressed) {
    if (pressed != Key::NONE) {
        processKey(pressed);
    }
    clampScroll();
}

void CalculatorApp::handlePointerDown(int logicalX, int logicalY) {
    const Button* btn = hitTest(logicalX, logicalY);
    if (btn) {
        handleKey(btn->key);
    }
}

void CalculatorApp::scrollHistory(int delta) {
    m_historyScroll += delta;
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

    math_typeset::CursorMove cursorMove{};
    if (cursorMoveForKey(pressed, cursorMove)) {
        const int moved = math_typeset::moveCursor(m_inputBuffer,
                                                   m_cursorPos,
                                                   cursorMove);
        if (moved != m_cursorPos) {
            m_cursorPos = moved;
        } else if (pressed == Key::CURSOR_UP) {
            m_historyScroll--;
        } else if (pressed == Key::CURSOR_DOWN) {
            m_historyScroll++;
        }

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
            ExprResult result = evaluate(m_inputBuffer, m_lastAnswer);
            if (result.ok) {
                snprintf(m_resultBuffer, sizeof(m_resultBuffer),
                         "%.6g", result.value);
                m_resultIsError = false;
                m_lastAnswer = result.value;
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
            if (pressed == Key::LOG || pressed == Key::ROOT) {
                m_cursorPos -= 2;
            } else {
                m_cursorPos--;
            }
        }
    } else if (pressed == Key::ANS) {
        clearInputAfterResult(m_inputBuffer, m_inputLen, m_cursorPos,
                              m_resultBuffer, m_resultIsError,
                              m_awaitingNewInput);
        insertTextAtCursor(m_inputBuffer, sizeof(m_inputBuffer),
                           m_inputLen, m_cursorPos, "Ans");
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
    int maxScrollBefore = bottomHistoryScroll(m_history, HISTORY_HEIGHT);
    bool wasNearBottom = (m_historyScroll >= std::max(0, maxScrollBefore - 1));

    m_history.push_back({m_inputBuffer, m_resultBuffer, m_resultIsError});

    if (wasNearBottom) {
        m_historyScroll = bottomHistoryScroll(m_history, HISTORY_HEIGHT);
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
    const int historyViewportHeight = historyViewportHeightForInput(m_inputBuffer);
    int maxScroll = bottomHistoryScroll(m_history, historyViewportHeight);
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
}

void CalculatorApp::drawHistory() {
    int startIndex = m_historyScroll;
    int y = HISTORY_TOP;
    const int historyViewportHeight = historyViewportHeightForInput(m_inputBuffer);
    const int historyBottom = HISTORY_TOP + historyViewportHeight;

    for (int i = startIndex; i < static_cast<int>(m_history.size()); i++) {
        const int entryHeight = historyEntryHeight(m_history[i]);
        if (y >= historyBottom || y + entryHeight > historyBottom) {
            break;
        }

        if (y > HISTORY_TOP) {
            m_display.drawRect(MARGIN, y - 2,
                               DISPLAY_WIDTH - MARGIN * 2, 1,
                               COLOR_SEPARATOR);
        }

        const int rowTop = y;

        math_typeset::LayoutMetrics historyMetrics{};
        const bool measuredMath = math_typeset::measure(m_history[i].input.c_str(),
                                                        historyMetrics);
        const float expressionScale = 1.0f;
        const int baselineY = measuredMath
            ? baselineForEntry(rowTop, historyMetrics, expressionScale)
            : y + (FONT_CHAR_HEIGHT - 1);

        const bool drewMath = measuredMath &&
                              math_typeset::drawScaled(m_history[i].input.c_str(),
                                                       m_display,
                                                       MARGIN,
                                                       baselineY,
                                                       Display::WHITE,
                                                       expressionScale);
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

        y += entryHeight;
    }

    int maxScroll = bottomHistoryScroll(m_history, historyViewportHeight);
    if (maxScroll > 0) {
        drawScrollbar(maxScroll, historyViewportHeight);
    }
}

void CalculatorApp::drawInputRow() {
    const int historyViewportHeight = historyViewportHeightForInput(m_inputBuffer);
    int inputY = HISTORY_TOP
        + visibleHistoryHeight(m_history, m_historyScroll, historyViewportHeight);

    if (inputY > HISTORY_TOP) {
        m_display.drawRect(MARGIN, inputY - 2,
                           DISPLAY_WIDTH - MARGIN * 2, 1,
                           COLOR_SEPARATOR);
    }

    const int inputTop = inputY;
    math_typeset::LayoutMetrics metrics{};
    const bool measuredMath = math_typeset::measure(m_inputBuffer, metrics);
    const float expressionScale = 1.0f;
    const int baselineY = measuredMath
        ? baselineForEntry(inputTop, metrics, expressionScale)
        : inputY + (FONT_CHAR_HEIGHT - 1);

    const int viewportWidth = DISPLAY_WIDTH - MARGIN * 2;
    int cursorLayoutX = 0;
    math_typeset::CursorMetrics cursorLayoutMetrics{};
    if (measuredMath &&
        math_typeset::cursorMetrics(m_inputBuffer,
                                    m_cursorPos,
                                    expressionScale,
                                    cursorLayoutMetrics)) {
        cursorLayoutX = cursorLayoutMetrics.x;
    } else {
        cursorLayoutX = math_typeset::scaleLength(
            math_typeset::measurePrefixWidth(m_inputBuffer, m_cursorPos),
            expressionScale);
    }

    if (cursorLayoutX - m_inputViewportX > viewportWidth - 8) {
        m_inputViewportX = cursorLayoutX - (viewportWidth - 8);
    }
    if (cursorLayoutX - m_inputViewportX < 0) {
        m_inputViewportX = cursorLayoutX;
    }
    if (m_inputViewportX < 0) {
        m_inputViewportX = 0;
    }

    const int inputOriginX = MARGIN - m_inputViewportX;
    const bool drewMath = measuredMath &&
                          math_typeset::drawScaled(m_inputBuffer,
                                                   m_display,
                                                   inputOriginX,
                                                   baselineY,
                                                   Display::WHITE,
                                                   expressionScale);
    if (!drewMath) {
        m_display.drawText(m_inputBuffer, inputOriginX, inputY, Display::WHITE);
        metrics = {
            Display::textWidth(m_inputBuffer),
            FONT_CHAR_HEIGHT - 1,
            1,
        };
    }

    int resultX = DISPLAY_WIDTH
                  - Display::textWidth(m_resultBuffer) - MARGIN;
    if (resultX < 0) resultX = 0;
    m_display.drawText(m_resultBuffer, resultX, inputY + 10,
                       m_resultIsError ? Display::RED : Display::GREEN);

    drawCursor(inputOriginX, baselineY, expressionScale, metrics, drewMath);
}

void CalculatorApp::drawCursor(int originX,
                               int baselineY,
                               float expressionScale,
                               const math_typeset::LayoutMetrics& metrics,
                               bool usedMathLayout) {
    bool showCursor = ((SDL_GetTicks() / 500) % 2) == 0;
    if (!showCursor) return;

    math_typeset::CursorMetrics cursorMetrics{};
    const bool hasCursorMetrics = usedMathLayout &&
        math_typeset::cursorMetrics(m_inputBuffer,
                                    m_cursorPos,
                                    expressionScale,
                                    cursorMetrics);

    int cursorX = originX;
    int cursorTop = baselineY
        - math_typeset::scaleLength(metrics.ascent, expressionScale);
    int cursorHeight = std::max(2,
        math_typeset::scaleLength(metrics.ascent + metrics.descent, expressionScale));

    if (hasCursorMetrics) {
        cursorX += cursorMetrics.x;
        cursorTop = baselineY + cursorMetrics.baselineOffset - cursorMetrics.ascent;
        cursorHeight = std::max(2, cursorMetrics.ascent + cursorMetrics.descent);
    } else {
        // Cursor sits at cursorPos, not necessarily the end of the string.
        const int prefixWidth = math_typeset::measurePrefixWidth(m_inputBuffer, m_cursorPos);
        cursorX += math_typeset::scaleLength(prefixWidth, expressionScale);
    }

    if (cursorX < DISPLAY_WIDTH - MARGIN) {
        m_display.drawRect(cursorX, cursorTop, 2, cursorHeight, Display::WHITE);
    }
}

void CalculatorApp::drawScrollbar(int maxScroll, int viewportHeight) {
    if (viewportHeight <= 0) {
        return;
    }

    int scrollbarX = DISPLAY_WIDTH - 4;
    const int visibleHeight = visibleHistoryHeight(m_history,
                                                   m_historyScroll,
                                                   viewportHeight);
    const int contentHeight = std::max(1, viewportHeight + ROW_HEIGHT * maxScroll);
    int scrollbarHeight = std::max(8,
        (viewportHeight * std::max(1, visibleHeight)) / contentHeight);
    int scrollbarY = HISTORY_TOP
        + ((viewportHeight - scrollbarHeight) * m_historyScroll)
            / std::max(1, maxScroll);

    m_display.drawRect(scrollbarX, HISTORY_TOP, 3, viewportHeight,
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

            if (btn.key == Key::SQRT || btn.key == Key::ROOT) {
                drawButtonRootIcon(m_display,
                                   x,
                                   y,
                                   btn.key == Key::ROOT,
                                   COLOR_BTN_TEXT);
                continue;
            }

            // Center the label text inside the button
            int labelW  = Display::textWidth(btn.label);
            int labelX  = x + (BTN_W - labelW) / 2;
            int labelY  = y + (BTN_H - FONT_CHAR_HEIGHT) / 2;
            m_display.drawText(btn.label, labelX, labelY, COLOR_BTN_TEXT);
        }
    }
}
