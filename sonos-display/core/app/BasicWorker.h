#pragma once

namespace app {

// Executes commands off the UI thread (Linux: std::thread, ESP32: FreeRTOS task).
// Both methods are called from the UI thread only and must not block.
template <typename CommandT, typename ResultT>
class BasicWorker {
public:
    virtual ~BasicWorker() = default;
    // Returns false if the queue is full; the command is dropped then.
    virtual bool submit(const CommandT& command) = 0;
    // Takes the next finished result, false if none is ready.
    virtual bool poll(ResultT& result) = 0;
};

} // namespace app
