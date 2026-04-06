// Tests for Cap'n Proto schema definitions.
// Validates that schemas compile, messages round-trip correctly,
// and generated code matches the API we expect.

#include <gtest/gtest.h>
#include <capnp/message.h>
#include <capnp/serialize.h>

#include "schema/types.capnp.h"
#include "schema/core.capnp.h"

// --- BufferInfo round-trip ---

TEST(SchemaTest, BufferInfoRoundTrip) {
    // Build
    capnp::MallocMessageBuilder builder;
    auto info = builder.initRoot<BufferInfo>();
    info.setId(42);
    info.setUrl("https://example.com");
    info.setTitle("Example");
    info.setLoading(true);
    info.setCanGoBack(false);
    info.setCanGoForward(true);
    info.setLoadProgress(0.75);

    // Serialize to bytes
    auto words = capnp::messageToFlatArray(builder);
    auto bytes = words.asBytes();
    ASSERT_GT(bytes.size(), 0u);

    // Deserialize
    auto wordArray = kj::ArrayPtr<const capnp::word>(
        reinterpret_cast<const capnp::word*>(bytes.begin()),
        bytes.size() / sizeof(capnp::word));
    capnp::FlatArrayMessageReader reader(wordArray);
    auto readInfo = reader.getRoot<BufferInfo>();

    // Verify
    EXPECT_EQ(readInfo.getId(), 42);
    EXPECT_STREQ(readInfo.getUrl().cStr(), "https://example.com");
    EXPECT_STREQ(readInfo.getTitle().cStr(), "Example");
    EXPECT_TRUE(readInfo.getLoading());
    EXPECT_FALSE(readInfo.getCanGoBack());
    EXPECT_TRUE(readInfo.getCanGoForward());
    EXPECT_DOUBLE_EQ(readInfo.getLoadProgress(), 0.75);
}

// --- KeyEvent round-trip ---

TEST(SchemaTest, KeyEventRoundTrip) {
    capnp::MallocMessageBuilder builder;
    auto event = builder.initRoot<KeyEvent>();
    event.setType(KeyEventType::CHAR);
    event.setKeyCode(65);       // 'A'
    event.setCharacter(97);     // 'a'
    event.setModifiers(0);

    auto words = capnp::messageToFlatArray(builder);
    auto bytes = words.asBytes();

    auto wordArray = kj::ArrayPtr<const capnp::word>(
        reinterpret_cast<const capnp::word*>(bytes.begin()),
        bytes.size() / sizeof(capnp::word));
    capnp::FlatArrayMessageReader reader(wordArray);
    auto readEvent = reader.getRoot<KeyEvent>();

    EXPECT_EQ(readEvent.getType(), KeyEventType::CHAR);
    EXPECT_EQ(readEvent.getKeyCode(), 65u);
    EXPECT_EQ(readEvent.getCharacter(), 97u);
    EXPECT_EQ(readEvent.getModifiers(), 0u);
}

// --- MouseEvent round-trip ---

TEST(SchemaTest, MouseEventRoundTrip) {
    capnp::MallocMessageBuilder builder;
    auto event = builder.initRoot<MouseEvent>();
    event.setType(MouseEventType::DOWN);
    event.setX(100);
    event.setY(200);
    event.setButton(MouseButton::LEFT);
    event.setModifiers(2);  // CTRL

    auto words = capnp::messageToFlatArray(builder);
    auto bytes = words.asBytes();

    auto wordArray = kj::ArrayPtr<const capnp::word>(
        reinterpret_cast<const capnp::word*>(bytes.begin()),
        bytes.size() / sizeof(capnp::word));
    capnp::FlatArrayMessageReader reader(wordArray);
    auto readEvent = reader.getRoot<MouseEvent>();

    EXPECT_EQ(readEvent.getType(), MouseEventType::DOWN);
    EXPECT_EQ(readEvent.getX(), 100);
    EXPECT_EQ(readEvent.getY(), 200);
    EXPECT_EQ(readEvent.getButton(), MouseButton::LEFT);
    EXPECT_EQ(readEvent.getModifiers(), 2u);
}

// --- Rect round-trip ---

TEST(SchemaTest, RectRoundTrip) {
    capnp::MallocMessageBuilder builder;
    auto rect = builder.initRoot<Rect>();
    rect.setX(10);
    rect.setY(20);
    rect.setWidth(800);
    rect.setHeight(600);

    auto words = capnp::messageToFlatArray(builder);
    auto bytes = words.asBytes();

    auto wordArray = kj::ArrayPtr<const capnp::word>(
        reinterpret_cast<const capnp::word*>(bytes.begin()),
        bytes.size() / sizeof(capnp::word));
    capnp::FlatArrayMessageReader reader(wordArray);
    auto readRect = reader.getRoot<Rect>();

    EXPECT_EQ(readRect.getX(), 10);
    EXPECT_EQ(readRect.getY(), 20);
    EXPECT_EQ(readRect.getWidth(), 800u);
    EXPECT_EQ(readRect.getHeight(), 600u);
}

// --- Enum values ---

TEST(SchemaTest, PixelFormatEnum) {
    EXPECT_NE(PixelFormat::BGRA, PixelFormat::RGBA);
}

TEST(SchemaTest, CursorTypeEnum) {
    // Verify all cursor types are distinct
    EXPECT_NE(CursorType::POINTER, CursorType::IBEAM);
    EXPECT_NE(CursorType::HAND, CursorType::WAIT);
}
