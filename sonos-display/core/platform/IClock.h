#pragma once

#include <cstdint>

namespace platform {

class IClock {
public:
    virtual ~IClock() = default;
    // Monotonic milliseconds since an arbitrary start point.
    virtual uint64_t nowMs() const = 0;
};

} // namespace platform
