#include "platform/rp2350/companion_system_actions_rp2350.h"

#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"

namespace {

constexpr uint32_t kWatchdogRebootDelayMs = 10;

} // namespace

Companion::SystemActionResult RP2350CompanionSystemActions::requestReboot() {
    return schedule(PendingAction::Reboot, "reboot scheduled\n");
}

Companion::SystemActionResult RP2350CompanionSystemActions::requestBootloader() {
    return schedule(PendingAction::Bootloader, "bootloader reboot scheduled\n");
}

void RP2350CompanionSystemActions::poll(uint64_t nowMs) {
    if (m_pendingAction == PendingAction::None || nowMs < m_dueMs) {
        return;
    }

    const PendingAction action = m_pendingAction;
    m_pendingAction = PendingAction::None;

    // The protocol handler sends a JSON success response before this poll hook
    // runs. Flush stdio and wait briefly so the USB CDC response has a chance
    // to leave the device before the requested reset tears down USB.
    stdio_flush();
    sleep_ms(20);

    if (action == PendingAction::Bootloader) {
        // Pico SDK 2.2.0 maps this no-return wrapper to the RP2350 boot ROM
        // BOOTSEL reboot path with both MSD and Picoboot interfaces enabled.
        reset_usb_boot(0, 0);
    }

    watchdog_reboot(0, 0, kWatchdogRebootDelayMs);
    while (true) {
        tight_loop_contents();
    }
}

Companion::SystemActionResult RP2350CompanionSystemActions::schedule(PendingAction action,
                                                                     const char* output) {
    if (m_pendingAction == action) {
        return Companion::SystemActionResult::acceptedWithOutput(
            action == PendingAction::Bootloader
                ? "bootloader reboot already scheduled\n"
                : "reboot already scheduled\n",
            true);
    }
    if (m_pendingAction != PendingAction::None) {
        return Companion::SystemActionResult::busy("Another reboot action is already scheduled.");
    }

    m_pendingAction = action;
    m_dueMs = to_ms_since_boot(get_absolute_time()) + kResetDelayMs;
    return Companion::SystemActionResult::acceptedWithOutput(output ? output : "");
}
