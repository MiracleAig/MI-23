#include "platform/host/companion_system_actions_host.h"

#include "hal/system_time.h"

#include <cstdio>

Companion::SystemActionResult HostCompanionSystemActions::requestReboot() {
    return schedule(PendingAction::Reboot, systemTimeMs(), "host mock reboot scheduled\n");
}

Companion::SystemActionResult HostCompanionSystemActions::requestBootloader() {
    m_bootloaderRequestCount++;
    return schedule(PendingAction::Bootloader, systemTimeMs(), "host mock BOOTSEL reboot scheduled\n");
}

void HostCompanionSystemActions::poll(uint64_t nowMs) {
    if (m_pendingAction == PendingAction::None || nowMs < m_dueMs) {
        return;
    }

    const PendingAction action = m_pendingAction;
    m_pendingAction = PendingAction::None;

    if (action == PendingAction::Bootloader) {
        m_bootloaderRequested = true;
        m_bootloaderMockEntryCount++;
        std::printf("[companion][host] mock BOOTSEL reboot requested; simulator stays running\n");
        return;
    }

    m_rebootRequested = true;
    std::printf("[companion][host] mock reboot requested; simulator stays running\n");
}

bool HostCompanionSystemActions::rebootRequested() const {
    return m_rebootRequested;
}

bool HostCompanionSystemActions::bootloaderRequested() const {
    return m_bootloaderRequested;
}

bool HostCompanionSystemActions::bootloaderPending() const {
    return m_pendingAction == PendingAction::Bootloader;
}

int HostCompanionSystemActions::bootloaderRequestCount() const {
    return m_bootloaderRequestCount;
}

int HostCompanionSystemActions::bootloaderMockEntryCount() const {
    return m_bootloaderMockEntryCount;
}

Companion::SystemActionResult HostCompanionSystemActions::schedule(PendingAction action,
                                                                   uint64_t nowMs,
                                                                   const char* output) {
    if (m_pendingAction == action) {
        return Companion::SystemActionResult::acceptedWithOutput(
            action == PendingAction::Bootloader
                ? "host mock BOOTSEL reboot already scheduled\n"
                : "host mock reboot already scheduled\n",
            true);
    }
    if (m_pendingAction != PendingAction::None) {
        return Companion::SystemActionResult::busy("Another host mock reboot action is already scheduled.");
    }

    m_pendingAction = action;
    m_dueMs = nowMs + kMockDelayMs;
    return Companion::SystemActionResult::acceptedWithOutput(output ? output : "");
}
