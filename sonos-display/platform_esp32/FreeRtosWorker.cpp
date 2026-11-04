#include "FreeRtosWorker.h"

namespace esp_platform {

namespace {
// The network stack runs on core 0; LVGL and loop() run on core 1.
constexpr BaseType_t kWorkerCore = 0;
constexpr UBaseType_t kWorkerPriority = 1;
} // namespace

FreeRtosWorker::FreeRtosWorker(Executor executor, UBaseType_t queueLength, uint32_t stackBytes)
    : executor_(std::move(executor)),
      commands_(xQueueCreate(queueLength, sizeof(app::Command*))),
      results_(xQueueCreate(queueLength, sizeof(app::Result*))) {
    xTaskCreatePinnedToCore(taskEntry, "sonos_worker", stackBytes, this, kWorkerPriority, nullptr,
                            kWorkerCore);
}

bool FreeRtosWorker::submit(const app::Command& command) {
    auto* copy = new app::Command(command);
    if (xQueueSend(commands_, &copy, 0) == pdTRUE) return true;
    delete copy;
    return false;
}

bool FreeRtosWorker::poll(app::Result& result) {
    app::Result* received = nullptr;
    if (xQueueReceive(results_, &received, 0) != pdTRUE) return false;
    result = std::move(*received);
    delete received;
    return true;
}

void FreeRtosWorker::taskEntry(void* self) { static_cast<FreeRtosWorker*>(self)->run(); }

void FreeRtosWorker::run() {
    while (true) {
        app::Command* command = nullptr;
        if (xQueueReceive(commands_, &command, portMAX_DELAY) != pdTRUE) continue;
        auto* result = new app::Result(executor_(*command));
        delete command;
        xQueueSend(results_, &result, portMAX_DELAY);
    }
}

} // namespace esp_platform
