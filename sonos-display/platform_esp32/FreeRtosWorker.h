#pragma once

#include "app/BasicWorker.h"
#include "app/Commands.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <functional>
#include <utility>

namespace esp_platform {

// Runs commands in a FreeRTOS task, one at a time, in submission order.
// The queues carry heap-allocated command/result pointers.
template <typename CommandT, typename ResultT>
class BasicFreeRtosWorker : public app::BasicWorker<CommandT, ResultT> {
public:
    using Executor = std::function<ResultT(const CommandT&)>;

    // The network stack runs on core 0; LVGL and loop() run on core 1.
    BasicFreeRtosWorker(Executor executor, const char* taskName = "sonos_worker",
                        UBaseType_t queueLength = 16, uint32_t stackBytes = 12 * 1024)
        : executor_(std::move(executor)),
          commands_(xQueueCreate(queueLength, sizeof(CommandT*))),
          results_(xQueueCreate(queueLength, sizeof(ResultT*))) {
        xTaskCreatePinnedToCore(taskEntry, taskName, stackBytes, this, 1, nullptr, 0);
    }

    bool submit(const CommandT& command) override {
        auto* copy = new CommandT(command);
        if (xQueueSend(commands_, &copy, 0) == pdTRUE) return true;
        delete copy;
        return false;
    }

    bool poll(ResultT& result) override {
        ResultT* received = nullptr;
        if (xQueueReceive(results_, &received, 0) != pdTRUE) return false;
        result = std::move(*received);
        delete received;
        return true;
    }

private:
    static void taskEntry(void* self) { static_cast<BasicFreeRtosWorker*>(self)->run(); }

    void run() {
        while (true) {
            CommandT* command = nullptr;
            if (xQueueReceive(commands_, &command, portMAX_DELAY) != pdTRUE) continue;
            auto* result = new ResultT(executor_(*command));
            delete command;
            xQueueSend(results_, &result, portMAX_DELAY);
        }
    }

    Executor executor_;
    QueueHandle_t commands_;
    QueueHandle_t results_;
};

using FreeRtosWorker = BasicFreeRtosWorker<app::Command, app::Result>;

} // namespace esp_platform
