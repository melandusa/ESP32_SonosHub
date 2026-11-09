#include "home/HomeController.h"
#include "home/HomeService.h"

#include <doctest/doctest.h>

#include <deque>
#include <vector>

using namespace home;

namespace {

class FakeClock : public platform::IClock {
public:
    uint64_t nowMs() const override { return now; }
    uint64_t now = 1000;
};

class FakeWorker : public HomeWorker {
public:
    bool submit(const HomeCommand& command) override {
        if (full) return false;
        submitted.push_back(command);
        return true;
    }
    bool poll(HomeResult& result) override {
        if (results.empty()) return false;
        result = results.front();
        results.pop_front();
        return true;
    }

    std::vector<HomeCommand> take() {
        std::vector<HomeCommand> out;
        out.swap(submitted);
        return out;
    }
    void reply(const HomeCommand& command, const DeviceList& devices, HomeStatus status = HomeStatus::Ok,
               bool connected = true) {
        HomeResult result;
        result.type = command.type;
        result.seq = command.seq;
        result.status = status;
        result.connected = connected;
        result.devices = devices;
        results.push_back(result);
    }

    bool full = false;
    std::vector<HomeCommand> submitted;
    std::deque<HomeResult> results;
};

Device makeSwitch(const char* name, bool on) {
    Device d;
    d.deviceId = std::string("ID-") + name;
    d.channel = 1;
    d.kind = DeviceKind::Switch;
    d.name = name;
    d.on = on;
    return d;
}

Device makeShutter(const char* name, double level) {
    Device d = makeSwitch(name, false);
    d.kind = DeviceKind::Shutter;
    d.level = level;
    return d;
}

struct Fixture {
    FakeClock clock;
    FakeWorker worker;
    HomeController controller{worker, clock};
    DeviceList devices{makeSwitch("Lampe", false), makeShutter("Rollladen", 0.0)};

    // Starts and completes the first connect; returns the poll that follows.
    HomeCommand connect() {
        controller.start();
        controller.tick();
        auto commands = worker.take();
        REQUIRE(commands.size() == 1);
        REQUIRE(commands[0].type == HomeCommandType::Connect);
        worker.reply(commands[0], devices);
        controller.tick();
        commands = worker.take();
        REQUIRE(commands.size() == 1);
        REQUIRE(commands[0].type == HomeCommandType::Poll);
        return commands[0];
    }
};

} // namespace

TEST_CASE("Home: nothing happens before start") {
    Fixture f;
    f.controller.tick();
    CHECK(f.worker.submitted.empty());
}

TEST_CASE("Home: connect, then keep exactly one poll running") {
    Fixture f;
    const HomeCommand poll = f.connect();
    CHECK(f.controller.state().connection == HomeConnection::Connected);
    CHECK(f.controller.state().devices == f.devices);

    f.controller.tick();
    CHECK(f.worker.take().empty());  // poll still running

    f.devices[0].on = true;  // changed by someone else
    const uint32_t version = f.controller.version();
    f.worker.reply(poll, f.devices);
    f.controller.tick();
    CHECK(f.controller.state().devices[0].on);
    CHECK(f.controller.version() != version);
    auto commands = f.worker.take();
    REQUIRE(commands.size() == 1);
    CHECK(commands[0].type == HomeCommandType::Poll);

    // Unchanged devices: no redraw
    const uint32_t unchanged = f.controller.version();
    f.worker.reply(commands[0], f.devices);
    f.controller.tick();
    CHECK(f.controller.version() == unchanged);
}

TEST_CASE("Home: toggling a socket is shown at once, stale polls cannot undo it") {
    Fixture f;
    const HomeCommand poll = f.connect();

    f.controller.toggleSwitch(0);
    CHECK(f.controller.state().devices[0].on);
    auto commands = f.worker.take();
    REQUIRE(commands.size() == 1);
    CHECK(commands[0].type == HomeCommandType::SetSwitch);
    CHECK(commands[0].on);
    CHECK(commands[0].device.deviceId == "ID-Lampe");

    // The poll was issued before the tap and still carries the old state
    f.worker.reply(poll, f.devices);
    f.controller.tick();
    CHECK(f.controller.state().devices[0].on);

    DeviceList after = f.devices;
    after[0].on = true;
    f.worker.reply(commands[0], after);
    f.controller.tick();
    CHECK(f.controller.state().devices[0].on);
}

