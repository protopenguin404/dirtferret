#include "shm/frame_pool.h"

#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

FramePool::FramePool(const std::string &prefix, uint32_t width, uint32_t height)
    : prefix_(prefix), width_(width), height_(height) {
  shm_names_[0] = "/" + prefix + "-frame-0";
  shm_names_[1] = "/" + prefix + "-frame-1";
  notify_name_ = "/" + prefix + "-notify";
}

FramePool::~FramePool() { destroy(); }

bool FramePool::create() {
  size_t size = buffer_size();
  if (size == 0)
    return false;

  for (int i = 0; i < 2; ++i) {
    shm_fds_[i] =
        shm_open(shm_names_[i].c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (shm_fds_[i] < 0) {
      std::cerr << "[shm] Failed to create: " << shm_names_[i] << std::endl;
      destroy();
      return false;
    }

    if (ftruncate(shm_fds_[i], static_cast<off_t>(size)) < 0) {
      std::cerr << "[shm] Failed to resize: " << shm_names_[i] << std::endl;
      destroy();
      return false;
    }

    shm_ptrs_[i] =
        mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fds_[i], 0);
    if (shm_ptrs_[i] == MAP_FAILED) {
      shm_ptrs_[i] = nullptr;
      std::cerr << "[shm] Failed to mmap: " << shm_names_[i] << std::endl;
      destroy();
      return false;
    }

    std::memset(shm_ptrs_[i], 0, size);
  }

  notify_fd_ =
      shm_open(notify_name_.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  if (notify_fd_ < 0) {
    std::cerr << "[shm] Failed to create: " << notify_name_ << std::endl;
    destroy();
    return false;
  }

  if (ftruncate(notify_fd_, sizeof(FrameNotifyHeader)) < 0) {
    std::cerr << "[shm] Failed to resize: " << notify_name_ << std::endl;
    destroy();
    return false;
  }

  void *nptr = mmap(nullptr, sizeof(FrameNotifyHeader),
                    PROT_READ | PROT_WRITE, MAP_SHARED, notify_fd_, 0);
  if (nptr == MAP_FAILED) {
    std::cerr << "[shm] Failed to mmap: " << notify_name_ << std::endl;
    destroy();
    return false;
  }

  notify_ptr_ = static_cast<FrameNotifyHeader *>(nptr);
  std::memset(notify_ptr_, 0, sizeof(FrameNotifyHeader));

  current_read_ = 0;
  return true;
}

void FramePool::destroy() {
  size_t size = buffer_size();
  for (int i = 0; i < 2; ++i) {
    if (shm_ptrs_[i]) {
      munmap(shm_ptrs_[i], size);
      shm_ptrs_[i] = nullptr;
    }
    if (shm_fds_[i] >= 0) {
      close(shm_fds_[i]);
      shm_fds_[i] = -1;
    }
    shm_unlink(shm_names_[i].c_str());
  }

  if (notify_ptr_) {
    munmap(notify_ptr_, sizeof(FrameNotifyHeader));
    notify_ptr_ = nullptr;
  }
  if (notify_fd_ >= 0) {
    close(notify_fd_);
    notify_fd_ = -1;
  }
  shm_unlink(notify_name_.c_str());
}

bool FramePool::resize(uint32_t width, uint32_t height) {
  destroy();
  width_ = width;
  height_ = height;
  return create();
}

void *FramePool::write_buffer() {
  int write_idx = 1 - current_read_;
  return shm_ptrs_[write_idx];
}

const void *FramePool::read_buffer() const { return shm_ptrs_[current_read_]; }

void FramePool::swap() { current_read_ = 1 - current_read_; }

void FramePool::publish(const std::vector<NotifyDirtyRect>& dirty_rects) {
  swap();

  if (!notify_ptr_) return;

  notify_ptr_->width = width_;
  notify_ptr_->height = height_;
  notify_ptr_->read_slot = static_cast<uint32_t>(current_read_);
  uint32_t count = static_cast<uint32_t>(std::min(dirty_rects.size(), size_t(16)));
  notify_ptr_->dirty_count = count;
  for (uint32_t i = 0; i < count; ++i) {
    notify_ptr_->dirty_rects[i].x = dirty_rects[i].x;
    notify_ptr_->dirty_rects[i].y = dirty_rects[i].y;
    notify_ptr_->dirty_rects[i].w = dirty_rects[i].width;
    notify_ptr_->dirty_rects[i].h = dirty_rects[i].height;
  }

  __atomic_store_n(&notify_ptr_->sequence,
                   __atomic_load_n(&notify_ptr_->sequence, __ATOMIC_RELAXED) + 1,
                   __ATOMIC_RELEASE);
}

std::string FramePool::read_shm_name() const {
  return shm_names_[current_read_];
}

std::string FramePool::notify_shm_name() const {
  return notify_name_;
}

size_t FramePool::buffer_size() const {
  return static_cast<size_t>(width_) * height_ * 4;
}
