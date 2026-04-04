#pragma once

#include "ipc/message.h"
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace cef_terminal {

// Abstract IPC transport. Implementations handle the actual byte-shuffling.
// The interface is symmetric — both server and client use the same type
// after connection is established. The listen/connect distinction is
// only for setup.
class Transport {
 public:
    virtual ~Transport() = default;

    // Server side: bind and listen on an endpoint.
    // The callback fires for each incoming client connection,
    // providing a Transport for that specific client.
    using ClientCallback = std::function<void(std::unique_ptr<Transport>)>;
    virtual bool listen(const std::string& endpoint, ClientCallback on_client) = 0;

    // Client side: connect to a listening server.
    virtual bool connect(const std::string& endpoint) = 0;

    // Send a message. Returns false on failure.
    virtual bool send(const Message& msg) = 0;

    // Non-blocking receive. Returns nullopt if no message is available.
    virtual std::optional<Message> receive() = 0;

    // Check if the connection is alive.
    virtual bool connected() const = 0;

    // Shut down this transport.
    virtual void close() = 0;
};

}  // namespace cef_terminal
