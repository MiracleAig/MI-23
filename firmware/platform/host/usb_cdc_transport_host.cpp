#include "platform/host/usb_cdc_transport_host.h"

#include <cstdio>

bool HostUsbCdcTransport::isConnected() const {
    return false;
}

int HostUsbCdcTransport::read(uint8_t* buffer, std::size_t maxLength) {
    (void)buffer;
    (void)maxLength;
    return 0;
}

bool HostUsbCdcTransport::write(const uint8_t* data, std::size_t length) {
    if (!data && length != 0) {
        return false;
    }
    std::printf("[companion][host] response suppressed: %u bytes\n",
                static_cast<unsigned>(length));
    return true;
}
