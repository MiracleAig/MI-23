#include "platform/rp2350/axiom_fs_flash_block_device.h"

#include "hardware/flash.h"
#include "pico/flash.h"
#include "platform/rp2350/axiom_fs_flash_config.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr uint32_t kMinimumLittleFsBlocks = 8;

bool rangeFits(uint32_t block, uint32_t offset, uint32_t size) {
    if (block >= RP2350FlashLayout::kLittleFsBlockCount) {
        return false;
    }
    if (offset > RP2350FlashLayout::kLittleFsBlockSize) {
        return false;
    }
    if (size > RP2350FlashLayout::kLittleFsBlockSize - offset) {
        return false;
    }
    return true;
}

uint32_t absoluteOffset(uint32_t block, uint32_t offset) {
    return RP2350FlashLayout::kLittleFsOffset
        + block * RP2350FlashLayout::kLittleFsBlockSize
        + offset;
}

struct FlashOperation {
    uint32_t offset;
    const uint8_t* data;
    uint32_t size;
};

void programFlash(void* parameter) {
    const auto* operation = static_cast<const FlashOperation*>(parameter);
    flash_range_program(operation->offset, operation->data, operation->size);
}

void eraseFlash(void* parameter) {
    const auto* operation = static_cast<const FlashOperation*>(parameter);
    flash_range_erase(operation->offset, operation->size);
}

const uint8_t* flashPtr(uint32_t offset) {
    return reinterpret_cast<const uint8_t*>(XIP_BASE + offset);
}

} // namespace

bool RP2350FlashBlockDevice::isLayoutValid() {
    return validateLayout() == LayoutError::None;
}

RP2350FlashBlockDevice::LayoutError RP2350FlashBlockDevice::validateLayout() {
    const uint64_t fsOffset = RP2350FlashLayout::kLittleFsOffset;
    const uint64_t fsSize = RP2350FlashLayout::kLittleFsSize;
    const uint64_t flashSize = detectedFlashSize();

    if (fsOffset >= flashSize) {
        return LayoutError::OffsetOutOfBounds;
    }
    if (fsSize > flashSize - fsOffset) {
        return LayoutError::RegionOutOfBounds;
    }
    if ((RP2350FlashLayout::kLittleFsOffset % FLASH_SECTOR_SIZE) != 0u) {
        return LayoutError::OffsetMisaligned;
    }
    if ((RP2350FlashLayout::kLittleFsSize % FLASH_SECTOR_SIZE) != 0u) {
        return LayoutError::SizeMisaligned;
    }
    if (RP2350FlashLayout::kLittleFsBlockSize != FLASH_SECTOR_SIZE) {
        return LayoutError::BlockSizeInvalid;
    }
    if (RP2350FlashLayout::kLittleFsProgramSize != FLASH_PAGE_SIZE) {
        return LayoutError::ProgramSizeInvalid;
    }
    if (RP2350FlashLayout::kLittleFsBlockCount < kMinimumLittleFsBlocks) {
        return LayoutError::TooSmall;
    }
    if (RP2350FlashLayout::kSettingsSectorOffset +
            RP2350FlashLayout::kSettingsSlotCount * RP2350FlashLayout::kSettingsSectorSize
        > RP2350FlashLayout::kLittleFsOffset) {
        return LayoutError::SettingsOverlap;
    }
    return LayoutError::None;
}

const char* RP2350FlashBlockDevice::layoutErrorToString(LayoutError error) {
    switch (error) {
        case LayoutError::None: return "ok";
        case LayoutError::OffsetOutOfBounds: return "filesystem offset is outside detected flash";
        case LayoutError::RegionOutOfBounds: return "filesystem offset + size exceeds detected flash";
        case LayoutError::OffsetMisaligned: return "filesystem offset is not sector aligned";
        case LayoutError::SizeMisaligned: return "filesystem size is not sector aligned";
        case LayoutError::BlockSizeInvalid: return "LittleFS block size does not match flash erase sector";
        case LayoutError::ProgramSizeInvalid: return "LittleFS program size does not match flash page size";
        case LayoutError::TooSmall: return "filesystem region is too small for LittleFS";
        case LayoutError::SettingsOverlap: return "settings sector overlaps filesystem region";
        default: return "unknown filesystem region error";
    }
}

uint32_t RP2350FlashBlockDevice::detectedFlashSize() {
    const uint32_t devInfoSize = flash_devinfo_size_to_bytes(flash_devinfo_get_cs_size(0));
    return devInfoSize != 0u ? devInfoSize : PICO_FLASH_SIZE_BYTES;
}

uint32_t RP2350FlashBlockDevice::baseOffset() {
    return RP2350FlashLayout::kLittleFsOffset;
}

uint32_t RP2350FlashBlockDevice::totalSize() {
    return RP2350FlashLayout::kLittleFsSize;
}

uint32_t RP2350FlashBlockDevice::blockSize() {
    return RP2350FlashLayout::kLittleFsBlockSize;
}

