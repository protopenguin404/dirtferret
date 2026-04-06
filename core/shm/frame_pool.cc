#include "shm/frame_pool.h"

#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

FramePool::FramePool(const std::string& prefix, uint32_t width, uint32_t height)
    : prefix_(prefix), width_(width), height_(height) {
    shm_names_[0] = "/" + prefix + "-frame-0";
    shm_names_[1] = "/" + prefix + "-frame-1";
}

FramePool::~FramePool() {
    destroy();
}

bool FramePool::create() {
    size_t size = buffer_size();
    if (size == 0) return false;

    for (int i = 0; i < 2; ++i) {
        shm_fds_[i] = shm_open(shm_names_[i].c_str(),
                                O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
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

        shm_ptrs_[i] = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                             MAP_SHARED, shm_fds_[i], 0);
        if (shm_ptrs_[i] == MAP_FAILED) {
            shm_ptrs_[i] = nullptr;
            std::cerr << "[shm] Failed to mmap: " << shm_names_[i] << std::endl;
            destroy();
            return false;
        }

        // Zero-fill
        std::memset(shm_ptrs_[i], 0, size);
    }

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
}

bool FramePool::resize(uint32_t width, uint32_t height) {
    destroy();
    width_ = width;
    height_ = height;
    return create();
}

void* FramePool::write_buffer() {
    int write_idx = 1 - current_read_;
    return shm_ptrs_[write_idx];
}

const void* FramePool::read_buffer() const {
    return shm_ptrs_[current_read_];
}

void FramePool::swap() {
    current_read_ = 1 - current_read_;
}

std::string FramePool::read_shm_name() const {
    return shm_names_[current_read_];
}

size_t FramePool::buffer_size() const {
    return static_cast<size_t>(width_) * height_ * 4;  // BGRA = 4 bytes/pixel
}
