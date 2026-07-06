#pragma once

#include "core/companion/CompanionProtocol.h"
#include "hal/usb_cdc_transport.h"

#include <cstdint>
#include <string>

namespace Companion {

class CompanionSession {
public:
    CompanionSession(UsbCdcTransport& transport, CompanionProtocol& protocol);

    void enter(uint64_t nowMs);
    void poll(uint64_t nowMs);
    bool isConnected() const;

private:
    static constexpr std::size_t kMaxLineLength = 96;
    static constexpr uint64_t kLineTimeoutMs = 5000;

    UsbCdcTransport& m_transport;
    CompanionProtocol& m_protocol;
    std::string m_line;
    uint64_t m_lineStartedAtMs;
    bool m_discardingLine;

    void handleByte(uint8_t byte, uint64_t nowMs);
    void processLine();
    void sendResponse(const std::string& response);
};

} // namespace Companion