TEST_CASE("Home: shutter commands") {
    Fixture f;
    f.connect();

    f.controller.closeShutter(1);
    auto commands = f.worker.take();
    REQUIRE(commands.size() == 1);
    CHECK(commands[0].type == HomeCommandType::SetShutterLevel);
    CHECK(commands[0].level == 1.0);
    CHECK(f.controller.state().devices[1].moving);

    f.controller.openShutter(1);
    commands = f.worker.take();
    REQUIRE(commands.size() == 1);
    CHECK(commands[0].level == 0.0);

    f.controller.stopShutter(1);
    commands = f.worker.take();
    REQUIRE(commands.size() == 1);
    CHECK(commands[0].type == HomeCommandType::StopShutter);
    CHECK_FALSE(f.controller.state().devices[1].moving);

    // Wrong kind or index: ignored
    f.controller.toggleSwitch(1);
    f.controller.closeShutter(0);
    f.controller.stopShutter(7);
    CHECK(f.worker.take().empty());
}

TEST_CASE("Home: lost connection, reconnect with backoff") {
    Fixture f;
    const HomeCommand poll = f.connect();

    f.worker.reply(poll, f.devices, HomeStatus::NetworkError, false);
    f.controller.tick();
    CHECK(f.controller.state().connection == HomeConnection::Unreachable);
    CHECK(f.controller.state().devices.size() == 2);  // still shown, disabled
    CHECK(f.worker.take().empty());

    f.controller.toggleSwitch(0);  // not possible while unreachable
    CHECK(f.worker.take().empty());

    f.clock.now += 5000;
    f.controller.tick();
    auto commands = f.worker.take();
    REQUIRE(commands.size() == 1);
    CHECK(commands[0].type == HomeCommandType::Connect);

    // Fails again: next attempt after 10 s
    f.worker.reply(commands[0], {}, HomeStatus::NetworkError, false);
    f.controller.tick();
    f.clock.now += 9000;
    f.controller.tick();
    CHECK(f.worker.take().empty());
    f.clock.now += 1000;
    f.controller.tick();
    commands = f.worker.take();
    REQUIRE(commands.size() == 1);
    f.worker.reply(commands[0], f.devices);
    f.controller.tick();
    CHECK(f.controller.state().connection == HomeConnection::Connected);
}

TEST_CASE("Home: rejected token") {
    Fixture f;
    f.controller.start();
    f.controller.tick();
    auto commands = f.worker.take();
    f.worker.reply(commands[0], {}, HomeStatus::AuthError, false);
    f.controller.tick();
    CHECK(f.controller.state().connection == HomeConnection::AuthFailed);
    f.clock.now += 60000;
    f.controller.tick();
    CHECK(f.worker.take().empty());
    f.clock.now += 5 * 60 * 1000;
    f.controller.tick();
    CHECK(f.worker.take().size() == 1);
}

TEST_CASE("Home: full worker queue drops the tap without changing the display") {
    Fixture f;
    f.connect();
    f.worker.full = true;
    f.controller.toggleSwitch(0);
    CHECK_FALSE(f.controller.state().devices[0].on);
}

namespace {

class FakeBackend : public IHomeBackend {
public:
    HomeStatus connect() override {
        connected = connectStatus == HomeStatus::Ok;
        return connectStatus;
    }
    bool isConnected() const override { return connected; }
    HomeStatus poll(int) override { return HomeStatus::Ok; }
    HomeStatus setSwitch(const Device& device, bool on) override {
        calls.push_back(device.name + (on ? " on" : " off"));
        return HomeStatus::Ok;
    }
    HomeStatus setShutterLevel(const Device& device, double level) override {
        calls.push_back(device.name + " " + std::to_string(level));
        return HomeStatus::Ok;
    }
    HomeStatus stopShutter(const Device& device) override {
        calls.push_back(device.name + " stop");
        return HomeStatus::Ok;
    }
    const DeviceList& devices() const override { return list; }

    HomeStatus connectStatus = HomeStatus::Ok;
    bool connected = false;
    DeviceList list{makeSwitch("Lampe", true)};
    std::vector<std::string> calls;
};

} // namespace

TEST_CASE("HomeService executes commands and reports the device list") {
    FakeBackend backend;
    HomeService service(backend);

    HomeCommand command;
    command.type = HomeCommandType::Poll;
    command.seq = 4;
    HomeResult result = service.execute(command);
    CHECK(result.status == HomeStatus::NetworkError);  // not connected yet
    CHECK_FALSE(result.connected);
    CHECK(result.seq == 4);

    command.type = HomeCommandType::Connect;
    result = service.execute(command);
    CHECK(result.ok());
    CHECK(result.connected);
    CHECK(result.devices.size() == 1);

    command.type = HomeCommandType::SetSwitch;
    command.device = makeSwitch("Lampe", true);
    command.on = false;
    CHECK(service.execute(command).ok());
    command.type = HomeCommandType::StopShutter;
    CHECK(service.execute(command).ok());
    CHECK(backend.calls == std::vector<std::string>{"Lampe off", "Lampe stop"});
}
