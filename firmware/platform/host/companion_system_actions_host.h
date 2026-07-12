#pragma once

#include "core/companion/CompanionSystemActions.h"

#include <cstdint>

class HostCompanionSystemActions : public Companion::CompanionSystemActions {
public:
    Companion::SystemActionResult requestReboot() override;
    Companion::SystemActionResult requestBootloader() override;
    void poll(uint64_t nowMs) override;

    bool rebootRequested() const;
    bool bootloaderRequested() const;
    bool bootloaderPending() const;
    int bootloaderRequestCount() const;
    int bootloaderMockEntryCount() const;

private:
    enum class PendingAction {
        None,
        Reboot,
        Bootloader,
    };

    static constexpr uint64_t kMockDelayMs = 250;

    PendingAction m_pendingAction = PendingAction::None;
    uint64_t m_dueMs = 0;
    bool m_rebootRequested = false;
    bool m_bootloaderRequested = false;
    int m_bootloaderRequestCount = 0;
    int m_bootloaderMockEntryCount = 0;

    Companion::SystemActionResult schedule(PendingAction action, uint64_t nowMs, const char* output);
};
