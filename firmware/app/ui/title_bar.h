#pragma once

#include "app/ui/system_status.h"
#include "hal/display.h"

class SystemTitleBar {
public:
    static constexpr int kHeight = 22;

    void render(Display& display, const SystemStatusState& status) const;

private:
    static void drawTextFit(Display& display,
                            const char* text,
                            int x,
                            int y,
                            int maxWidth,
                            Color color);
};
