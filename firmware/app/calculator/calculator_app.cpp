//
// Created by Miracle Aigbogun on 3/21/26.
//

#include "app/calculator/calculator_app.h"
#include "math/expression.h"
#include "math/math_typeset.h"
#include "graphics/font.h"
#include <algorithm>
#include <chrono>
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
static constexpr int SCALED_RESULT_GAP = 4;

static int normalizedScale(int scale) {
    return std::max(1, scale);
}

static int scaledTextWidth(const char* text, int scale) {
    return Display::textWidth(text) * normalizedScale(scale);
}

static int scaledFontHeight(int scale) {
    return FONT_CHAR_HEIGHT * normalizedScale(scale);
}

static void drawPlainTextScaled(Display& display,
                                const char* text,
                                int x,
                                int y,
                                uint16_t color,
                                int scale) {
    const int normalized = normalizedScale(scale);
    int cursorX = x;

    while (*text != '\0') {
        const unsigned char c = static_cast<unsigned char>(*text);
        const uint8_t* glyph = &FONT_DATA[c * FONT_CHAR_WIDTH];
        for (int col = 0; col < FONT_CHAR_WIDTH; col++) {
            const uint8_t columnBits = glyph[col];
            for (int row = 0; row < FONT_CHAR_HEIGHT; row++) {
                if ((columnBits >> row) & 1U) {
                    display.fillRect(cursorX + col * normalized,
                                     y + row * normalized,
                                     normalized,
                                     normalized,
                                     color);
                }
            }
        }

        cursorX += FONT_CHAR_ADVANCE * normalized;
        text++;
    }
}

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

static int historyEntryHeight(const HistoryEntry& entry, int uiScale) {
    const int scale = normalizedScale(uiScale);
    math_typeset::LayoutMetrics metrics{};
    if (math_typeset::measure(entry.input.c_str(), metrics)) {
        const int inputHeight = math_typeset::scaleLength(metrics.ascent + metrics.descent,
                                                          static_cast<float>(scale));
        if (scale > 1) {
            return std::max(ROW_HEIGHT,
                            inputHeight + scaledFontHeight(scale)
                                + ENTRY_VERTICAL_PADDING * 2 + SCALED_RESULT_GAP);
        }

        return std::max(ROW_HEIGHT,
                        metrics.ascent + metrics.descent
                            + ENTRY_VERTICAL_PADDING * 2);
    }

    if (scale > 1) {
        return std::max(ROW_HEIGHT,
                        scaledFontHeight(scale) * 2
                            + ENTRY_VERTICAL_PADDING * 2 + SCALED_RESULT_GAP);
    }

    return ROW_HEIGHT;
}

static int inputEntryHeight(const char* input, int uiScale) {
    const int scale = normalizedScale(uiScale);
    math_typeset::LayoutMetrics metrics{};
    if (math_typeset::measure(input, metrics)) {
        const int inputHeight = math_typeset::scaleLength(metrics.ascent + metrics.descent,
                                                          static_cast<float>(scale));
        if (scale > 1) {
            return std::max(ROW_HEIGHT,
                            inputHeight + scaledFontHeight(scale)
                                + ENTRY_VERTICAL_PADDING * 2 + SCALED_RESULT_GAP);
        }

        return std::max(ROW_HEIGHT,
                        metrics.ascent + metrics.descent
                            + ENTRY_VERTICAL_PADDING * 2);
    }

    if (scale > 1) {
        return std::max(ROW_HEIGHT,
                        scaledFontHeight(scale) * 2
                            + ENTRY_VERTICAL_PADDING * 2 + SCALED_RESULT_GAP);
    }

    return ROW_HEIGHT;
}

static int historyViewportHeightForInput(const char* input,
                                         int historyHeight,
                                         int uiScale) {
    return std::max(0, historyHeight - inputEntryHeight(input, uiScale));
}

