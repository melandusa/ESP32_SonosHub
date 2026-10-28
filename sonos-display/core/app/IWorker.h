#pragma once

#include "app/Commands.h"

namespace app {

// Executes commands off the UI thread (Linux: std::thread, ESP32: FreeRTOS task).
// Both methods are called from the UI thread only and must not block.
class IWorker {
public:
    virtual ~IWorker() = default;
    // Returns false if the queue is full; the command is dropped then.
    virtual bool submit(const Command& command) = 0;
    // Takes the next finished result, false if none is ready.
    virtual bool poll(Result& result) = 0;
};

} // namespace app
