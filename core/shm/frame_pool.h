#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Double-buffered POSIX shared memory pool for zero-copy frame delivery.
//
// Two shm segments alternate: while the core writes the next frame to
// the "write" buffer, the TUI reads the current frame from the "read" buffer.
//
// Usage:
//   FramePool pool("dirtferret", 1920, 1080);
//   pool.create();
//
//   // In OnPaint callback:
//   void* dst = pool.write_buffer();
//   memcpy(dst, pixels, pool.buffer_size());
//   pool.swap();  // "write" becomes "read"
//
//   // TUI opens pool.read_shm_name() via shm_open + mmap
//
// YOUR IMPLEMENTATION HERE
class FramePool {
 public:
  // prefix: used to construct shm names (e.g., "/dirtferret-frame-0")
  // width, height: pixel dimensions (BGRA = 4 bytes/pixel)
  FramePool(const std::string& prefix, uint32_t width, uint32_t height);
  ~FramePool();

  // Create the shared memory segments. Returns false on failure.
  bool create();

  // Destroy the shared memory segments.
  void destroy();

  // Resize both buffers. Destroys and recreates.
  bool resize(uint32_t width, uint32_t height);

  // Pointer to the current write buffer (core writes here).
  void* write_buffer();

  // Pointer to the current read buffer (TUI reads from here).
  const void* read_buffer() const;

  // Swap read and write buffers.
  void swap();

  // The shm name of the current read buffer (for the TUI to open).
  std::string read_shm_name() const;

  // Buffer size in bytes (width * height * 4).
  size_t buffer_size() const;

  uint32_t width() const { return width_; }
  uint32_t height() const { return height_; }

 private:
  std::string prefix_;
  uint32_t width_;
  uint32_t height_;
  int current_read_ = 0;  // 0 or 1 — index of the read buffer

  // Two shm segments
  std::string shm_names_[2];
  int shm_fds_[2] = {-1, -1};
  void* shm_ptrs_[2] = {nullptr, nullptr};
};
