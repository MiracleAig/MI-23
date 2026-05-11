#pragma once

#include <cstdint>

class Display;

namespace math_typeset {

struct LayoutMetrics {
    int width;
    int ascent;
    int descent;
};

struct CursorMetrics {
    int x;
    int baselineOffset;
    int ascent;
    int descent;
};

enum class CursorMove {
    Left,
    Right,
    Up,
    Down,
};

bool measure(const char* expression, LayoutMetrics& outMetrics);
int measurePrefixWidth(const char* expression, int prefixLength);
bool cursorMetrics(const char* expression, int cursorIndex, float scale,
                   CursorMetrics& outMetrics);
int moveCursor(const char* expression, int cursorIndex, CursorMove move);
int scaleLength(int value, float scale);
bool draw(const char* expression, Display& display,
          int x, int baselineY, uint16_t color,
          LayoutMetrics* outMetrics = nullptr);
bool drawScaled(const char* expression, Display& display,
                int x, int baselineY, uint16_t color, float scale,
                LayoutMetrics* outMetrics = nullptr);

} // namespace math_typeset
