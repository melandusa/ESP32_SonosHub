#include "home/HomeService.h"

#include "platform/Log.h"

namespace home {

const char* homeStatusName(HomeStatus status) {
    switch (status) {
    case HomeStatus::Ok: return "ok";
    case HomeStatus::NetworkError: return "network error";
    case HomeStatus::AuthError: return "authorization rejected";
    case HomeStatus::Rejected: return "rejected";
    }
    return "?";
}

const char* homeCommandName(HomeCommandType type) {
    switch (type) {
    case HomeCommandType::Connect: return "Connect";
    case HomeCommandType::Poll: return "Poll";
    case HomeCommandType::SetSwitch: return "SetSwitch";
    case HomeCommandType::SetShutterLevel: return "SetShutterLevel";
    case HomeCommandType::StopShutter: return "StopShutter";
    }
    return "?";
}

namespace {
constexpr int kPollTimeoutMs = 250;  // also the longest delay before a queued tap is sent
}

HomeResult HomeService::execute(const HomeCommand& command) {
    HomeResult result;
    result.type = command.type;
    result.seq = command.seq;

    if (command.type != HomeCommandType::Connect && !backend_.isConnected()) {
        result.status = HomeStatus::NetworkError;
    } else {
        switch (command.type) {
        case HomeCommandType::Connect: result.status = backend_.connect(); break;
        case HomeCommandType::Poll: result.status = backend_.poll(kPollTimeoutMs); break;
        case HomeCommandType::SetSwitch:
            result.status = backend_.setSwitch(command.device, command.on);
            break;
        case HomeCommandType::SetShutterLevel:
            result.status = backend_.setShutterLevel(command.device, command.level);
            break;
        case HomeCommandType::StopShutter: result.status = backend_.stopShutter(command.device); break;
        }
    }
    if (!result.ok() && command.type != HomeCommandType::Poll) {
        LOGW("Home: %s %s failed: %s", homeCommandName(command.type), command.device.name.c_str(),
             homeStatusName(result.status));
    }
    result.connected = backend_.isConnected();
    result.devices = backend_.devices();
    return result;
}

} // namespace home
