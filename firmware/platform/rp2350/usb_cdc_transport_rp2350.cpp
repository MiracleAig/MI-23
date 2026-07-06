#include "platform/rp2350/usb_cdc_transport_rp2350.h"

#include "pico/stdio.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"

bool RP2350UsbCdcTransport::isConnected() const {
    return stdio_usb_connected();
}

int RP2350UsbCdcTransport::read(uint8_t* buffer, std::size_t maxLength) {
    if (!buffer || maxLength == 0 || !stdio_usb_connected()) {
        return 0;
    }

    std::size_t count = 0;
    while (count < maxLength) {
        const int ch = getchar_timeout_us(0);
        if (ch == PICO_ERROR_TIMEOUT) {
            break;
        }
        buffer[count++] = static_cast<uint8_t>(ch);
    }

    return static_cast<int>(count);
}

bool RP2350UsbCdcTransport::write(const uint8_t* data, std::size_t length) {
    if (!data && length != 0) {
        return false;
    }
    if (!stdio_usb_connected()) {
        return false;
    }

    for (std::size_t i = 0; i < length; ++i) {
        putchar_raw(static_cast<char>(data[i]));
    }
    return true;
}
