#pragma once

#include <cstdint>

namespace esp_platform {

// Minimal GT911 driver: first touch point only, polled over I2C.
class Gt911Touch {
public:
    bool begin(int sda, int scl);
    // Returns true while touched; x/y keep the last position after release.
    bool read(int16_t& x, int16_t& y);

private:
    bool readRegister(uint16_t reg, uint8_t* data, uint8_t length);
    bool writeRegister(uint16_t reg, uint8_t value);

    uint8_t address_ = 0;
    bool pressed_ = false;
    uint32_t pressedAtMs_ = 0;
    int16_t x_ = 0;
    int16_t y_ = 0;
};

} // namespace esp_platform
