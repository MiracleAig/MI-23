#include "platform/rp2350/settings_store_rp2350.h"

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "platform/rp2350/axiom_fs_flash_config.h"

#include <array>
#include <cstring>

namespace {

constexpr uint32_t kSettingsPageSize = FLASH_PAGE_SIZE;

bool settingsRangeIsValid() {
    const uint32_t detectedSize =
        flash_devinfo_size_to_bytes(flash_devinfo_get_cs_size(0));
    const uint32_t flashSize = detectedSize != 0u ? detectedSize : PICO_FLASH_SIZE_BYTES;
    const uint64_t settingsEnd =
        static_cast<uint64_t>(RP2350FlashLayout::kSettingsSectorOffset)
        + RP2350FlashLayout::kSettingsSectorSize;
    return settingsEnd <= flashSize;
}

const uint8_t* flashAddress() {
    return reinterpret_cast<const uint8_t*>(XIP_BASE + RP2350FlashLayout::kSettingsSectorOffset);
}

} // namespace

bool RP2350SettingsStore::load(SettingsState& settings) {
    settings.resetToDefaults();

    // Loading settings happens before the broader storage-layout check during
    // boot. Never dereference an XIP address until it is known to be inside the
    // physical flash reported by the RP2350 boot ROM.
    if (!settingsRangeIsValid()) {
        return false;
    }

    std::array<uint8_t, SettingsStore::kSerializedSize> buffer{};
    std::memcpy(buffer.data(), flashAddress(), buffer.size());
    if (!SettingsStore::deserialize(buffer.data(), buffer.size(), settings)) {
        settings.resetToDefaults();
        return false;
    }

    return true;
}

bool RP2350SettingsStore::save(const SettingsState& settings) {
    if (!settingsRangeIsValid()) {
        return false;
    }

    std::array<uint8_t, RP2350FlashLayout::kSettingsSectorSize> sectorBuffer{};
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
    flash_range_erase(RP2350FlashLayout::kSettingsSectorOffset,
                      RP2350FlashLayout::kSettingsSectorSize);
    flash_range_program(RP2350FlashLayout::kSettingsSectorOffset,
                        sectorBuffer.data(),
                        kSettingsPageSize);
    restore_interrupts(irqState);
    return true;
}

bool RP2350SettingsStore::resetToDefaults(SettingsState& settings) {
    settings.resetToDefaults();
    settings.sanitize();
    return save(settings);
}
