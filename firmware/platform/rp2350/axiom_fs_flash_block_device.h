#pragma once

#include <cstddef>
#include <cstdint>

class RP2350FlashBlockDevice {
public:
    enum class LayoutError {
        None,
        OffsetOutOfBounds,
        RegionOutOfBounds,
        OffsetMisaligned,
        SizeMisaligned,
        BlockSizeInvalid,
        ProgramSizeInvalid,
        TooSmall,
        SettingsOverlap,
    };

    static bool isLayoutValid();
    static LayoutError validateLayout();
    static const char* layoutErrorToString(LayoutError error);
    static uint32_t detectedFlashSize();
    static uint32_t baseOffset();
    static uint32_t totalSize();
    static uint32_t blockSize();
    static uint32_t programSize();
    static uint32_t blockCount();
    static bool probe();
    static bool isRegionErased();
    static bool hasLittleFsMagic();

    static int read(uint32_t block, uint32_t offset, void* buffer, uint32_t size);
    static int program(uint32_t block, uint32_t offset, const void* buffer, uint32_t size);
    static int erase(uint32_t block);
    static int sync();
};
