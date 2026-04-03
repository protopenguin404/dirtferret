#include "api/dispatcher.h"
#include "api/exports.h"
#include <gtest/gtest.h>

using namespace cef_terminal;

// Helper: build a minimal manifest with one command, one query, one event,
// one property, and one reference.
static ExportManifest make_test_manifest() {
    ExportManifest m;
    m.ns = "test";
    m.description = "A test component.";

    m.commands.push_back({
        "do_thing",
        "Does a thing.",
        {{"value", "string", true, "The value to use."}},
        [](const Command& cmd) -> CommandResult {
            auto it = cmd.args.find("value");
            if (it == cmd.args.end()) {
                return CommandResult::failure("missing 'value'");
            }
            return CommandResult::success();
        }
    });

    m.queries.push_back({
        "get_thing",
        "Gets the thing.",
        {},
        {{"result", "string", true, "The result."}},
        [](const Query&) -> QueryResult {
            Args data;
            data["result"] = std::string("hello");
            return QueryResult::success(std::move(data));
        }
    });

    m.events.push_back({
        "thing_happened",
        "Fires when the thing happens.",
        {{"detail", "string", true, "What happened."}}
    });

    m.properties.push_back({
        "score",
        "The current score.",
        "int",
        true,  // writable
        [](const Query&) -> QueryResult {
            Args data;
            data["value"] = int64_t{42};
            return QueryResult::success(std::move(data));
        },
        [](const Command&) -> CommandResult {
            return CommandResult::success();
        }
    });

    m.references.push_back({
        "parent",
        "other",
        "Reference to the other namespace."
    });

    return m;
}

// --- Registration and dispatch ---

TEST(Exports, CommandRegisteredWithNamespace) {
    Dispatcher d;
    d.register_exports(make_test_manifest());
    EXPECT_TRUE(d.has_command("test.do_thing"));
}

TEST(Exports, QueryRegisteredWithNamespace) {
    Dispatcher d;
    d.register_exports(make_test_manifest());
    EXPECT_TRUE(d.has_query("test.get_thing"));
}

TEST(Exports, CommandDispatchWorks) {
    Dispatcher d;
    d.register_exports(make_test_manifest());

    Command cmd;
    cmd.name = "test.do_thing";
    cmd.args["value"] = std::string("hi");
    auto result = d.dispatch(cmd);
    EXPECT_TRUE(result.ok);
}

TEST(Exports, CommandDispatchValidatesArgs) {
    Dispatcher d;
    d.register_exports(make_test_manifest());

    Command cmd;
    cmd.name = "test.do_thing";
    // No "value" arg — handler should fail.
    auto result = d.dispatch(cmd);
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("missing"), std::string::npos);
}

TEST(Exports, QueryDispatchWorks) {
    Dispatcher d;
    d.register_exports(make_test_manifest());

    Query q;
    q.name = "test.get_thing";
    auto result = d.dispatch(q);
    EXPECT_TRUE(result.ok);
    auto it = result.data.find("result");
    ASSERT_NE(it, result.data.end());
    EXPECT_EQ(std::get<std::string>(it->second), "hello");
}

// --- Properties auto-generate get/set ---

TEST(Exports, PropertyGetterRegistered) {
    Dispatcher d;
    d.register_exports(make_test_manifest());
    EXPECT_TRUE(d.has_query("test.get_score"));
}

TEST(Exports, PropertySetterRegistered) {
    Dispatcher d;
    d.register_exports(make_test_manifest());
    EXPECT_TRUE(d.has_command("test.set_score"));
}

TEST(Exports, PropertyGetterReturnsValue) {
    Dispatcher d;
    d.register_exports(make_test_manifest());

    Query q;
    q.name = "test.get_score";
    auto result = d.dispatch(q);
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(std::get<int64_t>(result.data["value"]), 42);
}

TEST(Exports, ReadOnlyPropertyHasNoSetter) {
    ExportManifest m;
    m.ns = "ro";
    m.description = "Read-only test.";
    m.properties.push_back({
        "name",
        "A read-only property.",
        "string",
        false,  // not writable
        [](const Query&) -> QueryResult {
            Args data;
            data["value"] = std::string("immutable");
            return QueryResult::success(std::move(data));
        },
        nullptr
    });

    Dispatcher d;
    d.register_exports(std::move(m));
    EXPECT_TRUE(d.has_query("ro.get_name"));
    EXPECT_FALSE(d.has_command("ro.set_name"));
}

// --- Multiple manifests ---

TEST(Exports, MultipleNamespacesCoexist) {
    ExportManifest a;
    a.ns = "alpha";
    a.description = "Alpha component.";
    a.commands.push_back({
        "go", "Go.", {}, [](const Command&) { return CommandResult::success(); }
    });

    ExportManifest b;
    b.ns = "beta";
    b.description = "Beta component.";
    b.commands.push_back({
        "go", "Go.", {}, [](const Command&) { return CommandResult::success(); }
    });

    Dispatcher d;
    d.register_exports(std::move(a));
    d.register_exports(std::move(b));

    EXPECT_TRUE(d.has_command("alpha.go"));
    EXPECT_TRUE(d.has_command("beta.go"));
}

// --- Introspection ---

TEST(Exports, RegistryListReturnsNamespaces) {
    Dispatcher d;
    d.register_exports(make_test_manifest());

    Query q;
    q.name = "registry.list";
    auto result = d.dispatch(q);
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(std::get<int64_t>(result.data["count"]), 1);
    EXPECT_EQ(std::get<std::string>(result.data["0.ns"]), "test");
}

TEST(Exports, RegistryDescribeReturnsDetails) {
    Dispatcher d;
    d.register_exports(make_test_manifest());

    Query q;
    q.name = "registry.describe";
    q.args["ns"] = std::string("test");
    auto result = d.dispatch(q);
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(std::get<int64_t>(result.data["command_count"]), 1);
    EXPECT_EQ(std::get<int64_t>(result.data["query_count"]), 1);
    EXPECT_EQ(std::get<int64_t>(result.data["event_count"]), 1);
    EXPECT_EQ(std::get<int64_t>(result.data["property_count"]), 1);
    EXPECT_EQ(std::get<int64_t>(result.data["reference_count"]), 1);
    EXPECT_EQ(std::get<std::string>(result.data["command.0.name"]), "do_thing");
}

TEST(Exports, RegistryDescribeUnknownNamespace) {
    Dispatcher d;
    d.register_exports(make_test_manifest());

    Query q;
    q.name = "registry.describe";
    q.args["ns"] = std::string("nonexistent");
    auto result = d.dispatch(q);
    EXPECT_FALSE(result.ok);
}

// --- Manifest lookup ---

TEST(Exports, FindManifestByNamespace) {
    Dispatcher d;
    d.register_exports(make_test_manifest());

    auto* m = d.find_manifest("test");
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->ns, "test");
    EXPECT_EQ(m->commands.size(), 1u);
}

TEST(Exports, FindManifestReturnsNullForUnknown) {
    Dispatcher d;
    EXPECT_EQ(d.find_manifest("nope"), nullptr);
}

// --- Legacy registration still works ---

TEST(Exports, LegacyRegistrationCoexists) {
    Dispatcher d;
    d.register_exports(make_test_manifest());

    // Register a command the old way.
    d.register_command("legacy.ping", [](const Command&) {
        return CommandResult::success();
    });

    EXPECT_TRUE(d.has_command("test.do_thing"));
    EXPECT_TRUE(d.has_command("legacy.ping"));
}
