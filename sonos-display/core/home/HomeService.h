#pragma once

#include "home/Commands.h"
#include "home/IHomeBackend.h"

namespace home {

// Worker side: executes HomeCommands on the backend.
class HomeService {
public:
    explicit HomeService(IHomeBackend& backend) : backend_(backend) {}

    HomeResult execute(const HomeCommand& command);

private:
    IHomeBackend& backend_;
};

} // namespace home
