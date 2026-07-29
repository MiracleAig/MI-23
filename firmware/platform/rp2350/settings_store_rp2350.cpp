#include "platform/rp2350/settings_store_rp2350.h"

#include "hardware/flash.h"
#include "pico/flash.h"
#include "pico/stdlib.h"
#include "platform/rp2350/axiom_fs_flash_config.h"

#include <array>
#include <cstddef>
#include <cstring>

namespace {

constexpr uint32_t kSettingsPageSize = FLASH_PAGE_SIZE;
constexpr uint32_t kSlotMagic = 0x4D493253u; // "MI2S"

struct SlotRecord {
    uint32_t magic;
    uint32_t sequence;
    std::array<uint8_t, SettingsStore::kSerializedSize> payload;
    uint32_t checksum;
};
static_assert(sizeof(SlotRecord) <= FLASH_PAGE_SIZE, "Settings slot must fit one flash page");

uint32_t checksum(const uint8_t* data, std::size_t size) {
    uint32_t hash = 2166136261u;
    for (std::size_t i = 0; i < size; ++i) hash = (hash ^ data[i]) * 16777619u;
    return hash;
}

bool settingsRangeIsValid() {
    const uint32_t detectedSize =
        flash_devinfo_size_to_bytes(flash_devinfo_get_cs_size(0));
    const uint32_t flashSize = detectedSize != 0u ? detectedSize : PICO_FLASH_SIZE_BYTES;
    const uint64_t settingsEnd =
        static_cast<uint64_t>(RP2350FlashLayout::kSettingsSectorOffset)
        + RP2350FlashLayout::kSettingsSlotCount * RP2350FlashLayout::kSettingsSectorSize;
    return settingsEnd <= flashSize;
}

uint32_t slotOffset(uint32_t slot) {
    return RP2350FlashLayout::kSettingsSectorOffset +
           slot * RP2350FlashLayout::kSettingsSectorSize;
}

const uint8_t* flashAddress(uint32_t slot) {
    return reinterpret_cast<const uint8_t*>(XIP_BASE + slotOffset(slot));
}

bool readSlot(uint32_t slot, SettingsState& settings, uint32_t& sequence) {
    SlotRecord record{};
    std::memcpy(&record, flashAddress(slot), sizeof(record));
    if (record.magic != kSlotMagic ||
        record.checksum != checksum(reinterpret_cast<const uint8_t*>(&record),
                                    offsetof(SlotRecord, checksum)) ||
        !SettingsStore::deserialize(record.payload.data(), record.payload.size(), settings)) {
        return false;
    }
    sequence = record.sequence;
    return true;
}

struct FlashWrite {
    uint32_t offset;
    const uint8_t* page;
};

void programSettingsSlot(void* parameter) {
    const auto* write = static_cast<const FlashWrite*>(parameter);
    flash_range_erase(write->offset, RP2350FlashLayout::kSettingsSectorSize);
    flash_range_program(write->offset, write->page, kSettingsPageSize);
}

} // namespace

bool RP2350SettingsStore::load(SettingsState& settings) {
    const LoadResult result = loadDetailed(settings);
    return result == LoadResult::ValidRecord || result == LoadResult::LegacyRecord;
}

RP2350SettingsStore::LoadResult RP2350SettingsStore::loadDetailed(SettingsState& settings) {
    settings.resetToDefaults();

    // Loading settings happens before the broader storage-layout check during
    // boot. Never dereference an XIP address until it is known to be inside the
    // physical flash reported by the RP2350 boot ROM.
    if (!settingsRangeIsValid()) {
        return LoadResult::LayoutMismatch;
    }

    SettingsState slotSettings[2];
    uint32_t sequences[2] = {};
    const bool valid0 = readSlot(0, slotSettings[0], sequences[0]);
    const bool valid1 = readSlot(1, slotSettings[1], sequences[1]);
    if (valid0 || valid1) {
        const uint32_t selected = valid1 && (!valid0 ||
            static_cast<int32_t>(sequences[1] - sequences[0]) > 0) ? 1u : 0u;
        settings = slotSettings[selected];
        return LoadResult::ValidRecord;
    }

    // Legacy firmware stored its bare serialized record in the sector nearest
    // LittleFS, which is slot 1 in the two-slot layout.
    std::array<uint8_t, SettingsStore::kSerializedSize> legacy{};
    std::memcpy(legacy.data(), flashAddress(1), legacy.size());
    if (SettingsStore::deserialize(legacy.data(), legacy.size(), settings)) {
        return LoadResult::LegacyRecord;
    }
    settings.resetToDefaults();
    return LoadResult::MissingOrInvalid;
}

bool RP2350SettingsStore::save(const SettingsState& settings) {
    if (!settingsRangeIsValid()) {
        return false;
    }

    SettingsState ignored;
    uint32_t sequences[2] = {};
    const bool valid0 = readSlot(0, ignored, sequences[0]);
    const bool valid1 = readSlot(1, ignored, sequences[1]);
    const uint32_t active = valid1 && (!valid0 ||
        static_cast<int32_t>(sequences[1] - sequences[0]) > 0) ? 1u : 0u;
    const uint32_t target = (valid0 || valid1) ? 1u - active : 0u;
    const uint32_t nextSequence = (valid0 || valid1) ? sequences[active] + 1u : 1u;

    std::array<uint8_t, kSettingsPageSize> page{};
    page.fill(0xFFu);
    SlotRecord record{};
    record.magic = kSlotMagic;
    record.sequence = nextSequence;
    if (SettingsStore::serialize(settings, record.payload.data(), record.payload.size()) !=
        record.payload.size()) {
        return false;
    }
    record.checksum = checksum(reinterpret_cast<const uint8_t*>(&record), offsetof(SlotRecord, checksum));
    std::memcpy(page.data(), &record, sizeof(record));

    const FlashWrite write{slotOffset(target), page.data()};
    if (flash_safe_execute(programSettingsSlot, const_cast<FlashWrite*>(&write), 1000u) != PICO_OK) {
        return false;
    }
    SettingsState verified;
    uint32_t verifiedSequence = 0;
    return readSlot(target, verified, verifiedSequence) && verifiedSequence == nextSequence;
}

bool RP2350SettingsStore::resetToDefaults(SettingsState& settings) {
    settings.resetToDefaults();
    settings.sanitize();
    return save(settings);
}
