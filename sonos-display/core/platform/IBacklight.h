#pragma once

namespace platform {

class IBacklight {
public:
    virtual ~IBacklight() = default;
    // level: 0 (off) .. 100 (full brightness)
    virtual void setLevel(int level) = 0;
};

} // namespace platform
