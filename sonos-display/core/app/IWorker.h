#pragma once

#include "app/BasicWorker.h"
#include "app/Commands.h"

namespace app {

// Worker for the Sonos commands.
using IWorker = BasicWorker<Command, Result>;

} // namespace app