uint32_t RP2350FlashBlockDevice::programSize() {
    return RP2350FlashLayout::kLittleFsProgramSize;
}

uint32_t RP2350FlashBlockDevice::blockCount() {
    return RP2350FlashLayout::kLittleFsBlockCount;
}

bool RP2350FlashBlockDevice::probe() {
    if (!isLayoutValid()) {
        return false;
    }

    volatile uint8_t first = *flashPtr(RP2350FlashLayout::kLittleFsOffset);
    volatile uint8_t last = *flashPtr(RP2350FlashLayout::kLittleFsOffset
                                      + RP2350FlashLayout::kLittleFsSize - 1u);
    (void)first;
    (void)last;
    return true;
}

bool RP2350FlashBlockDevice::isRegionErased() {
    if (!isLayoutValid()) {
        return false;
    }

    const uint8_t* data = flashPtr(RP2350FlashLayout::kLittleFsOffset);
    for (uint32_t i = 0; i < RP2350FlashLayout::kLittleFsSize; i += FLASH_PAGE_SIZE) {
        for (uint32_t j = 0; j < FLASH_PAGE_SIZE; ++j) {
            if (data[i + j] != 0xFFu) {
                return false;
            }
        }
    }
    return true;
}

bool RP2350FlashBlockDevice::hasLittleFsMagic() {
    if (!isLayoutValid()) {
        return false;
    }

    constexpr char kMagic[] = "littlefs";
    constexpr uint32_t kMagicLength = sizeof(kMagic) - 1u;
    constexpr uint32_t kProbeSize = RP2350FlashLayout::kLittleFsBlockSize * 2u;
    const uint8_t* data = flashPtr(RP2350FlashLayout::kLittleFsOffset);
    for (uint32_t i = 0; i + kMagicLength <= kProbeSize; ++i) {
        bool matches = true;
        for (uint32_t j = 0; j < kMagicLength; ++j) {
            if (data[i + j] != static_cast<uint8_t>(kMagic[j])) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return true;
        }
    }
    return false;
}

int RP2350FlashBlockDevice::eraseRegion() {
    const LayoutError layoutError = validateLayout();
    if (layoutError != LayoutError::None) {
        std::printf("[fs][rp2350] erase-region blocked: filesystem region invalid: %s\n",
                    layoutErrorToString(layoutError));
        return -1;
    }
    if (!probe()) {
        std::printf("[fs][rp2350] erase-region blocked: flash storage probe failed\n");
        return -1;
    }

    std::printf("[fs][rp2350] erase-region start offset=%lu size=%lu block=%lu blocks=%lu\n",
                static_cast<unsigned long>(baseOffset()),
                static_cast<unsigned long>(totalSize()),
                static_cast<unsigned long>(blockSize()),
                static_cast<unsigned long>(blockCount()));
    for (uint32_t block = 0; block < blockCount(); ++block) {
        std::printf("[fs][rp2350] erase-region sector=%lu absolute_offset=%lu size=%lu\n",
                    static_cast<unsigned long>(block),
                    static_cast<unsigned long>(absoluteOffset(block, 0)),
                    static_cast<unsigned long>(blockSize()));
        const int result = erase(block);
        if (result != 0) {
            std::printf("[fs][rp2350] erase-region failed sector=%lu result=%d\n",
                        static_cast<unsigned long>(block),
                        result);
            return result;
        }
    }
    std::printf("[fs][rp2350] erase-region complete\n");
    return 0;
}

int RP2350FlashBlockDevice::read(uint32_t block, uint32_t offset, void* buffer, uint32_t size) {
    if (!buffer || !rangeFits(block, offset, size)) {
        return -1;
    }

    const uint8_t* source = reinterpret_cast<const uint8_t*>(XIP_BASE + absoluteOffset(block, offset));
    std::memcpy(buffer, source, size);
    return 0;
}

int RP2350FlashBlockDevice::program(uint32_t block, uint32_t offset, const void* buffer, uint32_t size) {
    if (!buffer || !rangeFits(block, offset, size)) {
        return -1;
    }
    if (offset % FLASH_PAGE_SIZE != 0u || size % FLASH_PAGE_SIZE != 0u) {
        return -1;
    }

    FlashOperation operation{absoluteOffset(block, offset),
                             static_cast<const uint8_t*>(buffer), size};
    return flash_safe_execute(programFlash, &operation, 1000u) == PICO_OK ? 0 : -1;
}

int RP2350FlashBlockDevice::erase(uint32_t block) {
    if (block >= RP2350FlashLayout::kLittleFsBlockCount) {
        return -1;
    }

    FlashOperation operation{absoluteOffset(block, 0), nullptr,
                             RP2350FlashLayout::kLittleFsBlockSize};
    return flash_safe_execute(eraseFlash, &operation, 1000u) == PICO_OK ? 0 : -1;
}

int RP2350FlashBlockDevice::sync() {
    return 0;
}
