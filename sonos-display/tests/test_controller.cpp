#include "app/AppController.h"

#include <doctest/doctest.h>

#include <deque>
#include <vector>

using namespace app;
using sonos::CallStatus;
using sonos::TransportState;

namespace {

class FakeClock : public platform::IClock {
public:
    uint64_t nowMs() const override { return now; }
    uint64_t now = 1000;
};

class FakeBacklight : public platform::IBacklight {
public:
    void setLevel(int value) override { level = value; }
    int level = -1;
};

// Records submitted commands; results are injected by the test.
class FakeWorker : public IWorker {
public:
    bool submit(const Command& command) override {
        submitted.push_back(command);
        return true;
    }
    bool poll(Result& result) override {
        if (results.empty()) return false;
        result = results.front();
        results.pop_front();
        return true;
    }

    std::vector<Command> take() {
        std::vector<Command> out;
        out.swap(submitted);
        return out;
    }
    void reply(const Command& command, CallStatus status = CallStatus::Ok) {
        Result result;
        result.type = command.type;
        result.seq = command.seq;
        result.status = status;
        results.push_back(result);
    }

    std::vector<Command> submitted;
    std::deque<Result> results;
};

int count(const std::vector<Command>& commands, CommandType type) {
    int n = 0;
    for (const Command& c : commands) n += c.type == type ? 1 : 0;
    return n;
}

const Command* find(const std::vector<Command>& commands, CommandType type) {
    for (const Command& c : commands) {
        if (c.type == type) return &c;
    }
    return nullptr;
}

struct Fixture {
    FakeClock clock;
    FakeBacklight backlight;
    FakeWorker worker;
    AppConfig config;
    AppController controller{worker, clock, backlight, makeConfig()};

    static AppConfig makeConfig() {
        AppConfig c;
        c.maxVolume = 60;
        return c;
    }

    // Runs discovery and the first status poll with the given volume.
    std::vector<Command> connect(int volume = 30) {
        controller.start();
        controller.tick();
        auto commands = worker.take();
        REQUIRE(commands.size() == 1);
        REQUIRE(commands[0].type == CommandType::Discover);
        Result found;
        found.type = CommandType::Discover;
        found.seq = commands[0].seq;
        found.deviceIp = "192.168.1.50";
        found.roomName = "Küche";
        worker.results.push_back(found);
        controller.tick();
        REQUIRE(controller.state().connection == Connection::Connected);

        commands = worker.take();
        const Command* status = find(commands, CommandType::RefreshStatus);
        REQUIRE(status != nullptr);
        Result result;
        result.type = CommandType::RefreshStatus;
        result.seq = status->seq;
        result.info.transport = TransportState::Stopped;
        result.info.volume = volume;
        worker.results.push_back(result);

        const Command* favorites = find(commands, CommandType::LoadFavorites);
        REQUIRE(favorites != nullptr);
        Result favs;
        favs.type = CommandType::LoadFavorites;
        favs.seq = favorites->seq;
        favs.favorites = {{"SWR1", "x-sonosapi-stream:s1", "<DIDL-Lite/>", ""},
                          {"SWR3", "x-sonosapi-stream:s3", "<DIDL-Lite/>", ""}};
        worker.results.push_back(favs);
        controller.tick();
        return worker.take();
    }
};

} // namespace

TEST_CASE("Controller: discovery, then status and favorites") {
    Fixture f;
    f.connect(30);
    const AppState& state = f.controller.state();
    CHECK(state.roomName == "Küche");
    CHECK(state.volume == 30);
    CHECK(state.volumeKnown);
    CHECK(state.favorites.size() == 2);
    CHECK(f.backlight.level == 100);
}

