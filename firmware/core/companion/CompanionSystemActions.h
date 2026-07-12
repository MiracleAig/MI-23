#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace Companion {

struct SystemActionResult {
    bool accepted = false;
    std::string output;
    std::string errorCode;
    std::string errorMessage;
    bool alreadyPending = false;

    static SystemActionResult acceptedWithOutput(std::string outputText, bool alreadyPendingValue = false) {
        SystemActionResult result;
        result.accepted = true;
        result.output = std::move(outputText);
        result.alreadyPending = alreadyPendingValue;
        return result;
    }

    static SystemActionResult rejected(std::string code, std::string message) {
        SystemActionResult result;
        result.accepted = false;
        result.errorCode = std::move(code);
        result.errorMessage = std::move(message);
        return result;
    }

    static SystemActionResult unsupported(std::string message) {
        return rejected("unsupported", std::move(message));
    }

    static SystemActionResult busy(std::string message) {
        return rejected("busy", std::move(message));
    }

    static SystemActionResult ioError(std::string message) {
        return rejected("io_error", std::move(message));
    }

    static SystemActionResult internalError(std::string message) {
        return rejected("internal_error", std::move(message));
    }

    const char* effectiveErrorCode() const {
        if (!errorCode.empty()) {
            return errorCode.c_str();
        }
        return "unsupported";
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
