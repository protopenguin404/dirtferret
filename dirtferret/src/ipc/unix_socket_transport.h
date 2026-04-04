#pragma once

#include "ipc/transport.h"

#include <string>
#include <vector>

namespace cef_terminal {

// Unix domain socket implementation of Transport.
//
// Two modes:
//   Server: call listen() to bind + accept clients. Call poll_accept()
//           in your event loop to check for new connections.
//   Client: call connect() to connect to a server.
//
// After connection, both sides use send()/receive() symmetrically.
class UnixSocketTransport : public Transport {
 public:
    UnixSocketTransport() = default;

    // Wrap an already-connected fd (used internally for accepted clients).
    explicit UnixSocketTransport(int connected_fd);

    ~UnixSocketTransport() override;

    // Not copyable, movable.
    UnixSocketTransport(const UnixSocketTransport&) = delete;
    UnixSocketTransport& operator=(const UnixSocketTransport&) = delete;

    // --- Transport interface ---
    bool listen(const std::string& endpoint, ClientCallback on_client) override;
    bool connect(const std::string& endpoint) override;
    bool send(const Message& msg) override;
    std::optional<Message> receive() override;
    bool connected() const override;
    void close() override;

    // Server-specific: check for pending connections (non-blocking).
    // Fires the ClientCallback registered via listen() for each new client.
    void poll_accept();

    // Blocking receive with timeout. -1 = wait forever, 0 = non-blocking.
    std::optional<Message> wait_for_message(int timeout_ms = -1);

 private:
    int fd_ = -1;           // connected data socket
    int listen_fd_ = -1;    // listening socket (server mode)
    std::string endpoint_;   // socket path (for cleanup)
    ClientCallback on_client_;
    std::vector<uint8_t> read_buf_;

    bool write_all(const uint8_t* data, size_t len);
    bool drain_socket();
    std::optional<Message> try_parse_message();
};

}  // namespace cef_terminal
