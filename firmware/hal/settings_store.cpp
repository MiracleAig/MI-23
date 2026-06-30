#include "hal/settings_store.h"

#include <cstring>

namespace {

struct SettingsRecord {
    uint32_t magic;
    uint32_t version;
    uint32_t payloadSize;
    uint32_t crc32;
    uint8_t angleMode;
    uint8_t graphGrid;
    uint8_t graphAxes;
    uint8_t graphResolution;
    uint8_t theme;
    uint8_t uiScale;
    uint8_t calculatorPrecision;
    uint8_t showTouchRegions;
    uint8_t showCursorPosition;
    uint8_t showGraphBounds;
    uint8_t parserLogs;
    uint8_t inputEventLogs;
    uint8_t reserved[4];
};

static_assert(sizeof(SettingsRecord) == SettingsStore::kSerializedSize,
              "SettingsRecord size must remain stable");

constexpr std::size_t kPayloadOffset = offsetof(SettingsRecord, angleMode);

uint32_t crc32(const uint8_t* data, std::size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            const uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1u) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

SettingsRecord makeRecord(const SettingsState& input) {
    SettingsState settings = input;
    settings.sanitize();

    SettingsRecord record{};
    record.magic = SettingsStore::kMagic;
    record.version = SettingsStore::kVersion;
    record.payloadSize = sizeof(SettingsRecord) - kPayloadOffset;
    record.angleMode = static_cast<uint8_t>(settings.angleMode);
    record.graphGrid = settings.graphGrid ? 1u : 0u;
    record.graphAxes = settings.graphAxes ? 1u : 0u;
    record.graphResolution = static_cast<uint8_t>(settings.graphResolution);
    record.theme = static_cast<uint8_t>(settings.theme);
    record.uiScale = static_cast<uint8_t>(settings.uiScale);
    record.calculatorPrecision = static_cast<uint8_t>(settings.calculatorPrecision);
    record.showTouchRegions = settings.developer.showTouchRegions ? 1u : 0u;
    record.showCursorPosition = settings.developer.showCursorPosition ? 1u : 0u;
    record.showGraphBounds = settings.developer.showGraphBounds ? 1u : 0u;
    record.parserLogs = settings.developer.parserLogs ? 1u : 0u;
    record.inputEventLogs = settings.developer.inputEventLogs ? 1u : 0u;
    record.crc32 = crc32(reinterpret_cast<const uint8_t*>(&record) + kPayloadOffset,
                         record.payloadSize);
    return record;
}

} // namespace

bool SettingsStore::deserialize(const uint8_t* data,
                                std::size_t size,
                                SettingsState& settings) {
    settings.resetToDefaults();

    if (!data || size != sizeof(SettingsRecord)) {
        return false;
    }

    SettingsRecord record{};
    std::memcpy(&record, data, sizeof(record));

    if (record.magic != kMagic ||
        record.version != kVersion ||
        record.payloadSize != sizeof(SettingsRecord) - kPayloadOffset) {
        return false;
    }

    const uint32_t expectedCrc = crc32(reinterpret_cast<const uint8_t*>(&record) + kPayloadOffset,
                                       record.payloadSize);
    if (record.crc32 != expectedCrc) {
        return false;
    }

    settings.angleMode = static_cast<AngleMode>(record.angleMode);
    settings.graphGrid = record.graphGrid != 0u;
    settings.graphAxes = record.graphAxes != 0u;
    settings.graphResolution = static_cast<GraphResolution>(record.graphResolution);
    settings.theme = static_cast<ThemeMode>(record.theme);
    settings.uiScale = static_cast<UiScaleMode>(record.uiScale);
    settings.calculatorPrecision = static_cast<int>(record.calculatorPrecision);
    settings.developer.showTouchRegions = record.showTouchRegions != 0u;
    settings.developer.showCursorPosition = record.showCursorPosition != 0u;
    settings.developer.showGraphBounds = record.showGraphBounds != 0u;
    settings.developer.parserLogs = record.parserLogs != 0u;
    settings.developer.inputEventLogs = record.inputEventLogs != 0u;
    settings.sanitize();
    return true;
}

std::size_t SettingsStore::serialize(const SettingsState& settings,
                                     uint8_t* out,
                                     std::size_t size) {
    if (!out || size < sizeof(SettingsRecord)) {
        return 0;
    }

    const SettingsRecord record = makeRecord(settings);
    std::memcpy(out, &record, sizeof(record));
    return sizeof(record);
}
