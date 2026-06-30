#include "platform/rp2350/settings_store_rp2350.h"

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

#include <array>
#include <cstring>

namespace {

constexpr uint32_t kSettingsSectorSize = FLASH_SECTOR_SIZE;
constexpr uint32_t kSettingsPageSize = FLASH_PAGE_SIZE;
constexpr uint32_t kSettingsOffset = PICO_FLASH_SIZE_BYTES - kSettingsSectorSize;

const uint8_t* flashAddress() {
    return reinterpret_cast<const uint8_t*>(XIP_BASE + kSettingsOffset);
}

} // namespace

bool RP2350SettingsStore::load(SettingsState& settings) {
    settings.resetToDefaults();

    std::array<uint8_t, SettingsStore::kSerializedSize> buffer{};
    std::memcpy(buffer.data(), flashAddress(), buffer.size());
    if (!SettingsStore::deserialize(buffer.data(), buffer.size(), settings)) {
        settings.resetToDefaults();
        return false;
    }

    return true;
}

bool RP2350SettingsStore::save(const SettingsState& settings) {
    std::array<uint8_t, kSettingsSectorSize> sectorBuffer{};
    sectorBuffer.fill(0xFFu);

    if (SettingsStore::serialize(settings,
                                 sectorBuffer.data(),
                                 SettingsStore::kSerializedSize) != SettingsStore::kSerializedSize) {
        return false;
    }

    if (std::memcmp(flashAddress(), sectorBuffer.data(), SettingsStore::kSerializedSize) == 0) {
        return true;
    }

    const uint32_t irqState = save_and_disable_interrupts();
    flash_range_erase(kSettingsOffset, kSettingsSectorSize);
    flash_range_program(kSettingsOffset, sectorBuffer.data(), kSettingsPageSize);
    restore_interrupts(irqState);
    return true;
}

bool RP2350SettingsStore::resetToDefaults(SettingsState& settings) {
    settings.resetToDefaults();
    settings.sanitize();
    return save(settings);
}
