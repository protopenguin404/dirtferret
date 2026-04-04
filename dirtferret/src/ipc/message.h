#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cef_terminal {

// Message types that travel over IPC between backend and frontend.
enum class MessageType : uint8_t {
    COMMAND,            // a Command serialized as payload
    QUERY,              // a Query serialized as payload
    COMMAND_RESULT,     // response to a command
    QUERY_RESULT,       // response to a query
    FRAME,              // pixel buffer (the hot path)
};

// A message is the unit of IPC communication.
// For commands/queries, payload carries the serialized data.
// For frames, payload carries pixel data (or a shm reference,
// depending on the transport).
struct Message {
    MessageType type;
    uint32_t id;            // correlates requests with responses
    std::vector<uint8_t> payload;

    // Frame-specific metadata (only meaningful when type == FRAME)
    int32_t width = 0;
    int32_t height = 0;
    int32_t buffer_id = 0;  // which buffer this frame belongs to
};

}  // namespace cef_terminal
