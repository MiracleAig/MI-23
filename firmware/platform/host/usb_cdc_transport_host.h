#pragma once

#include "hal/usb_cdc_transport.h"

class HostUsbCdcTransport : public Companion::UsbCdcTransport {
public:
    bool isConnected() const override;
    int read(uint8_t* buffer, std::size_t maxLength) override;
    bool write(const uint8_t* data, std::size_t length) override;
};
