// Tests for the shared memory frame pool.
// Validates create/destroy, write/read, swap, and resize.

#include <gtest/gtest.h>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "shm/frame_pool.h"

class FramePoolTest : public ::testing::Test {
 protected:
  void TearDown() override {
      // Clean up any leaked shm segments
      shm_unlink("/test-frame-0");
      shm_unlink("/test-frame-1");
  }
};

TEST_F(FramePoolTest, CreateAndDestroy) {
    FramePool pool("test", 100, 100);
    ASSERT_TRUE(pool.create());

    // Verify shm segments exist
    int fd = shm_open("/test-frame-0", O_RDONLY, 0);
    EXPECT_GE(fd, 0);
    if (fd >= 0) close(fd);

    fd = shm_open("/test-frame-1", O_RDONLY, 0);
    EXPECT_GE(fd, 0);
    if (fd >= 0) close(fd);

    pool.destroy();

    // Verify shm segments are gone
    fd = shm_open("/test-frame-0", O_RDONLY, 0);
    EXPECT_LT(fd, 0);
    if (fd >= 0) close(fd);
}

TEST_F(FramePoolTest, BufferSize) {
    FramePool pool("test", 1920, 1080);
    // BGRA = 4 bytes per pixel
    EXPECT_EQ(pool.buffer_size(), 1920u * 1080u * 4u);
}

TEST_F(FramePoolTest, WriteAndRead) {
    FramePool pool("test", 2, 2);  // 2x2 = 16 bytes
    ASSERT_TRUE(pool.create());

    // Write BGRA pixels to write buffer
    uint8_t pixels[] = {
        0, 0, 255, 255,   // pixel 0: blue
        0, 255, 0, 255,   // pixel 1: green
        255, 0, 0, 255,   // pixel 2: red
        255, 255, 0, 255,  // pixel 3: yellow
    };
    void* dst = pool.write_buffer();
    ASSERT_NE(dst, nullptr);
    std::memcpy(dst, pixels, sizeof(pixels));

    // Before swap, read buffer should be zeroed (initial state)
    const uint8_t* read = static_cast<const uint8_t*>(pool.read_buffer());
    EXPECT_EQ(read[0], 0);

    // Swap: write becomes read
    pool.swap();

    // Now read buffer should have our pixels
    read = static_cast<const uint8_t*>(pool.read_buffer());
    EXPECT_EQ(read[0], 0);       // B of blue pixel
    EXPECT_EQ(read[2], 255);     // R of blue pixel
    EXPECT_EQ(read[4], 0);       // B of green pixel
    EXPECT_EQ(read[5], 255);     // G of green pixel
}

TEST_F(FramePoolTest, DoubleBufferIndependence) {
    FramePool pool("test", 1, 1);  // 1x1 = 4 bytes
    ASSERT_TRUE(pool.create());

    // Write to write buffer
    uint8_t pixel1[] = {1, 2, 3, 4};
    std::memcpy(pool.write_buffer(), pixel1, 4);
    pool.swap();

    // Write different data to the new write buffer
    uint8_t pixel2[] = {5, 6, 7, 8};
    std::memcpy(pool.write_buffer(), pixel2, 4);

    // Read buffer should still have pixel1
    const uint8_t* read = static_cast<const uint8_t*>(pool.read_buffer());
    EXPECT_EQ(read[0], 1);
    EXPECT_EQ(read[1], 2);

    // Swap again — now read should have pixel2
    pool.swap();
    read = static_cast<const uint8_t*>(pool.read_buffer());
    EXPECT_EQ(read[0], 5);
    EXPECT_EQ(read[1], 6);
}

TEST_F(FramePoolTest, ShmNameAlternates) {
    FramePool pool("test", 1, 1);
    ASSERT_TRUE(pool.create());

    std::string name1 = pool.read_shm_name();
    pool.swap();
    std::string name2 = pool.read_shm_name();

    EXPECT_NE(name1, name2);

    pool.swap();
    EXPECT_EQ(pool.read_shm_name(), name1);
}

TEST_F(FramePoolTest, Resize) {
    FramePool pool("test", 10, 10);
    ASSERT_TRUE(pool.create());
    EXPECT_EQ(pool.buffer_size(), 10u * 10u * 4u);

    ASSERT_TRUE(pool.resize(20, 20));
    EXPECT_EQ(pool.buffer_size(), 20u * 20u * 4u);
    EXPECT_EQ(pool.width(), 20u);
    EXPECT_EQ(pool.height(), 20u);

    // Write buffer should be valid after resize
    void* dst = pool.write_buffer();
    ASSERT_NE(dst, nullptr);
}

TEST_F(FramePoolTest, ExternalProcessCanRead) {
    FramePool pool("test", 2, 1);  // 8 bytes
    ASSERT_TRUE(pool.create());

    // Write known data
    uint8_t pixels[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    std::memcpy(pool.write_buffer(), pixels, 8);
    pool.swap();

    // Simulate external process: open shm by name and read
    std::string name = pool.read_shm_name();
    int fd = shm_open(name.c_str(), O_RDONLY, 0);
    ASSERT_GE(fd, 0);

    void* ext_ptr = mmap(nullptr, 8, PROT_READ, MAP_SHARED, fd, 0);
    ASSERT_NE(ext_ptr, MAP_FAILED);

    const uint8_t* ext = static_cast<const uint8_t*>(ext_ptr);
    EXPECT_EQ(ext[0], 0xDE);
    EXPECT_EQ(ext[3], 0xEF);
    EXPECT_EQ(ext[4], 0xCA);
    EXPECT_EQ(ext[7], 0xBE);

    munmap(ext_ptr, 8);
    close(fd);
}
