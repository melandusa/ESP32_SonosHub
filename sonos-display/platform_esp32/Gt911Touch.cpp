#include "Gt911Touch.h"

#include "platform/Log.h"

#include <Arduino.h>
#include <Wire.h>

namespace esp_platform {

namespace {
constexpr uint8_t kAddresses[] = {0x5D, 0x14};  // depends on the INT level at power-up
constexpr uint16_t kRegProductId = 0x8140;
constexpr uint16_t kRegStatus = 0x814E;
constexpr uint16_t kRegPoint1 = 0x8150;
} // namespace

bool Gt911Touch::begin(int sda, int scl) {
    Wire.begin(sda, scl, 400000);
    for (uint8_t address : kAddresses) {
        address_ = address;
        char id[5] = {};  // e.g. "911", zero padded
        if (readRegister(kRegProductId, reinterpret_cast<uint8_t*>(id), 4)) {
            LOGI("Touch: GT%s at 0x%02X", id, address);
            return true;
        }
    }
    LOGE("Touch: GT911 not found");
    address_ = 0;
    return false;
}

bool Gt911Touch::read(int16_t& x, int16_t& y) {
    uint8_t status = 0;
    if (address_ != 0 && readRegister(kRegStatus, &status, 1) && (status & 0x80)) {
        const uint8_t points = status & 0x0F;
        uint8_t data[4];
        if (points > 0 && readRegister(kRegPoint1, data, sizeof(data))) {
            x_ = static_cast<int16_t>(data[0] | (data[1] << 8));
            y_ = static_cast<int16_t>(data[2] | (data[3] << 8));
            if (!pressed_) {
                LOGD("Touch down at %d/%d (raw)", x_, y_);
                pressedAtMs_ = millis();
            }
            pressed_ = true;
        } else {
            if (pressed_) LOGD("Touch up after %lu ms", static_cast<unsigned long>(millis() - pressedAtMs_));
            pressed_ = false;
        }
        writeRegister(kRegStatus, 0);  // buffer consumed
    }
    x = x_;
    y = y_;
    return pressed_;
}

bool Gt911Touch::readRegister(uint16_t reg, uint8_t* data, uint8_t length) {
    Wire.beginTransmission(address_);
    Wire.write(static_cast<uint8_t>(reg >> 8));
    Wire.write(static_cast<uint8_t>(reg & 0xFF));
    if (Wire.endTransmission() != 0) return false;
    if (Wire.requestFrom(address_, length) != length) return false;
    for (uint8_t i = 0; i < length; ++i) data[i] = static_cast<uint8_t>(Wire.read());
    return true;
}

bool Gt911Touch::writeRegister(uint16_t reg, uint8_t value) {
    Wire.beginTransmission(address_);
    Wire.write(static_cast<uint8_t>(reg >> 8));
    Wire.write(static_cast<uint8_t>(reg & 0xFF));
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

} // namespace esp_platform
