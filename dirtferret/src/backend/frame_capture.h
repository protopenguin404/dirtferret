// ============================================================================
// LEARNING TASK: Frame Capture — Backend Side of Frame Delivery
// ============================================================================
//
// PURPOSE:
//   This is NOT a new class. This file documents the changes you need to make
//   to EXISTING backend files to get pixel data flowing from CEF's OnPaint
//   through IPC to the frontend. Read this, then go modify the real files.
//
//   DELETE THIS FILE once you've made the changes — it's a guide, not code.
//
// ============================================================================
// OVERVIEW OF THE FRAME PIPELINE
// ============================================================================
//
//   CEF renders a page → OnPaint fires with pixel buffer
//     → RenderHandler calls a callback with the pixels
//       → Engine packages pixels into a Message (type=FRAME)
//         → backend_main broadcasts the Message to all connected clients
//           → Frontend receives it, hands pixels to KittyRenderer
//
//   You need to touch 4 existing files. Here's exactly what and why:
//
// ============================================================================
// FILE 1: src/backend/minimal_renderer.h
// ============================================================================
//
//   ADD to RenderHandler:
//     - A using declaration for the callback type:
//         using PaintCallback = std::function<void(const void*, int, int)>;
//       (takes: pixel buffer ptr, width, height)
//
//     - A method to set the callback:
//         void set_paint_callback(PaintCallback cb);
//
//     - A private member to store it:
//         PaintCallback paint_callback_;
//
//   INCLUDE: <functional> for std::function
//
//   WHY: RenderHandler is a CEF class — it owns OnPaint. But it shouldn't
//   know about IPC or Messages. The callback is the decoupling seam: the
//   renderer just says "here are pixels" and whoever set the callback
//   decides what to do with them.
//
//   PITFALL: OnPaint is called on CEF's UI thread. The callback must be
//   fast — don't do heavy work in it. Copying the pixel buffer into a
//   Message payload (a std::vector<uint8_t>) is fine. Encoding/compressing
//   is not (do that on the frontend side).
//
//   PITFALL: Check if the callback is set before calling it (it's a
//   std::function, test it with `if (paint_callback_) { ... }`). During
//   startup, OnPaint may fire before anyone has set a callback.
//
// ============================================================================
// FILE 2: src/backend/minimal_renderer.cc
// ============================================================================
//
//   MODIFY OnPaint():
//     - Instead of the empty stub, call paint_callback_ with the buffer,
//       width, and height.
//     - Guard with: if (paint_callback_) { paint_callback_(buffer, width, height); }
//
//   ADD set_paint_callback():
//     - Just stores the callback: paint_callback_ = std::move(cb);
//
//   CEF DETAIL: The `buffer` pointer in OnPaint is only valid for the
//   duration of the callback. You MUST copy the data if you want to keep
//   it. The callback recipient (Engine) will copy it into a Message payload.
//   Size in bytes = width * height * 4 (BGRA, 4 bytes per pixel).
//
//   CEF DETAIL: `type` parameter is PET_VIEW (full page) or PET_POPUP
//   (dropdown/popup). For now, only handle PET_VIEW. You can check with:
//     if (type != PET_VIEW) return;
//
// ============================================================================
// FILE 3: src/backend/engine.h / engine.cc
// ============================================================================
//
//   The Engine already has:
//     using FrameCallback = std::function<void(int32_t, const void*, int, int)>;
//     void set_frame_callback(FrameCallback cb) {}  // <-- stub, does nothing
//
//   MODIFY set_frame_callback():
//     - Store the callback in a private member: FrameCallback frame_callback_;
//     - Actually wire it up: in initialize() (or after the app/client are
//       created), get the RenderHandler from the client and call
//       set_paint_callback() on it, wrapping frame_callback_.
//
//   THE WIRING PROBLEM:
//     Engine has app_ → app_ has client() → client has render_handler.
//     But right now, MinimalClient creates RenderHandler internally and
//     doesn't expose it. You have two choices:
//
//     Option A: Add a render_handler() accessor to MinimalClient.
//       Then Engine can do:
//         app_->client()->render_handler()->set_paint_callback(...)
//       PITFALL: client() might be null during init (browser isn't created
//       yet in initialize() — it's created in OnContextInitialized, which
//       fires asynchronously). You'll need to set the callback at the right
//       time. One approach: pass the callback THROUGH MinimalApp to
//       MinimalClient to RenderHandler during construction.
//
//     Option B: Pass the FrameCallback into MinimalApp's constructor,
//       which passes it to MinimalClient, which passes it to RenderHandler.
//       Cleaner flow, slightly more constructor churn.
//
//     Either works. Option A is simpler for now. You'll also need to add
//     a render_handler() accessor to MinimalClient (in client.h).
//
//   THE CALLBACK ITSELF:
//     Engine's frame_callback_ signature is (int32_t buffer_id, const void*, int, int).
//     RenderHandler's paint_callback_ is (const void*, int, int).
//     Engine bridges them by wrapping: when setting the paint callback on
//     the renderer, capture the buffer_id and prepend it:
//       renderer->set_paint_callback([this, buffer_id](const void* buf, int w, int h) {
//           if (frame_callback_) frame_callback_(buffer_id, buf, w, h);
//       });
//     For now, buffer_id can be 0 (we only have one buffer).
//
// ============================================================================
// FILE 4: src/backend_main.cc
// ============================================================================
//
//   AFTER engine.initialize() and BEFORE the main loop, set the frame
//   callback on the engine:
//
//     engine.set_frame_callback(
//         [&clients](int32_t buffer_id, const void* pixels, int w, int h) {
//             // Package into a FRAME message
//             cef_terminal::Message frame_msg;
//             frame_msg.type = cef_terminal::MessageType::FRAME;
//             frame_msg.id = 0;  // frames don't need request correlation
//             frame_msg.buffer_id = buffer_id;
//             frame_msg.width = w;
//             frame_msg.height = h;
//
//             // Copy pixel data into payload
//             size_t byte_count = w * h * 4;  // BGRA = 4 bytes/pixel
//             frame_msg.payload.resize(byte_count);
//             std::memcpy(frame_msg.payload.data(), pixels, byte_count);
//
//             // Broadcast to all connected clients
//             for (auto& client : clients) {
//                 if (client->connected()) {
//                     client->send(frame_msg);
//                 }
//             }
//         });
//
//   INCLUDE: <cstring> for std::memcpy
//
//   NOTE ON SERIALIZATION:
//     For FRAME messages, we're putting raw pixels directly in the payload —
//     no serialize_*/deserialize_* needed. The Message struct already has
//     width/height/buffer_id fields that get written into the wire frame
//     header. Check that the unix_socket_transport send/receive code
//     actually writes and reads those fields for FRAME messages. If it
//     doesn't (the current serialization only handles the basic
//     [len][type][id][payload] frame), you'll need to extend the wire
//     format.
//
//     Look at unix_socket_transport.cc — specifically send() and
//     try_parse_message(). The current wire frame is:
//       [payload_len:u32][type:u8][id:u32][payload:bytes]
//
//     For FRAME messages, you need to also transmit width, height, and
//     buffer_id. Options:
//       A) Extend the wire header for FRAME type: add 3x int32 after id
//       B) Prepend them to the payload and parse them out on receive
//       C) Use the existing serialization to pack them
//
//     Option A is cleanest. In send(), if type==FRAME, write the extra
//     12 bytes. In try_parse_message(), if type==FRAME, read them.
//
// ============================================================================
// FILE 5 (modification): src/frontend_main.cc
// ============================================================================
//
//   The current frontend is a one-shot test client. You'll reshape it into
//   a loop:
//
//   1. Create a Terminal (setup raw mode, alt screen)
//   2. Create a KittyRenderer
//   3. Connect to backend
//   4. Send a buffer.navigate command
//   5. Enter a receive loop:
//      - Call transport.receive() (non-blocking)
//      - If you get a FRAME message:
//        - Extract width, height from the message
//        - Extract pixel data from message.payload
//        - Call renderer.render_frame(payload.data(), width, height)
//      - If you get other message types, handle or ignore
//      - Small sleep (usleep 16000 = ~60fps cap) to avoid spinning
//      - Check for quit condition (for now, maybe run for N seconds or
//        until stdin gets 'q' — you can add proper input later)
//   6. On exit: transport.close(), Terminal destructor restores terminal
//
//   IMPORTANT: The existing send_command/send_query helpers use
//   wait_for_message() which blocks. In the new loop-based design,
//   you'll want to send the navigate command and then NOT block —
//   instead, check for responses in the same receive loop as frames.
//   For the prototype, it's OK to keep the blocking send_command for
//   the initial navigate, then switch to the non-blocking loop for
//   frame reception.
//
// ============================================================================
// SUMMARY: FILE EDIT CHECKLIST
// ============================================================================
//
//   [ ] src/backend/minimal_renderer.h — add PaintCallback, set_paint_callback
//   [ ] src/backend/minimal_renderer.cc — call callback in OnPaint
//   [ ] src/backend/client.h — add render_handler() accessor
//   [ ] src/backend/engine.h — store FrameCallback, unwire stub
//   [ ] src/backend/engine.cc — wire paint callback through to renderer
//   [ ] src/backend_main.cc — set frame callback, broadcast to clients
//   [ ] src/ipc/unix_socket_transport.cc — extend wire format for FRAME
//   [ ] src/frontend/terminal.h + .cc — implement Terminal (raw mode, etc.)
//   [ ] src/frontend/kitty_renderer.h + .cc — implement KittyRenderer
//   [ ] src/frontend_main.cc — rewrite as loop with Terminal + KittyRenderer
//   [ ] src/CMakeLists.txt — add new frontend source files
//
// ============================================================================
#pragma once
// This file is a guide. Delete it when done.
