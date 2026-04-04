// ============================================================================
// TEST: FRAME Message Wire Format
// ============================================================================
//
// These tests verify that FRAME messages (pixel data) survive a round-trip
// through the IPC transport's wire format.
//
// WON'T COMPILE (or will fail) until you:
//   1. Extend UnixSocketTransport::send() to include width/height/buffer_id
//      in the wire frame for FRAME-type messages.
//   2. Extend UnixSocketTransport::try_parse_message() to read them back.
//
// The current wire format is:
//   [payload_len:u32][type:u8][id:u32][payload:bytes]
//
// For FRAME messages, you need to add:
//   [payload_len:u32][type:u8][id:u32][buffer_id:i32][width:i32][height:i32][payload:bytes]
//
// That's 12 extra bytes in the header for FRAME messages only.
//
// APPROACH: These tests use a Unix socket pair (socketpair()) to test
// send/receive without needing a real server. This is a common pattern
// for testing socket-based protocols in isolation.
//
// ============================================================================
#include <gtest/gtest.h>

#include "ipc/message.h"
#include "ipc/unix_socket_transport.h"

#include <cstring>
#include <sys/socket.h>
#include <vector>

using namespace cef_terminal;

// Helper: create a connected pair of transports using socketpair().
// This gives us two transports connected to each other without needing
// listen/accept/connect. Useful for unit-testing the wire format.
static std::pair<UnixSocketTransport, UnixSocketTransport> make_pair() {
    int fds[2];
    // SOCK_STREAM = TCP-like byte stream (same as Unix domain sockets).
    int rc = ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    EXPECT_EQ(rc, 0) << "socketpair() failed";
    return {UnixSocketTransport(fds[0]), UnixSocketTransport(fds[1])};
}

// --- Existing message types should still work ---

TEST(FrameWire, CommandMessageRoundTrip) {
    auto [sender, receiver] = make_pair();

    Message msg;
    msg.type = MessageType::COMMAND;
    msg.id = 42;
    msg.payload = {0x01, 0x02, 0x03};

    ASSERT_TRUE(sender.send(msg));

    auto reply = receiver.wait_for_message(1000);
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->type, MessageType::COMMAND);
    EXPECT_EQ(reply->id, 42);
    EXPECT_EQ(reply->payload, msg.payload);
}

// --- FRAME message tests ---

TEST(FrameWire, FrameMetadataRoundTrip) {
    auto [sender, receiver] = make_pair();

    Message frame;
    frame.type = MessageType::FRAME;
    frame.id = 0;
    frame.buffer_id = 7;
    frame.width = 800;
    frame.height = 600;
    frame.payload = {};  // empty payload — just testing metadata

    ASSERT_TRUE(sender.send(frame));

    auto reply = receiver.wait_for_message(1000);
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->type, MessageType::FRAME);
    EXPECT_EQ(reply->buffer_id, 7);
    EXPECT_EQ(reply->width, 800);
    EXPECT_EQ(reply->height, 600);
}

TEST(FrameWire, FrameWithPixelData) {
    auto [sender, receiver] = make_pair();

    // Simulate a tiny 2x2 BGRA frame (16 bytes of pixel data).
    std::vector<uint8_t> pixels = {
        0xAA, 0xBB, 0xCC, 0xDD,
        0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88,
        0x99, 0x00, 0xEE, 0xFF,
    };

    Message frame;
    frame.type = MessageType::FRAME;
    frame.id = 0;
    frame.buffer_id = 1;
    frame.width = 2;
    frame.height = 2;
    frame.payload = pixels;

    ASSERT_TRUE(sender.send(frame));

    auto reply = receiver.wait_for_message(1000);
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->type, MessageType::FRAME);
    EXPECT_EQ(reply->buffer_id, 1);
    EXPECT_EQ(reply->width, 2);
    EXPECT_EQ(reply->height, 2);
    EXPECT_EQ(reply->payload, pixels);
}

TEST(FrameWire, LargeFrameRoundTrip) {
    auto [sender, receiver] = make_pair();

    // 100x100 BGRA = 40,000 bytes. Not huge, but exercises the
    // transport's ability to handle payloads larger than its internal
    // read buffer (typically 4096 bytes).
    int w = 100, h = 100;
    std::vector<uint8_t> pixels(w * h * 4);
    // Fill with a recognizable pattern.
    for (size_t i = 0; i < pixels.size(); i++) {
        pixels[i] = static_cast<uint8_t>(i & 0xFF);
    }

    Message frame;
    frame.type = MessageType::FRAME;
    frame.id = 0;
    frame.buffer_id = 0;
    frame.width = w;
    frame.height = h;
    frame.payload = pixels;

    ASSERT_TRUE(sender.send(frame));

    auto reply = receiver.wait_for_message(2000);
    ASSERT_TRUE(reply.has_value());
    EXPECT_EQ(reply->width, w);
    EXPECT_EQ(reply->height, h);
    ASSERT_EQ(reply->payload.size(), pixels.size());
    EXPECT_EQ(reply->payload, pixels);
}

TEST(FrameWire, FrameAndCommandInterleaved) {
    // Verify that FRAME and non-FRAME messages can be interleaved
    // on the same connection without corruption.
    auto [sender, receiver] = make_pair();

    // Send: COMMAND, FRAME, COMMAND
    Message cmd1;
    cmd1.type = MessageType::COMMAND;
    cmd1.id = 1;
    cmd1.payload = {0xAA};

    Message frame;
    frame.type = MessageType::FRAME;
    frame.id = 0;
    frame.buffer_id = 5;
    frame.width = 1;
    frame.height = 1;
    frame.payload = {0x00, 0x00, 0xFF, 0xFF};  // one BGRA pixel

    Message cmd2;
    cmd2.type = MessageType::COMMAND;
    cmd2.id = 2;
    cmd2.payload = {0xBB};

    ASSERT_TRUE(sender.send(cmd1));
    ASSERT_TRUE(sender.send(frame));
    ASSERT_TRUE(sender.send(cmd2));

    // Receive in order.
    auto r1 = receiver.wait_for_message(1000);
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(r1->type, MessageType::COMMAND);
    EXPECT_EQ(r1->id, 1);

    auto r2 = receiver.wait_for_message(1000);
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r2->type, MessageType::FRAME);
    EXPECT_EQ(r2->buffer_id, 5);
    EXPECT_EQ(r2->width, 1);
    EXPECT_EQ(r2->height, 1);

    auto r3 = receiver.wait_for_message(1000);
    ASSERT_TRUE(r3.has_value());
    EXPECT_EQ(r3->type, MessageType::COMMAND);
    EXPECT_EQ(r3->id, 2);
}