TEST_CASE("Controller: status is polled every 2 s, favorites every 10 min") {
    Fixture f;
    f.connect();
    f.clock.now += 1999;
    f.controller.tick();
    CHECK(f.worker.take().empty());

    f.clock.now += 1;
    f.controller.tick();
    auto commands = f.worker.take();
    CHECK(count(commands, CommandType::RefreshStatus) == 1);

    // No second poll while one is in flight
    f.clock.now += 5000;
    f.controller.tick();
    CHECK(count(f.worker.take(), CommandType::RefreshStatus) == 0);
    f.worker.reply(commands[0]);
    f.controller.tick();
    CHECK(count(f.worker.take(), CommandType::RefreshStatus) == 1);

    f.clock.now += 10 * 60 * 1000;
    f.controller.tick();
    CHECK(count(f.worker.take(), CommandType::LoadFavorites) == 1);
}

TEST_CASE("Controller: volume is optimistic and debounced, last value wins") {
    Fixture f;
    f.connect(30);

    f.controller.volumeUp();
    CHECK(f.controller.state().volume == 33);
    auto commands = f.worker.take();
    REQUIRE(count(commands, CommandType::SetVolume) == 1);
    CHECK(find(commands, CommandType::SetVolume)->volume == 33);

    // Two more presses within 200 ms: shown at once, sent once afterwards
    f.clock.now += 50;
    f.controller.volumeUp();
    f.controller.volumeUp();
    CHECK(f.controller.state().volume == 39);
    CHECK(count(f.worker.take(), CommandType::SetVolume) == 0);

    f.clock.now += 100;
    f.controller.tick();
    CHECK(count(f.worker.take(), CommandType::SetVolume) == 0);

    f.clock.now += 50;
    f.controller.tick();
    commands = f.worker.take();
    REQUIRE(count(commands, CommandType::SetVolume) == 1);
    CHECK(find(commands, CommandType::SetVolume)->volume == 39);
}

TEST_CASE("Controller: volume never exceeds the limit") {
    Fixture f;
    f.connect(58);
    f.controller.volumeUp();
    CHECK(f.controller.state().volume == 60);
    f.clock.now += 300;
    f.controller.volumeUp();
    f.controller.tick();
    CHECK(f.controller.state().volume == 60);
    for (const Command& c : f.worker.take()) {
        if (c.type == CommandType::SetVolume) CHECK(c.volume <= 60);
    }
}

TEST_CASE("Controller: volume set elsewhere above the limit is reduced") {
    Fixture f;
    // Corrected in the same tick the status arrives
    auto commands = f.connect(80);
    CHECK(f.controller.state().volume == 60);
    REQUIRE(count(commands, CommandType::SetVolume) == 1);
    CHECK(find(commands, CommandType::SetVolume)->volume == 60);
}

TEST_CASE("Controller: volume down stops at 0") {
    Fixture f;
    f.connect(2);
    f.controller.volumeDown();
    CHECK(f.controller.state().volume == 0);
}

TEST_CASE("Controller: play is optimistic, stale polls are ignored") {
    Fixture f;
    f.connect();

    // A poll is issued before the user taps a station
    f.clock.now += 2000;
    f.controller.tick();
    const Command poll = f.worker.take().at(0);

    f.controller.playFavorite(1);
    auto commands = f.worker.take();
    const Command* play = find(commands, CommandType::PlayFavorite);
    REQUIRE(play != nullptr);
    CHECK(play->uri == "x-sonosapi-stream:s3");
    CHECK(play->metadata == "<DIDL-Lite/>");
    CHECK(f.controller.state().currentTitle == "SWR3");
    CHECK(f.controller.state().transport == TransportState::Transitioning);
    CHECK(f.controller.state().activeFavorite == 1);

    // The old poll still reports "stopped": must not undo the optimistic state
    Result stale;
    stale.type = CommandType::RefreshStatus;
    stale.seq = poll.seq;
    stale.info.transport = TransportState::Stopped;
    stale.info.volume = 30;
    f.worker.results.push_back(stale);
    f.controller.tick();
    CHECK(f.controller.state().transport == TransportState::Transitioning);
    CHECK(f.controller.state().currentTitle == "SWR3");
}

