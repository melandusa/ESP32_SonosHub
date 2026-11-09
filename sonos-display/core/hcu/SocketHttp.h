#pragma once

#include "hcu/WebSocketClient.h"
#include "platform/IHttpClient.h"
#include "platform/IStreamSocket.h"

#include <string>

namespace hcu {

// One HTTPS POST over a platform stream (Connection: close). Used for pairing with the HCU,
// whose self-signed certificate the regular HTTP clients would reject.
platform::HttpResponse postOverSocket(platform::IStreamSocket& socket, const std::string& host, int port,
                                      const std::string& path, const HeaderList& headers,
                                      const std::string& body, int timeoutMs);

} // namespace hcu
