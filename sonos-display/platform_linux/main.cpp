#include "app/AppController.h"
#include "app/SonosService.h"
#include "platform/Log.h"
#include "sonos/SonosClient.h"
#include "ui/Fonts.h"
#include "ui/LogoDecoder.h"
#include "ui/MainScreen.h"

#include "CurlHttpClient.h"
#include "LinuxPlatform.h"
#include "MockSonos.h"
#include "PosixSsdpDiscovery.h"
#include "ThreadWorker.h"

#include "lvgl.h"

#include <SDL2/SDL.h>
#include <curl/curl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

namespace {

constexpr int kScreenSize = 480;
constexpr uint32_t kUiTickMs = 30;

struct Options {
    std::string ip;
    bool mock = false;
    int maxVolume = 60;
    bool dumpXml = false;
    bool verbose = false;
};

void printUsage(const char* program) {
    std::printf("Usage: %s [options]\n"
                "  --ip <address>     Use this speaker, skip SSDP discovery\n"
                "  --mock             Simulated speaker, no network needed\n"
                "  --max-volume <n>   Volume limit 0..100 (default 60)\n"
                "  --dump-xml         Log raw XML responses (for test fixtures)\n"
                "  --verbose          Debug logging\n",
                program);
}

bool parseOptions(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (std::strcmp(arg, "--ip") == 0 && hasValue) {
            options.ip = argv[++i];
        } else if (std::strcmp(arg, "--mock") == 0) {
            options.mock = true;
        } else if (std::strcmp(arg, "--max-volume") == 0 && hasValue) {
            char* end = nullptr;
            const long value = std::strtol(argv[++i], &end, 10);
            if (*end != '\0' || value < 0 || value > 100) {
                std::fprintf(stderr, "--max-volume must be 0..100\n");
                return false;
            }
            options.maxVolume = static_cast<int>(value);
        } else if (std::strcmp(arg, "--dump-xml") == 0) {
            options.dumpXml = true;
        } else if (std::strcmp(arg, "--verbose") == 0) {
            options.verbose = true;
        } else {
            return false;
        }
    }
    return true;
}

// Mock mode: key 'o' unplugs / replugs the simulated speaker.
int onSdlEvent(void* userData, SDL_Event* event) {
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_o && event->key.repeat == 0) {
        auto* mock = static_cast<linux_platform::MockSonos*>(userData);
        mock->setOnline(!mock->isOnline());
        LOGI("Mock: speaker %s", mock->isOnline() ? "plugged in" : "unplugged");
    }
    return 1;
}

struct UiContext {
    app::AppController* controller;
    ui::MainScreen* screen;
};

void onUiTick(lv_timer_t* timer) {
    auto* context = static_cast<UiContext*>(lv_timer_get_user_data(timer));
    if (lv_display_get_inactive_time(nullptr) < kUiTickMs * 2) {
        context->controller->noteUserActivity();
    }
    context->controller->tick();
    context->screen->refresh();
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parseOptions(argc, argv, options)) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    linux_platform::StdoutLogger logger;
    platform::setLogger(&logger);
    platform::setMinLogLevel(options.verbose ? platform::LogLevel::Debug : platform::LogLevel::Info);
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Network backend: real speaker or simulation
    std::unique_ptr<platform::IHttpClient> http;
    std::unique_ptr<platform::IDiscovery> discovery;
    linux_platform::MockSonos* mockSonos = nullptr;
    if (options.mock) {
        LOGI("Mock mode: simulated speaker (press 'o' to unplug/replug it)");
        auto mock = std::make_unique<linux_platform::MockSonos>();
        mockSonos = mock.get();
        discovery = std::make_unique<linux_platform::MockDiscoveryAdapter>(*mock);
        http = std::move(mock);
    } else {
        http = std::make_unique<linux_platform::CurlHttpClient>();
        discovery = std::make_unique<linux_platform::PosixSsdpDiscovery>();
    }
    if (!options.ip.empty()) LOGI("Using fixed speaker IP %s", options.ip.c_str());

    sonos::SonosClient client(*http);
    client.setDumpXml(options.dumpXml);
    app::AppConfig config;
    config.maxVolume = options.maxVolume;
    app::SonosService service(client, *discovery, options.ip);
    service.setLogoDecoder(ui::decodeLogo, config.logoSizePx);  // before the worker starts
    linux_platform::ThreadWorker worker(
        [&service](const app::Command& command) { return service.execute(command); });

    linux_platform::SteadyClock clock;
    linux_platform::LogBacklight backlight;
    app::AppController controller(worker, clock, backlight, config);

    lv_init();
    lv_display_t* display = lv_sdl_window_create(kScreenSize, kScreenSize);
    lv_sdl_window_set_title(display, "Sonos Display (Simulation)");
    lv_sdl_mouse_create();
    if (mockSonos) SDL_AddEventWatch(onSdlEvent, mockSonos);
    ui::fonts::init();

    ui::MainScreen screen(controller);
    screen.create(lv_screen_active());
    controller.start();

    UiContext context{&controller, &screen};
    lv_timer_create(onUiTick, kUiTickMs, &context);

    // Closing the window ends the process (LV_SDL_DIRECT_EXIT).
    while (true) {
        uint32_t sleepMs = lv_timer_handler();
        if (sleepMs == LV_NO_TIMER_READY || sleepMs > 10) sleepMs = 10;
        SDL_Delay(sleepMs);
    }
}