TEST_CASE("Controller: station changed in the Sonos app shows up after the next poll") {
    Fixture f;
    f.connect();
    f.clock.now += 2000;
    f.controller.tick();
    const Command poll = f.worker.take().at(0);
    Result result;
    result.type = CommandType::RefreshStatus;
    result.seq = poll.seq;
    result.info.transport = TransportState::Playing;
    result.info.volume = 30;
    result.info.media = {"x-sonosapi-stream:s1", "SWR1"};
    f.worker.results.push_back(result);
    f.controller.tick();
    CHECK(f.controller.state().currentTitle == "SWR1");
    CHECK(f.controller.state().activeFavorite == 0);
}

TEST_CASE("Controller: 3 network errors make the speaker unreachable, then rediscover with backoff") {
    Fixture f;
    f.connect();

    for (int i = 0; i < 3; ++i) {
        f.clock.now += 2000;
        f.controller.tick();
        auto commands = f.worker.take();
        const Command* poll = find(commands, CommandType::RefreshStatus);
        REQUIRE(poll != nullptr);
        CHECK(f.controller.state().connection == Connection::Connected);
        f.worker.reply(*poll, CallStatus::NetworkError);
    }
    f.controller.tick();
    CHECK(f.controller.state().connection == Connection::Unreachable);
    CHECK(count(f.worker.take(), CommandType::Discover) == 0);

    // Rediscovery after 5 s, then 10 s, 20 s, 30 s, 30 s
    const uint64_t expected[] = {5000, 10000, 20000, 30000, 30000};
    for (uint64_t delay : expected) {
        f.clock.now += delay - 1;
        f.controller.tick();
        CHECK(count(f.worker.take(), CommandType::Discover) == 0);
        f.clock.now += 1;
        f.controller.tick();
        auto commands = f.worker.take();
        REQUIRE(count(commands, CommandType::Discover) == 1);
        CHECK(find(commands, CommandType::Discover)->preferredIp == "192.168.1.50");
        f.worker.reply(commands[0], CallStatus::NetworkError);
        f.controller.tick();
    }

    // Speaker is back
    f.clock.now += 30000;
    f.controller.tick();
    auto commands = f.worker.take();
    REQUIRE(count(commands, CommandType::Discover) == 1);
    Result found;
    found.type = CommandType::Discover;
    found.seq = commands[0].seq;
    found.deviceIp = "192.168.1.50";
    f.worker.results.push_back(found);
    f.controller.tick();
    CHECK(f.controller.state().connection == Connection::Connected);
    commands = f.worker.take();
    CHECK(count(commands, CommandType::RefreshStatus) == 1);
    CHECK(count(commands, CommandType::LoadFavorites) == 1);
}

TEST_CASE("Controller: SOAP faults do not count as connection loss") {
    Fixture f;
    f.connect();
    for (int i = 0; i < 5; ++i) {
        f.controller.stop();
        auto commands = f.worker.take();
        f.worker.reply(commands.at(0), CallStatus::SoapFault);
        f.controller.tick();
    }
    CHECK(f.controller.state().connection == Connection::Connected);
}

TEST_CASE("Controller: commands are ignored while not connected") {
    Fixture f;
    f.controller.start();
    f.controller.tick();
    f.worker.take();
    f.controller.playFavorite(0);
    f.controller.volumeUp();
    f.controller.stop();
    CHECK(f.worker.take().empty());
}

TEST_CASE("Controller: display dims after 2 min without touch") {
    Fixture f;
    f.connect();
    CHECK(f.backlight.level == 100);
    f.clock.now += 2 * 60 * 1000 - 1;
    f.controller.tick();
    CHECK(f.backlight.level == 100);
    f.clock.now += 1;
    f.controller.tick();
    CHECK(f.backlight.level == f.controller.config().backlightDim);

    f.controller.noteUserActivity();
    CHECK(f.backlight.level == 100);
}
