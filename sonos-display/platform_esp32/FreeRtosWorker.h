#pragma once

#include "app/IWorker.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <functional>

namespace esp_platform {

// Runs commands in a FreeRTOS task, one at a time, in submission order.
// The queues carry heap-allocated Command/Result pointers.
class FreeRtosWorker : public app::IWorker {
public:
    using Executor = std::function<app::Result(const app::Command&)>;

    FreeRtosWorker(Executor executor, UBaseType_t queueLength = 16, uint32_t stackBytes = 12 * 1024);

    bool submit(const app::Command& command) override;
    bool poll(app::Result& result) override;

private:
    static void taskEntry(void* self);
    void run();

    Executor executor_;
    QueueHandle_t commands_;
    QueueHandle_t results_;
};

} // namespace esp_platform
