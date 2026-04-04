#include "ipc/unix_socket_transport.h"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>

namespace cef_terminal {

namespace {

// Wire format for a single message on the socket:
//   [uint32_t payload_len]  (length of everything after this field)
//   [uint8_t  type]
//   [uint32_t id]
//   [payload bytes]
constexpr size_t HEADER_SIZE = 4 + 1 + 4;  // payload_len + type + id

bool set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

uint32_t read_u32_le(const uint8_t* p) {
    return p[0]
        | (static_cast<uint32_t>(p[1]) << 8)
        | (static_cast<uint32_t>(p[2]) << 16)
        | (static_cast<uint32_t>(p[3]) << 24);
}

void write_u32_le(uint8_t* p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

}  // namespace

UnixSocketTransport::UnixSocketTransport(int connected_fd) : fd_(connected_fd) {
    set_nonblocking(fd_);
}

UnixSocketTransport::~UnixSocketTransport() {
    close();
}

bool UnixSocketTransport::listen(const std::string& endpoint, ClientCallback on_client) {
    endpoint_ = endpoint;
    on_client_ = std::move(on_client);

    listen_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        std::cerr << "[ipc] socket() failed: " << strerror(errno) << std::endl;
        return false;
    }

    // Remove stale socket file if it exists.
    ::unlink(endpoint.c_str());

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, endpoint.c_str(), sizeof(addr.sun_path) - 1);

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[ipc] bind() failed: " << strerror(errno) << std::endl;
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    if (::listen(listen_fd_, 4) < 0) {
        std::cerr << "[ipc] listen() failed: " << strerror(errno) << std::endl;
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    set_nonblocking(listen_fd_);
    std::cerr << "[ipc] Listening on " << endpoint << std::endl;
    return true;
}

bool UnixSocketTransport::connect(const std::string& endpoint) {
    endpoint_ = endpoint;

    fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd_ < 0) {
        std::cerr << "[ipc] socket() failed: " << strerror(errno) << std::endl;
        return false;
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, endpoint.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[ipc] connect() failed: " << strerror(errno) << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    set_nonblocking(fd_);
    std::cerr << "[ipc] Connected to " << endpoint << std::endl;
    return true;
}

bool UnixSocketTransport::send(const Message& msg) {
    if (fd_ < 0) return false;

    // Build frame: [payload_len][type][id][payload]
    uint32_t payload_len = 1 + 4 + static_cast<uint32_t>(msg.payload.size());
    std::vector<uint8_t> frame(4 + payload_len);

    write_u32_le(frame.data(), payload_len);
    frame[4] = static_cast<uint8_t>(msg.type);
    write_u32_le(frame.data() + 5, msg.id);

    if (!msg.payload.empty()) {
        std::memcpy(frame.data() + 9, msg.payload.data(), msg.payload.size());
    }

    return write_all(frame.data(), frame.size());
}

std::optional<Message> UnixSocketTransport::receive() {
    drain_socket();
    return try_parse_message();
}

bool UnixSocketTransport::connected() const {
    return fd_ >= 0;
}

void UnixSocketTransport::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    if (listen_fd_ >= 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
        // Clean up socket file.
        if (!endpoint_.empty()) {
            ::unlink(endpoint_.c_str());
        }
    }
}

void UnixSocketTransport::poll_accept() {
    if (listen_fd_ < 0 || !on_client_) return;

    int client_fd = ::accept(listen_fd_, nullptr, nullptr);
    if (client_fd < 0) {
        // EAGAIN/EWOULDBLOCK = no pending connections, that's fine.
        return;
    }

    std::cerr << "[ipc] Accepted client connection (fd=" << client_fd << ")" << std::endl;
    on_client_(std::make_unique<UnixSocketTransport>(client_fd));
}

std::optional<Message> UnixSocketTransport::wait_for_message(int timeout_ms) {
    // First check if we already have a complete message buffered.
    auto msg = try_parse_message();
    if (msg) return msg;

    if (fd_ < 0) return std::nullopt;

    struct pollfd pfd{};
    pfd.fd = fd_;
    pfd.events = POLLIN;

    int ret = ::poll(&pfd, 1, timeout_ms);
    if (ret <= 0) return std::nullopt;

    drain_socket();
    return try_parse_message();
}

// --- Private helpers ---

bool UnixSocketTransport::write_all(const uint8_t* data, size_t len) {
    size_t written = 0;
    while (written < len) {
        ssize_t n = ::write(fd_, data + written, len - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Wait briefly for the socket to become writable.
                struct pollfd pfd{};
                pfd.fd = fd_;
                pfd.events = POLLOUT;
                ::poll(&pfd, 1, 100);
                continue;
            }
            std::cerr << "[ipc] write failed: " << strerror(errno) << std::endl;
            return false;
        }
        written += n;
    }
    return true;
}

bool UnixSocketTransport::drain_socket() {
    if (fd_ < 0) return false;

    uint8_t tmp[4096];
    while (true) {
        ssize_t n = ::read(fd_, tmp, sizeof(tmp));
        if (n > 0) {
            read_buf_.insert(read_buf_.end(), tmp, tmp + n);
        } else if (n == 0) {
            // Peer closed connection.
            std::cerr << "[ipc] Peer disconnected." << std::endl;
            ::close(fd_);
            fd_ = -1;
            return false;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;
            std::cerr << "[ipc] read failed: " << strerror(errno) << std::endl;
            return false;
        }
    }
    return true;
}

std::optional<Message> UnixSocketTransport::try_parse_message() {
    // Need at least 4 bytes for the payload length.
    if (read_buf_.size() < 4) return std::nullopt;

    uint32_t payload_len = read_u32_le(read_buf_.data());
    uint32_t frame_size = 4 + payload_len;

    if (read_buf_.size() < frame_size) return std::nullopt;

    // Parse the message.
    Message msg;
    msg.type = static_cast<MessageType>(read_buf_[4]);
    msg.id = read_u32_le(read_buf_.data() + 5);

    size_t data_len = payload_len - 5;  // subtract type(1) + id(4)
    if (data_len > 0) {
        msg.payload.assign(read_buf_.begin() + 9,
                           read_buf_.begin() + 9 + data_len);
    }

    // Remove consumed bytes from buffer.
    read_buf_.erase(read_buf_.begin(), read_buf_.begin() + frame_size);
    return msg;
}

}  // namespace cef_terminal
