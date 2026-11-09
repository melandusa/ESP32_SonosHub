#include "home/HomeController.h"

#include "platform/Log.h"

#include <algorithm>

namespace home {

HomeController::HomeController(HomeWorker& worker, const platform::IClock& clock, HomeConfig config)
    : worker_(worker), clock_(clock), config_(config) {}

void HomeController::start() {
    started_ = true;
    nextConnectMs_ = 0;
    backoffMs_ = config_.reconnectBackoffMinMs;
    setConnection(HomeConnection::Connecting);
}

bool HomeController::submit(HomeCommand command) {
    command.seq = nextSeq_++;
    if (!worker_.submit(command)) {
        LOGW("Home: worker queue full, %s dropped", homeCommandName(command.type));
        return false;
    }
    ++inFlight_;
    return true;
}

void HomeController::tick() {
    if (!started_) return;
    const uint64_t now = clock_.nowMs();

    HomeResult result;
    while (worker_.poll(result)) handleResult(result, now);

    if (state_.connection == HomeConnection::Connected) {
        // Keep exactly one poll queued: it waits for push events on the worker.
        if (!pollInFlight_) {
            HomeCommand poll;
            poll.type = HomeCommandType::Poll;
            pollInFlight_ = submit(poll);
        }
    } else if (!connectInFlight_ && inFlight_ == 0 && now >= nextConnectMs_) {
        HomeCommand connect;
        connect.type = HomeCommandType::Connect;
        connectInFlight_ = submit(connect);
    }
}

void HomeController::handleResult(const HomeResult& result, uint64_t now) {
    --inFlight_;
    if (result.type == HomeCommandType::Connect) connectInFlight_ = false;
    if (result.type == HomeCommandType::Poll) pollInFlight_ = false;

    if (result.status == HomeStatus::AuthError) {
        nextConnectMs_ = now + config_.authRetryMs;
        setConnection(HomeConnection::AuthFailed);
        return;
    }
    if (!result.connected) {
        if (state_.connection == HomeConnection::Connected || result.type == HomeCommandType::Connect) {
            nextConnectMs_ = now + backoffMs_;
            LOGI("Home: HCU unreachable, next attempt in %u s", static_cast<unsigned>(backoffMs_ / 1000));
            backoffMs_ = std::min(backoffMs_ * 2, config_.reconnectBackoffMaxMs);
            setConnection(HomeConnection::Unreachable);
        }
        return;
    }
    if (result.type == HomeCommandType::Connect && result.ok()) {
        backoffMs_ = config_.reconnectBackoffMinMs;
        setConnection(HomeConnection::Connected);
    }
    if (result.seq < controlSeq_) return;  // stale: issued before the latest tap
    if (result.devices != state_.devices) {
        state_.devices = result.devices;
        markChanged();
    }
}

void HomeController::setConnection(HomeConnection connection) {
    if (state_.connection == connection) return;
    state_.connection = connection;
    markChanged();
}

const Device* HomeController::controllable(std::size_t index, DeviceKind kind) const {
    if (state_.connection != HomeConnection::Connected || index >= state_.devices.size()) return nullptr;
    const Device& device = state_.devices[index];
    return device.kind == kind ? &device : nullptr;
}

void HomeController::toggleSwitch(std::size_t index) {
    const Device* device = controllable(index, DeviceKind::Switch);
    if (!device) return;
    HomeCommand command;
    command.type = HomeCommandType::SetSwitch;
    command.device = *device;
    command.on = !device->on;
    if (!submit(command)) return;
    controlSeq_ = nextSeq_ - 1;
    state_.devices[index].on = command.on;  // show it at once; events confirm or correct it
    markChanged();
}

void HomeController::moveShutter(std::size_t index, double level) {
    const Device* device = controllable(index, DeviceKind::Shutter);
    if (!device) return;
    HomeCommand command;
    command.type = HomeCommandType::SetShutterLevel;
    command.device = *device;
    command.level = level;
    if (!submit(command)) return;
    controlSeq_ = nextSeq_ - 1;
    state_.devices[index].moving = true;
    markChanged();
}

void HomeController::stopShutter(std::size_t index) {
    const Device* device = controllable(index, DeviceKind::Shutter);
    if (!device) return;
    HomeCommand command;
    command.type = HomeCommandType::StopShutter;
    command.device = *device;
    if (!submit(command)) return;
    controlSeq_ = nextSeq_ - 1;
    state_.devices[index].moving = false;
    markChanged();
}

} // namespace home
