#include "app/AppController.h"
#include "app/SonosService.h"
#include "hcu/HcuClient.h"
#include "home/HomeController.h"
#include "home/HomeService.h"
#include "platform/Log.h"
#include "sonos/SonosClient.h"
#include "ui/Fonts.h"
#include "ui/LogoDecoder.h"
#include "ui/MainScreen.h"

#include "CurlHttpClient.h"
#include "LinuxPlatform.h"
#include "MockHome.h"
#include "MockSonos.h"
#include "OpenSslSocket.h"
#include "PosixSsdpDiscovery.h"
#include "ThreadWorker.h"

#include "lvgl.h"

#include <SDL2/SDL.h>
#include <curl/curl.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <random>
#include <sstream>
#include <string>

namespace {

constexpr int kScreenSize = 480;
constexpr uint32_t kUiTickMs = 30;
constexpr const char* kHcuPluginId = "de.esp32sonoshub.display.simulation";

struct Options {
    std::string ip;
    bool mock = false;
    int maxVolume = 60;
    bool dumpXml = false;
    bool verbose = false;
    std::string hcuHost;
    std::string hcuTokenFile = "hcu_token.txt";
    std::string hcuActivationKey;
    bool hcuMock = false;
};

void printUsage(const char* program) {
    std::printf("Usage: %s [options]\n"
                "  --ip <address>     Use this speaker, skip SSDP discovery\n"
                "  --mock             Simulated speaker, no network needed\n"
                "  --max-volume <n>   Volume limit 0..100 (default 60)\n"
                "  --dump-xml         Log raw XML responses (for test fixtures)\n"
                "  --verbose          Debug logging\n"
                "  --hcu <host>       Homematic IP HCU (IP or hcu1-XXXX.local): sockets and shutters\n"
                "  --hcu-pair <key>   Pair with the HCU using an activation key from HCUweb, save the token\n"
                "  --hcu-token-file <file>  Authorization token for --hcu (default hcu_token.txt)\n"
                "  --hcu-mock         Simulated HCU with sockets and shutters\n",
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
        } else if (std::strcmp(arg, "--hcu") == 0 && hasValue) {
            options.hcuHost = argv[++i];
        } else if (std::strcmp(arg, "--hcu-pair") == 0 && hasValue) {
            options.hcuActivationKey = argv[++i];
        } else if (std::strcmp(arg, "--hcu-token-file") == 0 && hasValue) {
            options.hcuTokenFile = argv[++i];
        } else if (std::strcmp(arg, "--hcu-mock") == 0) {
            options.hcuMock = true;
        } else {
            return false;
        }
    }
    return true;
}

struct Mocks {
    linux_platform::MockSonos* sonos = nullptr;
    linux_platform::MockHome* home = nullptr;
};

// Mock mode: key 'o' unplugs / replugs the simulated speaker, 'h' the simulated HCU.
int onSdlEvent(void* userData, SDL_Event* event) {
    if (event->type != SDL_KEYDOWN || event->key.repeat != 0) return 1;
    auto* mocks = static_cast<Mocks*>(userData);
    if (event->key.keysym.sym == SDLK_o && mocks->sonos) {
        mocks->sonos->setOnline(!mocks->sonos->isOnline());
        LOGI("Mock: speaker %s", mocks->sonos->isOnline() ? "plugged in" : "unplugged");
    } else if (event->key.keysym.sym == SDLK_h && mocks->home) {
        mocks->home->setOnline(!mocks->home->isOnline());
        LOGI("Mock: HCU %s", mocks->home->isOnline() ? "plugged in" : "unplugged");
    }
    return 1;
}

std::string readToken(const std::string& path) {
    std::ifstream file(path);
    std::ostringstream content;
    content << file.rdbuf();
    std::string token = content.str();
    while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back()))) token.pop_back();
    return token;
}

struct UiContext {
    app::AppController* controller;
    home::HomeController* home;
    ui::MainScreen* screen;
};

