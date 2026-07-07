#include "core/companion/CompanionSession.h"

#include <cstdarg>
#include <cstdio>

#ifndef MI23_COMPANION_DEBUG_LOGS
#define MI23_COMPANION_DEBUG_LOGS 0
#endif

namespace Companion {

namespace {

void logCompanion(const char* format, ...) {
#if MI23_COMPANION_DEBUG_LOGS
    va_list args;
    va_start(args, format);
    std::vprintf(format, args);
    va_end(args);
#else
    (void)format;
#endif
}

} // namespace

CompanionSession::CompanionSession(UsbCdcTransport& transport, CompanionProtocol& protocol)
    : m_transport(transport)
    , m_protocol(protocol)
    , m_line()
    , m_lineStartedAtMs(0)
    , m_discardingLine(false) {}

void CompanionSession::enter(uint64_t nowMs) {
    m_line.clear();
    m_lineStartedAtMs = nowMs;
    m_discardingLine = false;
    logCompanion("[companion] companion mode entered\n");
}

void CompanionSession::poll(uint64_t nowMs) {
    if (!m_line.empty() && nowMs - m_lineStartedAtMs > kLineTimeoutMs) {
        logCompanion("[companion] malformed command: line timeout\n");
        m_line.clear();
        m_discardingLine = false;
    }

    uint8_t buffer[32] = {};
    int count = 0;
    do {
        count = m_transport.read(buffer, sizeof(buffer));
        for (int i = 0; i < count; ++i) {
            handleByte(buffer[i], nowMs);
        }
    } while (count == static_cast<int>(sizeof(buffer)));

    m_protocol.pollSystemActions(nowMs);
}

bool CompanionSession::isConnected() const {
    return m_transport.isConnected();
}

void CompanionSession::handleByte(uint8_t byte, uint64_t nowMs) {
    if (byte == '\r') {
        return;
    }

    if (byte == '\n') {
        if (m_discardingLine) {
            m_discardingLine = false;
            m_line.clear();
            sendResponse("{\"id\":0,\"ok\":false,\"error\":{\"code\":\"invalid_argument\",\"message\":\"Request is too long\"}}\n");
            return;
        }
        processLine();
        return;
    }

    if (m_line.empty()) {
        m_lineStartedAtMs = nowMs;
    }

    if (m_discardingLine) {
        return;
    }

    if (m_line.size() >= kMaxLineLength) {
        logCompanion("[companion] malformed command: line too long\n");
        m_line.clear();
        m_discardingLine = true;
        return;
    }

    m_line.push_back(static_cast<char>(byte));
}

void CompanionSession::processLine() {
    if (m_line.empty()) {
        return;
    }

    logCompanion("[companion] command received: %s\n", m_line.c_str());

    std::string response;
    (void)m_protocol.handleCommand(m_line, response);
    m_line.clear();
    sendResponse(response);
}

void CompanionSession::sendResponse(const std::string& response) {
    if (response.empty()) {
        return;
    }

    static constexpr std::size_t kWriteChunkSize = 128;
    bool ok = true;
    std::size_t written = 0;
    while (written < response.size()) {
        const std::size_t remaining = response.size() - written;
        const std::size_t chunk = remaining < kWriteChunkSize ? remaining : kWriteChunkSize;
        if (!m_transport.write(reinterpret_cast<const uint8_t*>(response.data() + written), chunk)) {
            ok = false;
            break;
        }
        written += chunk;
    }

    if (ok) {
        logCompanion("[companion] response sent: %u bytes\n",
                     static_cast<unsigned>(response.size()));
    } else {
        logCompanion("[companion] response send failed: %u bytes\n",
                     static_cast<unsigned>(response.size()));
    }
}

} // namespace Companion
