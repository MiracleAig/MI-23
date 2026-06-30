#pragma once

#include <cstdint>

#if __has_include("pico/time.h")
#include "pico/time.h"
#else
#include <chrono>
#endif

inline uint64_t systemTimeMs() {
#if __has_include("pico/time.h")
    return static_cast<uint64_t>(to_ms_since_boot(get_absolute_time()));
#else
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
#endif
}
