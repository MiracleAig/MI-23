#include "app/ui/title_bar.h"

#include "graphics/font.h"

#include <cstdio>
#include <cstring>

namespace {

const Color COLOR_BG = Display::rgb(22, 35, 48);
const Color COLOR_TEXT = Display::WHITE;
const Color COLOR_MUTED = Display::rgb(185, 195, 206);
const Color COLOR_SEPARATOR = Display::rgb(70, 86, 102);
const Color COLOR_BATTERY = Display::rgb(105, 225, 145);

constexpr int PADDING_X = 8;
constexpr int TEXT_Y = 7;
constexpr int SECTION_GAP = 7;
constexpr int BATTERY_ICON_W = 18;
constexpr int BATTERY_ICON_H = 9;
constexpr int BATTERY_ICON_GAP = 4;

int stringLength(const char* text) {
    if (!text) {
        return 0;
    }
    int length = 0;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

void appendText(char* buffer, int capacity, const char* text) {
    if (!buffer || capacity <= 0 || !text) {
        return;
    }

    const int used = stringLength(buffer);
    if (used >= capacity - 1) {
        return;
    }

    std::strncpy(buffer + used, text, static_cast<size_t>(capacity - used - 1));
    buffer[capacity - 1] = '\0';
}

void drawVerticalSeparator(Display& display, int x) {
    display.fillRect(x, 5, 1, SystemTitleBar::kHeight - 10, COLOR_SEPARATOR);
}

void drawBatteryIcon(Display& display, int x, int y, int percentage) {
    display.drawRect(x, y, BATTERY_ICON_W, BATTERY_ICON_H, COLOR_MUTED);
    display.fillRect(x + BATTERY_ICON_W, y + 3, 2, 3, COLOR_MUTED);

    const int fillW = ((BATTERY_ICON_W - 4) * percentage) / 100;
    if (fillW > 0) {
        display.fillRect(x + 2, y + 2, fillW, BATTERY_ICON_H - 4, COLOR_BATTERY);
    }
}

} // namespace

void SystemTitleBar::render(Display& display, const SystemStatusState& status) const {
    display.fillRect(0, 0, DISPLAY_WIDTH, kHeight, COLOR_BG);
    display.fillRect(0, kHeight - 1, DISPLAY_WIDTH, 1, COLOR_SEPARATOR);

    char battery[8] = {};
    std::snprintf(battery, sizeof(battery), "%d%%", status.batteryPercentage());

    const char* angle = compactAngleModeLabel(status.angleMode());
    const char* layer = inputLayerLabel(status.inputLayer());

    const int layerW = Display::textWidth(layer);
    const int angleW = Display::textWidth(angle);
    const int batteryW = Display::textWidth(battery);

    const int layerX = DISPLAY_WIDTH - PADDING_X - layerW;
    const int separator2X = layerX - SECTION_GAP;
    const int angleX = separator2X - SECTION_GAP - angleW;
    const int separator1X = angleX - SECTION_GAP;
    const int batteryTextX = separator1X - SECTION_GAP - batteryW;
    const int batteryIconX = batteryTextX - BATTERY_ICON_GAP - BATTERY_ICON_W - 2;
    const int rightStartX = batteryIconX;

    drawBatteryIcon(display,
                    batteryIconX,
                    (kHeight - BATTERY_ICON_H) / 2,
                    status.batteryPercentage());
    display.drawText(battery, batteryTextX, TEXT_Y, COLOR_TEXT);
    drawVerticalSeparator(display, separator1X);
    display.drawText(angle, angleX, TEXT_Y, COLOR_TEXT);
    drawVerticalSeparator(display, separator2X);
    display.drawText(layer, layerX, TEXT_Y, COLOR_TEXT);

    char left[48] = {};
    appendText(left, sizeof(left), "MI-23 | ");
    appendText(left, sizeof(left), status.appTitle());

    drawTextFit(display,
                left,
                PADDING_X,
                TEXT_Y,
                rightStartX - PADDING_X - SECTION_GAP,
                COLOR_TEXT);
}

void SystemTitleBar::drawTextFit(Display& display,
                                 const char* text,
                                 int x,
                                 int y,
                                 int maxWidth,
                                 Color color) {
    if (!text || maxWidth <= 0) {
        return;
    }

    char buffer[48] = {};
    const int maxChars = maxWidth / FONT_CHAR_ADVANCE;
    if (maxChars <= 0) {
        return;
    }

    int count = 0;
    while (text[count] != '\0' &&
           count < maxChars &&
           count < static_cast<int>(sizeof(buffer)) - 1) {
        buffer[count] = text[count];
        ++count;
    }

    if (text[count] != '\0' && count >= 3) {
        buffer[count - 3] = '.';
        buffer[count - 2] = '.';
        buffer[count - 1] = '.';
    }
    buffer[count] = '\0';

    display.drawText(buffer, x, y, color);
}
