#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace Companion {

struct SystemActionResult {
    bool accepted = false;
    std::string output;
    std::string errorMessage;

    static SystemActionResult acceptedWithOutput(std::string outputText) {
        SystemActionResult result;
        result.accepted = true;
        result.output = std::move(outputText);
        return result;
    }

    static SystemActionResult unsupported(std::string message) {
        SystemActionResult result;
        result.accepted = false;
        result.errorMessage = std::move(message);
        return result;
    }
};

class CompanionSystemActions {
public:
    virtual ~CompanionSystemActions() = default;

    virtual SystemActionResult requestReboot() = 0;
    virtual SystemActionResult requestBootloader() = 0;
    virtual void poll(uint64_t nowMs) {
        (void)nowMs;
    }
};

} // namespace Companion