template <typename EntryGetter>
static int bottomHistoryScroll(int historySize,
                               EntryGetter getEntry,
                               int viewportHeight,
                               int uiScale) {
    if (viewportHeight <= 0) {
        return historySize;
    }

    int scrollIndex = historySize;
    int usedHeight = 0;

    while (scrollIndex > 0) {
        const int entryHeight = historyEntryHeight(getEntry(scrollIndex - 1), uiScale);
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

template <typename EntryGetter>
static int visibleHistoryHeight(int historySize,
                                EntryGetter getEntry,
                                int scrollIndex,
                                int viewportHeight,
                                int uiScale) {
    if (viewportHeight <= 0) {
        return 0;
    }

    int usedHeight = 0;

    for (int i = scrollIndex; i < historySize; i++) {
        const int entryHeight = historyEntryHeight(getEntry(i), uiScale);
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
    display.fillRect(stemX, top + 1, 1, baseline - top - 3, color);
    display.fillRect(stemX, top, radicalWidth - 5, 1, color);
}


CalculatorApp::CalculatorApp(Display& display, Keypad& keypad,
                             const CalculatorAppConfig& config)
    : m_display(display)
    , m_keypad(keypad)
    , m_inputBuffer{}
    , m_resultBuffer{}
    , m_resultIsError(false)
    , m_lastAnswer(0.0f)
    , m_inputLen(0)
    , m_cursorPos(0)
    , m_inputViewportX(0)
    , m_inputLayoutDirty(true)
    , m_cachedInputMeasured(false)
    , m_cachedInputMetrics{0, FONT_CHAR_HEIGHT - 1, 1}
    , m_awaitingNewInput(false)
    , m_historyBottom(config.showOnScreenKeypad
        ? HISTORY_BOTTOM_WITH_KEYPAD
        : (DISPLAY_HEIGHT - 4))
    , m_historyHeight(m_historyBottom - HISTORY_TOP)
    , m_historyCount(0)
    , m_historyStart(0)
    , m_historyScroll(0)
    , m_injectedKey(Key::NONE)
    , m_config(config)
    , m_needsRender(true)
{}

void CalculatorApp::init() {
    m_display.init();
    m_keypad.init();
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
        markInputLayoutDirty();
        processKey(pressed);
        m_needsRender = true;
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
    m_needsRender = true;
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
    int maxScrollBefore = bottomHistoryScroll(historySize(),
                                              [this](int i) -> const HistoryEntry& {
                                                  return historyAt(i);
                                              },
                                              m_historyHeight,
                                              m_config.uiScale);
    bool wasNearBottom = (m_historyScroll >= std::max(0, maxScrollBefore - 1));

    if (m_historyCount < MAX_HISTORY) {
        const int insertIndex = (m_historyStart + m_historyCount) % MAX_HISTORY;
        m_history[insertIndex] = {m_inputBuffer, m_resultBuffer, m_resultIsError};
        m_historyCount++;
    } else {
        m_history[m_historyStart] = {m_inputBuffer, m_resultBuffer, m_resultIsError};
        m_historyStart = (m_historyStart + 1) % MAX_HISTORY;
    }

    if (wasNearBottom) {
        m_historyScroll = bottomHistoryScroll(historySize(),
                                              [this](int i) -> const HistoryEntry& {
                                                  return historyAt(i);
                                              },
                                              m_historyHeight,
                                              m_config.uiScale);
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
    const int historyViewportHeight = historyViewportHeightForInput(m_inputBuffer,
                                                                    m_historyHeight,
                                                                    m_config.uiScale);
    int maxScroll = bottomHistoryScroll(historySize(),
                                        [this](int i) -> const HistoryEntry& {
                                            return historyAt(i);
                                        },
                                        historyViewportHeight,
                                        m_config.uiScale);
    if (m_historyScroll > maxScroll) m_historyScroll = maxScroll;
}

// ── Render ───────────────────────────────────────────────────────────────────
void CalculatorApp::render() {
    if (!m_needsRender) {
        return;
    }
    m_display.clear(Display::BLACK);
    m_display.fillRect(0, HISTORY_TOP, DISPLAY_WIDTH, m_historyHeight,
                       COLOR_HISTORY_BG);
    drawHistory();
    drawInputRow();
    if (m_config.showOnScreenKeypad) {
        drawButtonGrid();
    }
    m_display.present();
    m_needsRender = false;
}

void CalculatorApp::requestRender() {
    m_needsRender = true;
}

void CalculatorApp::markInputLayoutDirty() {
    // Cursor placement is derived from the parsed math layout. Any input edit or
    // structural cursor move invalidates that geometry before the next draw.
    m_inputLayoutDirty = true;
}

bool CalculatorApp::ensureInputLayout() {
    if (!m_inputLayoutDirty) {
        return m_cachedInputMeasured;
    }

    m_cachedInputMetrics = {0, FONT_CHAR_HEIGHT - 1, 1};
    m_cachedInputMeasured = math_typeset::measure(m_inputBuffer, m_cachedInputMetrics);
    m_inputLayoutDirty = false;
    return m_cachedInputMeasured;
}

void CalculatorApp::drawHistory() {
    int startIndex = m_historyScroll;
    int y = HISTORY_TOP;
    const int historyViewportHeight = historyViewportHeightForInput(m_inputBuffer,
                                                                    m_historyHeight,
                                                                    m_config.uiScale);
    const int historyBottom = HISTORY_TOP + historyViewportHeight;

    for (int i = startIndex; i < historySize(); i++) {
        const int entryHeight = historyEntryHeight(historyAt(i), m_config.uiScale);
        if (y >= historyBottom || y + entryHeight > historyBottom) {
            break;
        }

        if (y > HISTORY_TOP) {
            m_display.fillRect(MARGIN, y - 2,
                               DISPLAY_WIDTH - MARGIN * 2, 1,
                               COLOR_SEPARATOR);
        }

        const int rowTop = y;

        math_typeset::LayoutMetrics historyMetrics{};
        const bool measuredMath = math_typeset::measure(historyAt(i).input.c_str(),
                                                        historyMetrics);
        const int uiScale = normalizedScale(m_config.uiScale);
        const float expressionScale = static_cast<float>(uiScale);
        const int baselineY = measuredMath
            ? baselineForEntry(rowTop, historyMetrics, expressionScale)
            : y + math_typeset::scaleLength(FONT_CHAR_HEIGHT - 1, expressionScale);

        const bool drewMath = measuredMath &&
                              math_typeset::drawScaled(historyAt(i).input.c_str(),
                                                       m_display,
                                                       MARGIN,
                                                       baselineY,
                                                       Display::WHITE,
                                                       expressionScale);
        if (!drewMath) {
            drawPlainTextScaled(m_display,
                                historyAt(i).input.c_str(),
                                MARGIN,
                                y,
                                Display::WHITE,
                                uiScale);
        }

        int resultX = DISPLAY_WIDTH
                      - scaledTextWidth(historyAt(i).result.c_str(), uiScale)
                      - MARGIN;
        if (resultX < 0) resultX = 0;
        const int resultY = (uiScale > 1)
            ? y + entryHeight - ENTRY_VERTICAL_PADDING - scaledFontHeight(uiScale)
            : y + 8;
        drawPlainTextScaled(m_display,
                            historyAt(i).result.c_str(),
                            resultX,
                            resultY,
                            historyAt(i).isError ? Display::RED : Display::GREEN,
                            uiScale);

        y += entryHeight;
    }

    int maxScroll = bottomHistoryScroll(historySize(),
                                        [this](int i) -> const HistoryEntry& {
                                            return historyAt(i);
                                        },
                                        historyViewportHeight,
                                        m_config.uiScale);
    if (maxScroll > 0) {
        drawScrollbar(maxScroll, historyViewportHeight);
    }
}

void CalculatorApp::drawInputRow() {
    const int historyViewportHeight = historyViewportHeightForInput(m_inputBuffer,
                                                                    m_historyHeight,
                                                                    m_config.uiScale);
    int inputY = HISTORY_TOP
        + visibleHistoryHeight(historySize(),
                               [this](int i) -> const HistoryEntry& { return historyAt(i); },
                               m_historyScroll,
                               historyViewportHeight,
                               m_config.uiScale);

    if (inputY > HISTORY_TOP) {
        m_display.fillRect(MARGIN, inputY - 2,
                           DISPLAY_WIDTH - MARGIN * 2, 1,
                           COLOR_SEPARATOR);
    }

    const int inputTop = inputY;
    const int inputRowHeight = inputEntryHeight(m_inputBuffer, m_config.uiScale);
    m_display.fillRect(0,
                       inputTop,
                       DISPLAY_WIDTH,
                       std::min(inputRowHeight, DISPLAY_HEIGHT - inputTop),
                       Display::BLACK);

    math_typeset::LayoutMetrics metrics = m_cachedInputMetrics;
    const bool measuredMath = ensureInputLayout();
    metrics = m_cachedInputMetrics;
    const int uiScale = normalizedScale(m_config.uiScale);
    const float expressionScale = static_cast<float>(uiScale);
    const int baselineY = measuredMath
        ? baselineForEntry(inputTop, metrics, expressionScale)
        : inputY + math_typeset::scaleLength(FONT_CHAR_HEIGHT - 1, expressionScale);

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
        drawPlainTextScaled(m_display, m_inputBuffer, inputOriginX, inputY,
                            Display::WHITE, uiScale);
        metrics = {
            Display::textWidth(m_inputBuffer),
            FONT_CHAR_HEIGHT - 1,
            1,
        };
    }

    int resultX = DISPLAY_WIDTH
                  - scaledTextWidth(m_resultBuffer, uiScale) - MARGIN;
    if (resultX < 0) resultX = 0;
    const int resultY = (uiScale > 1)
        ? inputY + inputRowHeight - ENTRY_VERTICAL_PADDING - scaledFontHeight(uiScale)
        : inputY + 10;
    drawPlainTextScaled(m_display,
                        m_resultBuffer,
                        resultX,
                        resultY,
                        m_resultIsError ? Display::RED : Display::GREEN,
                        uiScale);

    drawCursor(inputOriginX, baselineY, expressionScale, drewMath);
}

void CalculatorApp::drawCursor(int originX,
                               int baselineY,
                               float expressionScale,
                               bool usedMathLayout) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const auto blinkMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    bool showCursor = ((blinkMs / 500) % 2) == 0;
    if (!showCursor) return;

    math_typeset::CursorMetrics cursorMetrics{};
    const bool hasCursorMetrics = usedMathLayout &&
        math_typeset::cursorMetrics(m_inputBuffer,
                                    m_cursorPos,
                                    expressionScale,
                                    cursorMetrics);

    int cursorX = originX;
    const int defaultCursorAscent =
        math_typeset::scaleLength(FONT_CHAR_HEIGHT - 1, expressionScale);
    const int defaultCursorDescent = math_typeset::scaleLength(1, expressionScale);
    int cursorTop = baselineY - defaultCursorAscent;
    int cursorHeight = std::max(2,
        defaultCursorAscent + defaultCursorDescent);

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
        const int cursorWidth = std::max(2, math_typeset::scaleLength(1, expressionScale));
        cursorTop = std::max(0, cursorTop);
        cursorHeight = std::min(cursorHeight, DISPLAY_HEIGHT - cursorTop);
        m_display.fillRect(cursorX, cursorTop, cursorWidth, cursorHeight, Display::WHITE);
    }
}

void CalculatorApp::drawScrollbar(int maxScroll, int viewportHeight) {
    if (viewportHeight <= 0) {
        return;
    }

    int scrollbarX = DISPLAY_WIDTH - 4;
    const int visibleHeight = visibleHistoryHeight(historySize(),
                                                   [this](int i) -> const HistoryEntry& { return historyAt(i); },
                                                   m_historyScroll,
                                                   viewportHeight,
                                                   m_config.uiScale);
    const int contentHeight = std::max(1,
        viewportHeight + inputEntryHeight("", m_config.uiScale) * maxScroll);
    int scrollbarHeight = std::max(8,
        (viewportHeight * std::max(1, visibleHeight)) / contentHeight);
    int scrollbarY = HISTORY_TOP
        + ((viewportHeight - scrollbarHeight) * m_historyScroll)
            / std::max(1, maxScroll);

    m_display.fillRect(scrollbarX, HISTORY_TOP, 3, viewportHeight,
                       COLOR_SCROLLBAR_BG);
    m_display.fillRect(scrollbarX, scrollbarY, 3, scrollbarHeight,
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

            m_display.fillRect(x, y, BTN_W, BTN_H, bgColor);

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

int CalculatorApp::historySize() const {
    return m_historyCount;
}

const HistoryEntry& CalculatorApp::historyAt(int index) const {
    const int slot = (m_historyStart + index) % MAX_HISTORY;
    return m_history[slot];
}
