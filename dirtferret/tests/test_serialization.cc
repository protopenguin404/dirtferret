// ============================================================================
// TEST: IPC Serialization Round-Trips
// ============================================================================
//
// These tests verify the existing serialization code in ipc/serialization.cc.
// They should ALL PASS right now — they're a regression safety net.
//
// If any of these break after you edit serialization code, you introduced
// a bug. Fix the code, not the test.
//
// ============================================================================
#include <gtest/gtest.h>

#include "api/command.h"
#include "api/query.h"
#include "api/types.h"
#include "ipc/serialization.h"

using namespace cef_terminal;

// --- Command round-trips ---

TEST(Serialization, CommandWithStringArg) {
    Command cmd("buffer.navigate", {{"url", std::string("https://example.com")}});
    auto bytes = serialize_command(cmd);
    auto out = deserialize_command(bytes);

    EXPECT_EQ(out.name, "buffer.navigate");
    ASSERT_EQ(out.args.count("url"), 1);
    auto* url = std::get_if<std::string>(&out.args["url"]);
    ASSERT_NE(url, nullptr);
    EXPECT_EQ(*url, "https://example.com");
}

TEST(Serialization, CommandEmptyArgs) {
    Command cmd("buffer.close");
    auto bytes = serialize_command(cmd);
    auto out = deserialize_command(bytes);

    EXPECT_EQ(out.name, "buffer.close");
    EXPECT_TRUE(out.args.empty());
}

TEST(Serialization, CommandMultipleArgs) {
    Args args;
    args["url"] = std::string("https://example.com");
    args["tab"] = int64_t(3);
    args["force"] = true;
    Command cmd("buffer.navigate", std::move(args));

    auto bytes = serialize_command(cmd);
    auto out = deserialize_command(bytes);

    EXPECT_EQ(out.name, "buffer.navigate");
    EXPECT_EQ(out.args.size(), 3);

    auto* url = std::get_if<std::string>(&out.args["url"]);
    ASSERT_NE(url, nullptr);
    EXPECT_EQ(*url, "https://example.com");

    auto* tab = std::get_if<int64_t>(&out.args["tab"]);
    ASSERT_NE(tab, nullptr);
    EXPECT_EQ(*tab, 3);

    auto* force = std::get_if<bool>(&out.args["force"]);
    ASSERT_NE(force, nullptr);
    EXPECT_TRUE(*force);
}

// --- Query round-trips ---

TEST(Serialization, QueryRoundTrip) {
    Query q("buffer.get_title");
    auto bytes = serialize_query(q);
    auto out = deserialize_query(bytes);

    EXPECT_EQ(out.name, "buffer.get_title");
    EXPECT_TRUE(out.args.empty());
}

// --- Value types ---

TEST(Serialization, AllValueTypesRoundTrip) {
    // Pack every Value variant into one Args map and verify all survive.
    Args args;
    args["null_val"] = Value{std::monostate{}};
    args["bool_val"] = true;
    args["int_val"] = int64_t(-42);
    args["double_val"] = 3.14159;
    args["string_val"] = std::string("hello world");

    auto bytes = serialize_args(args);
    size_t offset = 0;
    auto out = deserialize_args(bytes.data(), bytes.size(), offset);

    EXPECT_EQ(out.size(), 5);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(out["null_val"]));
    EXPECT_EQ(std::get<bool>(out["bool_val"]), true);
    EXPECT_EQ(std::get<int64_t>(out["int_val"]), -42);
    EXPECT_DOUBLE_EQ(std::get<double>(out["double_val"]), 3.14159);
    EXPECT_EQ(std::get<std::string>(out["string_val"]), "hello world");
}

TEST(Serialization, Int64EdgeValues) {
    Args args;
    args["max"] = std::numeric_limits<int64_t>::max();
    args["min"] = std::numeric_limits<int64_t>::min();
    args["zero"] = int64_t(0);

    auto bytes = serialize_args(args);
    size_t offset = 0;
    auto out = deserialize_args(bytes.data(), bytes.size(), offset);

    EXPECT_EQ(std::get<int64_t>(out["max"]), std::numeric_limits<int64_t>::max());
    EXPECT_EQ(std::get<int64_t>(out["min"]), std::numeric_limits<int64_t>::min());
    EXPECT_EQ(std::get<int64_t>(out["zero"]), 0);
}

// --- Result types ---

TEST(Serialization, CommandResultSuccess) {
    auto result = CommandResult::success();
    auto bytes = serialize_command_result(result);
    auto out = deserialize_command_result(bytes);

    EXPECT_TRUE(out.ok);
    EXPECT_TRUE(out.error.empty());
}

TEST(Serialization, CommandResultFailure) {
    auto result = CommandResult::failure("no browser available");
    auto bytes = serialize_command_result(result);
    auto out = deserialize_command_result(bytes);

    EXPECT_FALSE(out.ok);
    EXPECT_EQ(out.error, "no browser available");
}

TEST(Serialization, QueryResultSuccessWithData) {
    Args data;
    data["title"] = std::string("Wikipedia");
    data["loading"] = false;
    auto result = QueryResult::success(std::move(data));

    auto bytes = serialize_query_result(result);
    auto out = deserialize_query_result(bytes);

    EXPECT_TRUE(out.ok);
    EXPECT_EQ(out.data.size(), 2);
    EXPECT_EQ(std::get<std::string>(out.data["title"]), "Wikipedia");
    EXPECT_EQ(std::get<bool>(out.data["loading"]), false);
}

TEST(Serialization, QueryResultFailure) {
    auto result = QueryResult::failure("not found");
    auto bytes = serialize_query_result(result);
    auto out = deserialize_query_result(bytes);

    EXPECT_FALSE(out.ok);
    EXPECT_EQ(out.error, "not found");
    EXPECT_TRUE(out.data.empty());
}

// --- Robustness ---

TEST(Serialization, EmptyStringRoundTrip) {
    Command cmd("", {{"", std::string("")}});
    auto bytes = serialize_command(cmd);
    auto out = deserialize_command(bytes);

    EXPECT_EQ(out.name, "");
    auto* val = std::get_if<std::string>(&out.args[""]);
    ASSERT_NE(val, nullptr);
    EXPECT_EQ(*val, "");
}

TEST(Serialization, LargeStringRoundTrip) {
    // 10KB string — exercises the length-prefix for non-trivial sizes.
    std::string big(10240, 'x');
    Command cmd("test", {{"data", big}});
    auto bytes = serialize_command(cmd);
    auto out = deserialize_command(bytes);

    EXPECT_EQ(std::get<std::string>(out.args["data"]), big);
}
