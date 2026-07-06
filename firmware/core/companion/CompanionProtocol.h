#pragma once

#include "hal/fs/axiom_fs.h"

#include <string>

namespace Companion {

struct DeviceInfo {
    const char* firmwareVersion = "dev";
    const char* hardwareRevision = "unknown";
};

class CompanionProtocol {
public:
    CompanionProtocol(AxiomFS::FileSystem& filesystem, DeviceInfo deviceInfo);

    bool handleCommand(const std::string& command, std::string& response);

private:
    AxiomFS::FileSystem& m_filesystem;
    DeviceInfo m_deviceInfo;

    void appendHello(std::string& response) const;
    void appendInfo(std::string& response);
};

} // namespace Companion
