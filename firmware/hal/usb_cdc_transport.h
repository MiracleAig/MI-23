#pragma once

#include <cstddef>
#include <cstdint>

namespace Companion {

class UsbCdcTransport {
public:
    virtual ~UsbCdcTransport() = default;

    virtual bool isConnected() const = 0;
    virtual int read(uint8_t* buffer, std::size_t maxLength) = 0;
    virtual bool write(const uint8_t* data, std::size_t length) = 0;
};

} // namespace Companion
