#pragma once

#include "hardware/flash.h"
#include "mi23_metadata.h"
#include "pico/stdlib.h"

#include <cstdint>

namespace RP2350FlashLayout {

constexpr uint32_t kExpectedFlashSize = 4u * 1024u * 1024u;

// Development layout for the Waveshare RP2350-PiZero board's 4 MiB flash:
// reserve the last 2 MiB for LittleFS. Pico firmware is linked from the bottom
// of flash, so this end-of-flash region avoids normal code/data growth. This is
// board-specific; final hardware should move this into a linker-enforced flash
// partition map and keep the UF2 image below kLittleFsOffset.
constexpr uint32_t kLittleFsSize = 2u * 1024u * 1024u;
constexpr uint32_t kLittleFsOffset = PICO_FLASH_SIZE_BYTES - kLittleFsSize;
constexpr uint32_t kLittleFsBlockSize = FLASH_SECTOR_SIZE;
constexpr uint32_t kLittleFsProgramSize = FLASH_PAGE_SIZE;
constexpr uint32_t kLittleFsBlockCount = kLittleFsSize / kLittleFsBlockSize;

// Two independent sectors preserve the previous valid settings record if power
// is lost while the inactive slot is erased or programmed.
constexpr uint32_t kSettingsSectorSize = FLASH_SECTOR_SIZE;
constexpr uint32_t kSettingsSlotCount = 2;
constexpr uint32_t kSettingsSectorOffset =
    kLittleFsOffset - kSettingsSlotCount * kSettingsSectorSize;

static_assert(kLittleFsOffset % FLASH_SECTOR_SIZE == 0u, "LittleFS offset must be sector aligned");
static_assert(kLittleFsSize % FLASH_SECTOR_SIZE == 0u, "LittleFS size must be sector aligned");
static_assert(kLittleFsProgramSize == FLASH_PAGE_SIZE, "Flash writes must be page sized");
static_assert(kSettingsSectorOffset % FLASH_SECTOR_SIZE == 0u, "Settings offset must be sector aligned");
static_assert(kSettingsSectorOffset + kSettingsSlotCount * kSettingsSectorSize <= kExpectedFlashSize,
              "Settings slots must fit within physical flash");
static_assert(PICO_FLASH_SIZE_BYTES == kExpectedFlashSize,
              "RP2350 AxiomFS layout expects 4 MiB external flash");
static_assert(PICO_FLASH_SIZE_BYTES > kLittleFsSize + kSettingsSlotCount * kSettingsSectorSize,
              "Flash layout needs room for firmware, settings, and LittleFS");
static_assert(MI23::Metadata::kFlashSizeBytes == kExpectedFlashSize,
              "RP2350 metadata flash size must match the flash layout");
static_assert(MI23::Metadata::kFilesystemOffsetBytes == kLittleFsOffset,
              "RP2350 metadata filesystem offset must match the flash layout");
static_assert(MI23::Metadata::kFilesystemSizeBytes == kLittleFsSize,
              "RP2350 metadata filesystem size must match the flash layout");

} // namespace RP2350FlashLayout
