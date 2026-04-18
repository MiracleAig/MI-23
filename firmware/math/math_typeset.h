#pragma once

#include <cstdint>

class Display;

namespace math_typeset {

enum class CursorMove {
    Left,
    Right,
    Up,
    Down,
};

struct LayoutMetrics {
    int width;
    int ascent;
    int descent;
};

bool measure(const char* expression, LayoutMetrics& outMetrics);
int measurePrefixWidth(const char* expression, int prefixLength);
int moveCursor(const char* expression, int cursorPosition, CursorMove move);
bool draw(const char* expression, Display& display,
          int x, int baselineY, uint16_t color,
          LayoutMetrics* outMetrics = nullptr);

} // namespace math_typeset
