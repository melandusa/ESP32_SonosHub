#pragma once

#include "app/BasicWorker.h"
#include "home/Types.h"

#include <cstdint>
#include <string>

namespace home {

enum class HomeCommandType { Connect, Poll, SetSwitch, SetShutterLevel, StopShutter };

const char* homeCommandName(HomeCommandType type);

// Sent from the UI thread to the home worker.
struct HomeCommand {
    HomeCommandType type = HomeCommandType::Poll;
    uint32_t seq = 0;
    Device device;       // SetSwitch, SetShutterLevel, StopShutter: target channel
    bool on = false;     // SetSwitch
    double level = 0.0;  // SetShutterLevel: 0.0 open .. 1.0 closed
};

// Sent from the worker back to the UI thread. Every result carries the full device list
// as the worker knows it after the command.
struct HomeResult {
    HomeCommandType type = HomeCommandType::Poll;
    uint32_t seq = 0;
    HomeStatus status = HomeStatus::Ok;
    bool connected = false;  // the connection to the HCU is open after the command
    DeviceList devices;

    bool ok() const { return status == HomeStatus::Ok; }
};

using HomeWorker = app::BasicWorker<HomeCommand, HomeResult>;

} // namespace home
