#pragma once

#include <cstdint>

class Display;

namespace math_typeset {

struct LayoutMetrics {
    int width;
    int ascent;
    int descent;
};

bool measure(const char* expression, LayoutMetrics& outMetrics);
int measurePrefixWidth(const char* expression, int prefixLength);
bool draw(const char* expression, Display& display,
          int x, int baselineY, uint16_t color,
          LayoutMetrics* outMetrics = nullptr);

} // namespace math_typeset
