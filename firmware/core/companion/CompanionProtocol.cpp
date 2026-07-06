#include "core/companion/CompanionProtocol.h"

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

bool containsWhitespace(const std::string& text) {
    for (const char ch : text) {
        if (ch == ' ' || ch == '\t') {
            return true;
        }
    }
    return false;
}

const char* filesystemProtocolName(const char* backendName) {
    if (!backendName) {
        return "unknown";
    }
    return "littlefs";
}

} // namespace

CompanionProtocol::CompanionProtocol(AxiomFS::FileSystem& filesystem, DeviceInfo deviceInfo)
    : m_filesystem(filesystem)
    , m_deviceInfo(deviceInfo) {}

bool CompanionProtocol::handleCommand(const std::string& command, std::string& response) {
    response.clear();

    if (command.empty() || containsWhitespace(command)) {
        logCompanion("[companion] malformed command: '%s'\n", command.c_str());
        response = "ERR MALFORMED\n";
        return false;
    }

    if (command == "PING") {
        response = "OK PONG\n";
        return true;
    }

    if (command == "HELLO") {
        appendHello(response);
        return true;
    }

    if (command == "INFO") {
        appendInfo(response);
        return true;
    }

    logCompanion("[companion] malformed command: unknown '%s'\n", command.c_str());
    response = "ERR UNKNOWN_COMMAND\n";
    return false;
}

void CompanionProtocol::appendHello(std::string& response) const {
    response += "OK MIRACLE_PROTOCOL 1\n";
    response += "DEVICE_TYPE calculator\n";
    response += "MODEL MI-23\n";
    response += "FIRMWARE ";
    response += m_deviceInfo.firmwareVersion ? m_deviceInfo.firmwareVersion : "dev";
    response += "\n";
    response += "HARDWARE ";
    response += m_deviceInfo.hardwareRevision ? m_deviceInfo.hardwareRevision : "unknown";
    response += "\n";
    response += "CAPABILITIES filesystem,graphs,settings,terminal\n";
    response += "END\n";
}

void CompanionProtocol::appendInfo(std::string& response) {
    const AxiomFS::StorageStats stats = AxiomFS::getStorageStats(m_filesystem);

    response += "OK INFO\n";
    char line[64] = {};
    std::snprintf(line,
                  sizeof(line),
                  "STORAGE_TOTAL %llu\n",
                  static_cast<unsigned long long>(stats.totalBytes));
    response += line;
    std::snprintf(line,
                  sizeof(line),
                  "STORAGE_USED %llu\n",
                  static_cast<unsigned long long>(stats.usedBytes));
    response += line;
    std::snprintf(line,
                  sizeof(line),
                  "STORAGE_FREE %llu\n",
                  static_cast<unsigned long long>(stats.freeBytes));
    response += line;
    response += "FILESYSTEM ";
    response += filesystemProtocolName(m_filesystem.backendName());
    response += "\n";
    response += "END\n";
}

} // namespace Companion
