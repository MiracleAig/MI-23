#pragma once

#include "core/companion/CompanionSystemActions.h"

#include <cstdint>

class RP2350CompanionSystemActions : public Companion::CompanionSystemActions {
public:
    Companion::SystemActionResult requestReboot() override;
    Companion::SystemActionResult requestBootloader() override;
    void poll(uint64_t nowMs) override;

private:
    enum class PendingAction {
        None,
        Reboot,
        Bootloader,
    };

    static constexpr uint64_t kResetDelayMs = 250;

    PendingAction m_pendingAction = PendingAction::None;
    uint64_t m_dueMs = 0;

    Companion::SystemActionResult schedule(PendingAction action, const char* output);
};