void onUiTick(lv_timer_t* timer) {
    auto* context = static_cast<UiContext*>(lv_timer_get_user_data(timer));
    if (lv_display_get_inactive_time(nullptr) < kUiTickMs * 2) {
        context->controller->noteUserActivity();
    }
    context->controller->tick();
    if (context->home) context->home->tick();
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

    // Homematic IP (optional): real HCU or simulation
    Mocks mocks;
    mocks.sonos = mockSonos;
    linux_platform::OpenSslSocket hcuSocket;
    std::unique_ptr<home::IHomeBackend> homeBackend;
    if (!options.hcuActivationKey.empty()) {
        if (options.hcuHost.empty()) {
            std::fprintf(stderr, "--hcu-pair needs --hcu <host>\n");
            return EXIT_FAILURE;
        }
        LOGI("HCU: pairing with %s", options.hcuHost.c_str());
        const hcu::PairingResult pairing =
            hcu::pairWithHcu(hcuSocket, options.hcuHost, options.hcuActivationKey, kHcuPluginId,
                             "Sonos-Display (Simulation)");
        if (!pairing.ok) {
            LOGE("HCU: pairing failed: %s", pairing.error.c_str());
            return EXIT_FAILURE;
        }
        std::ofstream(options.hcuTokenFile) << pairing.authToken << "\n";
        LOGI("HCU: paired, token saved to %s", options.hcuTokenFile.c_str());
    }
    if (options.hcuMock) {
        LOGI("Mock mode: simulated HCU (press 'h' to unplug/replug it)");
        auto mock = std::make_unique<linux_platform::MockHome>();
        mocks.home = mock.get();
        homeBackend = std::move(mock);
    } else if (!options.hcuHost.empty()) {
        hcu::HcuConfig hcuConfig;
        hcuConfig.host = options.hcuHost;
        hcuConfig.authToken = readToken(options.hcuTokenFile);
        hcuConfig.pluginId = kHcuPluginId;
        if (hcuConfig.authToken.empty()) {
            LOGW("HCU: no token in %s, pair first with --hcu-pair <activation key>",
                 options.hcuTokenFile.c_str());
        }
        homeBackend = std::make_unique<hcu::HcuClient>(hcuSocket, clock, hcuConfig, std::random_device{}());
    }
    std::unique_ptr<home::HomeService> homeService;
    std::unique_ptr<linux_platform::BasicThreadWorker<home::HomeCommand, home::HomeResult>> homeWorker;
    std::unique_ptr<home::HomeController> homeController;
    if (homeBackend) {
        homeService = std::make_unique<home::HomeService>(*homeBackend);
        homeWorker = std::make_unique<linux_platform::BasicThreadWorker<home::HomeCommand, home::HomeResult>>(
            [&homeService](const home::HomeCommand& command) { return homeService->execute(command); });
        homeController = std::make_unique<home::HomeController>(*homeWorker, clock);
    }

    lv_init();
    lv_display_t* display = lv_sdl_window_create(kScreenSize, kScreenSize);
    lv_sdl_window_set_title(display, "Sonos Display (Simulation)");
    lv_sdl_mouse_create();
    if (mocks.sonos || mocks.home) SDL_AddEventWatch(onSdlEvent, &mocks);
    ui::fonts::init();

    ui::MainScreen screen(controller);
    screen.create(lv_screen_active());
    screen.setVersionText("Simulation");
    controller.start();
    if (homeController) {
        screen.enableHome(*homeController);
        homeController->start();
    }

    UiContext context{&controller, homeController.get(), &screen};
    lv_timer_create(onUiTick, kUiTickMs, &context);

    // Closing the window ends the process (LV_SDL_DIRECT_EXIT).
    while (true) {
        uint32_t sleepMs = lv_timer_handler();
        if (sleepMs == LV_NO_TIMER_READY || sleepMs > 10) sleepMs = 10;
        SDL_Delay(sleepMs);
    }
}
