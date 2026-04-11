#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Dirty rectangle for publish(). Matches DirtyRect in engine.h
// but defined here to avoid frame_pool depending on engine headers.
struct NotifyDirtyRect {
  int32_t x, y;
  uint32_t width, height;
};

// Binary layout of the per-buffer notification header.
// Shared between C++ (producer) and Rust (consumer).
// Total: 280 bytes.
struct FrameNotifyHeader {
  uint64_t sequence;                   //  0: atomic, incremented after publish
  uint32_t width;                      //  8: frame width in pixels
  uint32_t height;                     // 12: frame height in pixels
  uint32_t read_slot;                  // 16: which frame slot (0 or 1) to read
  uint32_t dirty_count;               // 20: number of valid dirty rects (0..16)
  struct { int32_t x, y; uint32_t w, h; } dirty_rects[16];  // 24..280
};

static_assert(sizeof(FrameNotifyHeader) == 280, "Header layout mismatch");

// Double-buffered POSIX shared memory pool for zero-copy frame delivery.
//
// Two shm segments alternate: while the core writes the next frame to
// the "write" buffer, the TUI reads the current frame from the "read" buffer.
//
// A third shm segment holds a FrameNotifyHeader that the TUI polls
// instead of waiting for RPC notifications.
//
// Usage:
//   FramePool pool("dirtferret-1", 1920, 1080);
//   pool.create();
//
//   // In OnPaint callback:
//   void* dst = pool.write_buffer();
//   memcpy(dst, pixels, pool.buffer_size());
//   pool.publish(dirty_rects);  // swaps + writes header + increments sequence
//
//   // TUI polls notify_shm_name() header for new frames
class FramePool {
 public:
  FramePool(const std::string& prefix, uint32_t width, uint32_t height);
  ~FramePool();

  bool create();
  void destroy();
  bool resize(uint32_t width, uint32_t height);

  void* write_buffer();
  const void* read_buffer() const;

  // Swap + write notify header + atomic increment sequence.
  void publish(const std::vector<NotifyDirtyRect>& dirty_rects);

  std::string read_shm_name() const;
  std::string notify_shm_name() const;

  size_t buffer_size() const;
  uint32_t width() const { return width_; }
  uint32_t height() const { return height_; }

 private:
  void swap();

  std::string prefix_;
  uint32_t width_;
  uint32_t height_;
  int current_read_ = 0;

  std::string shm_names_[2];
  int shm_fds_[2] = {-1, -1};
  void* shm_ptrs_[2] = {nullptr, nullptr};

  std::string notify_name_;
  int notify_fd_ = -1;
  FrameNotifyHeader* notify_ptr_ = nullptr;
};
